//
// Created by Administrator on 2017/12/14 0014.
//

#ifndef FFMPEGPLAYER_DLOG_H
#define FFMPEGPLAYER_DLOG_H

#include <android/log.h>
#define TAG "DMFF"
#define LOGI(FORMAT,...) __android_log_print(ANDROID_LOG_INFO,TAG,FORMAT,##__VA_ARGS__);
#define LOGE(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR,TAG,FORMAT,##__VA_ARGS__);

#endif //FFMPEGPLAYER_DLOG_H
