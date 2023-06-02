//
// Created by odm on 2023/3/8.
//

#ifndef LDSTREAMS_TIME_UTILS_H
#define LDSTREAMS_TIME_UTILS_H

#include "FLog.h"

#define DEBUG2 0

#ifdef DEBUG2

#include <ctime>
#include <chrono>

#define __TSC__(tag) auto time_##tag##_start = std::chrono::high_resolution_clock::now()
#define __TEC__(tag) auto time_##tag##_end = std::chrono::high_resolution_clock::now();\
        std::chrono::duration<double> time_##tag##_elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(time_##tag##_end - time_##tag##_start); \
        float costTime = time_##tag##_elapsed.count() * 1000; \
        LOGD(#tag " time: %.3f ms", costTime)
#define __TEC_LIMIT__(tag, limit) auto time_##tag##_end = std::chrono::high_resolution_clock::now();\
        std::chrono::duration<double> time_##tag##_elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(time_##tag##_end - time_##tag##_start); \
        float costTime = time_##tag##_elapsed.count() * 1000; \
        if(costTime > limit){ \
        LOGD(#tag " time: %.3f ms", costTime); \
        }
#else
#define __TSC__(tag)
#define __TEC__(tag)
#define __TEC_LIMIT__(tag, limit)
#endif // DEBUG

int64_t GetCurrentTimeMs();

int64_t GetCurrentTimeUs();

#endif //LDSTREAMS_TIME_UTILS_H
