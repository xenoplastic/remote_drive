#include "SoLogger.h"
#include "BaseFunc.h"

#include <iostream>
#include <thread>

using namespace std;

Logger::Logger(
        std::string level,
        const char* fileName,
        int fileNameLen,
        unsigned int line,
        const char* funcName,
        int funcLen) :
    m_loglevel(level),
    m_bufStream()
{
    m_bufStream << "[" << GetCurrentSystemTime() << "] "
        << "[thread:" << this_thread::get_id() << "] "
        << "[" << level << "] "
        << "[" << string(fileName, fileNameLen) << "] "
        << "[function:" << string(funcName, funcLen) << "] "
        << "[line:" << line << "] ";

}

Logger::~Logger()
{
    Flush();
}

template <typename T>
Logger& Logger::operator << (T const& t)
{
    m_bufStream << t;
    return *this;
}

std::stringstream& Logger::Stream()
{
    return m_bufStream;
}

void Logger::Flush()
{
    cout << m_bufStream.str() << endl;

    m_bufStream.str("");
    m_bufStream.clear();
}
