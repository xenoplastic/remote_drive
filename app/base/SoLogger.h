/**
 * @brief lijun
 * 控制台日志
 */

#pragma once

#include <sstream>
#include <string>

class Logger
{
public:
    Logger(std::string level,
           const char* fileName,
           int fileNameLen,
           unsigned int line,
           const char* funcName,
           int funcLen);

    ~Logger();

    template <typename T>
    Logger& operator << (T const& t);

    std::stringstream& Stream();

protected:
    void Flush();

private:
    std::stringstream m_bufStream;
    std::string m_loglevel;
};

#define LOG_STREAM(lv)  Logger(lv, __FILE__, sizeof(__FILE__) - 1, __LINE__, __FUNCTION__, sizeof(__FUNCTION__) - 1).Stream()
#define log_debug LOG_STREAM("debug")
#define log_info LOG_STREAM("info")
#define log_warning LOG_STREAM("warning")
#define log_error LOG_STREAM("error")
#define log_fatal LOG_STREAM("fatal")

