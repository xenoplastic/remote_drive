/**
 * @brief lijun
 * 基础功能库
 */

#pragma once

#include <string>

std::string GetCurrentSystemTime();

//此函数主要解决clock函数linux下不计算sleep时长
int64_t GetMillTimestampSinceEpoch();
