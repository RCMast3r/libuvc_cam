// Copyright 2022 Whitley Software Services
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <libuvc_cam/libuvc_cam_node.hpp>

#include <image_transport/image_transport.hpp>
#include <sensor_msgs/image_encodings.hpp>
#include <sensor_msgs/msg/image.hpp>

#include <algorithm>

#include <memory>
#include <string>
#include <iterator>

using libuvc_cam::StreamFormat;

namespace libuvc_cam
{

UvcCameraNode::UvcCameraNode(const rclcpp::NodeOptions & options)
: rclcpp::Node("uvc_camera_node", options)
{
  auto vendor_id_param = declare_parameter<std::string>("vendor_id", "0x046d");
  auto product_id_param = declare_parameter<std::string>("product_id", "0x0825");
  std::string serial_num = declare_parameter<std::string>("serial_num", "");
  std::string frame_fmt_string = declare_parameter<std::string>("frame_fmt", "UNCOMPRESSED");
  int requested_width = declare_parameter("image_width", 0);
  int requested_height = declare_parameter("image_height", 0);
  int requested_frame_rate = declare_parameter("frames_per_second", 30);
  m_frame = declare_parameter<std::string>("frame_id", "camera");

  // if (vendor_id_param.get_type() == rclcpp::PARAMETER_NOT_SET) {
  //   throw rclcpp::exceptions::InvalidParameterValueException{"vendor_id is missing."};
  // } else if (product_id_param.get_type() == rclcpp::PARAMETER_NOT_SET) {
  //   throw rclcpp::exceptions::InvalidParameterValueException{"product_id is missing."};
  // }

  StreamFormat requested_fmt = StreamFormat::ANY;

  if (frame_fmt_string.empty() ||
    frame_fmt_string == "ANY")
  {
    // do nothing
  } else if (frame_fmt_string == "UNCOMPRESSED") {
    requested_fmt = StreamFormat::UNCOMPRESSED;
  } else if (frame_fmt_string == "MJPEG") {
    requested_fmt = StreamFormat::MJPEG;
  } else {
    throw rclcpp::exceptions::InvalidParameterValueException{
            "Invalid frame_fmt provided. Valid values are ANY, UNCOMPRESSED, or MJPEG"};
  }

  m_camera = std::make_unique<UvcCamera>(
    vendor_id_param,
    product_id_param,
    serial_num);

  // Register callback
  m_camera->register_frame_callback(
    std::bind(&UvcCameraNode::frame_callback, this, std::placeholders::_1));

  if (requested_fmt == StreamFormat::ANY &&
    requested_width == 0 &&
    requested_height == 0 &&
    requested_frame_rate == 0)
  {
    // Use first-available stream
    RCLCPP_INFO(
      get_logger(), "No frame parameters specified. Using first available stream type.");

    m_camera->start_streaming();
  } else {
    // Try to find supported stream matching requested parameters
    RCLCPP_INFO(
      get_logger(), "Attempting to acquire stream with specified parameters.");

    if (m_camera->format_is_supported(
        requested_fmt,
        requested_width,
        requested_height,
        requested_frame_rate))
    {
      RCLCPP_INFO(get_logger(), "Requested stream parameters available! Connecting...");
      m_camera->start_streaming_with_format(
        requested_fmt,
        requested_width,
        requested_height,
        requested_frame_rate);
    } else {
      RCLCPP_FATAL(
        get_logger(), "Requested stream is not supported. "
        "See output below for formats supported by this camera.\n");
      m_camera->print_supported_formats();
      rclcpp::shutdown();
    }
  }

  // Create publisher
  m_image_pub = image_transport::create_publisher(this, "image_raw");
}

void UvcCameraNode::frame_callback(UvcFrame * frame)
{
  sensor_msgs::msg::Image img;
  img.header.frame_id = m_frame;
  img.height = frame->height;
  img.width = frame->width;
  img.step = frame->step;
  img.data.resize(frame->data.size());
  // std::memcpy(&(image.data[0]), frame->data, frame->data_bytes);
  // std::copy(frame->data.begin(), frame->data.end(), std::back_inserter(img.data));
  for(std::size_t i =0; i< frame->data.size(); i++)
  {
    img.data[i] = frame->data[i];
  }

  switch (frame->frame_format) {
    case UvcFrameFormat::YUYV:
      img.encoding = sensor_msgs::image_encodings::YUV422_YUY2;
      break;
    case UvcFrameFormat::UYVY:
      img.encoding = sensor_msgs::image_encodings::YUV422;
      break;
    case UvcFrameFormat::RGB:
      img.encoding = sensor_msgs::image_encodings::RGB8;
      break;
    case UvcFrameFormat::BGR:
      img.encoding = sensor_msgs::image_encodings::BGR8;
      break;
    case UvcFrameFormat::MJPEG:
      // Can't decode this format.
      break;
    case UvcFrameFormat::H264:
      // Can't decode this format.
      break;
    case UvcFrameFormat::GRAY8:
      img.encoding = sensor_msgs::image_encodings::MONO8;
      break;
    case UvcFrameFormat::GRAY16:
      img.encoding = sensor_msgs::image_encodings::MONO16;
      break;
    case UvcFrameFormat::BY8:
      // Not supported
      break;
    case UvcFrameFormat::BA81:
    case UvcFrameFormat::SBGGR8:
      img.encoding = sensor_msgs::image_encodings::BAYER_BGGR8;
      break;
    case UvcFrameFormat::SGRBG8:
      img.encoding = sensor_msgs::image_encodings::BAYER_GRBG8;
      break;
    case UvcFrameFormat::SGBRG8:
      img.encoding = sensor_msgs::image_encodings::BAYER_GBRG8;
      break;
    case UvcFrameFormat::SRGGB8:
      img.encoding = sensor_msgs::image_encodings::BAYER_RGGB8;
      break;
    case UvcFrameFormat::NV12:
      // Not supported
      break;
  }
  // img.data = 
  m_image_pub.publish(img);
}

}  // namespace libuvc_cam

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(libuvc_cam::UvcCameraNode)
