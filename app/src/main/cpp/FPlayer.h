//
// Created by Administrator on 2019/9/12.
//

#ifndef TESTPLAYER_FFUTILS_H
#define TESTPLAYER_FFUTILS_H

#include "Audio.h"
#include "Video.h"
#include "SearchFile.h"

extern int start_player(const char *src_filename, ANativeWindow *window,
        JNIEnv *env,jobject onProgressListener,jmethodID onProgress);

extern void seek(float percent);

extern int64_t get_current_time();

extern int64_t get_duration_time();

extern void pause();

extern void resume();

extern void update_surface(ANativeWindow *window);

extern void release();

extern JavaVM *native_jvm;

#endif //TESTPLAYER_FFUTILS_H
