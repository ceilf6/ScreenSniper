#pragma once
#ifndef NO_OPENCV
#include <opencv2/opencv.hpp>
#include <string>

// 返回 true 表示成功
bool embedRobustWatermark(cv::Mat &image, const std::string &text15);
// 提取到 outText（长度应为 15），返回 true 表示成功（通过 CRC 校验）
bool extractRobustWatermark(const cv::Mat &image, std::string &outText);
#endif // NO_OPENCV
