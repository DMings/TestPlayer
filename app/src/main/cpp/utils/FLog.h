//
// Created by Administrator on 2017/12/14 0014.
//

#ifndef FFMPEGPLAYER_DLOG_H
#define FFMPEGPLAYER_DLOG_H

#include <android/log.h>

#define TAG "DMPP"

#define DEBUG 1

#ifdef DEBUG
#define LOGV(FORMAT, ...) __android_log_print(ANDROID_LOG_VERBOSE,TAG,FORMAT,##__VA_ARGS__)
#define LOGD(FORMAT, ...) __android_log_print(ANDROID_LOG_DEBUG,TAG,FORMAT,##__VA_ARGS__)
#define LOGI(FORMAT, ...) __android_log_print(ANDROID_LOG_INFO,TAG,FORMAT,##__VA_ARGS__)
#define LOGW(FORMAT, ...) __android_log_print(ANDROID_LOG_WARN,TAG,FORMAT,##__VA_ARGS__)
#define LOGE(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR,TAG,FORMAT,##__VA_ARGS__)
#else
#define LOGI(FORMAT,...)
#define LOGE(FORMAT,...)
#endif

#endif //FFMPEGPLAYER_DLOG_H
