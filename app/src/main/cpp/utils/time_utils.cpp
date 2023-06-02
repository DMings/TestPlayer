//
// Created by odm on 2023/3/24.
//

#include <sys/time.h>
#include "time_utils.h"

int64_t GetCurrentTimeMs()
{
    int64_t time;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time = (int64_t)tv.tv_sec*1000;
    time += tv.tv_usec/1000;
    return time;
}

int64_t GetCurrentTimeUs()
{
    int64_t time;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time = (int64_t)tv.tv_sec*1000'000;
    time += tv.tv_usec;
    return time;
}
