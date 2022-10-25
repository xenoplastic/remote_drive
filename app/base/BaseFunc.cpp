#include "BaseFunc.h"

#include <chrono>
#include <string>

#if defined(_MSC_VER) && defined(_WIN32)

static inline struct tm *localtime_r(const time_t *ptime, struct tm *result)
{
	errno = localtime_s(result, ptime);
	return result;
}

#else

#include <time.h>

#endif

std::string GetCurrentSystemTime()
{
	std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
	std::time_t time = std::chrono::system_clock::to_time_t(tp);
	struct tm buf;
	localtime_r(&time, &buf);
	char date[32];
	int cch = strftime(date, sizeof(date), "%F %T", &buf);
	return std::string(date, cch);
}

std::int64_t GetMillTimestampSinceEpoch()
{
	std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
	std::chrono::system_clock::duration epoch = tp.time_since_epoch();
	return std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
}
