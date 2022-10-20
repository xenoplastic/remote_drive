#include "BaseFunc.h"

#include <chrono>
#include <string>

using namespace std::chrono;
using namespace std;

std::string GetCurrentSystemTime()
{
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm* ptm = localtime(&t);
    char date[60] = { 0 };
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
        (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
        (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return date;
}

int64_t GetMillTimestampSinceEpoch()
{
    std::chrono::system_clock::time_point curTime = std::chrono::system_clock::now();
    std::chrono::system_clock::duration curTimeEpoch = curTime.time_since_epoch();
    return duration_cast<milliseconds>(curTimeEpoch).count();
}
