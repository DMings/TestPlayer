//
// Created by Administrator on 2019/9/12.
//

#ifndef TESTPLAYER_FFUTILS_H
#define TESTPLAYER_FFUTILS_H

#include "Audio2.h"
#include "Video2.h"

extern int start_player(const char *src_filename, ANativeWindow *window,JNIEnv *env,jobject onProgressListener,jmethodID onProgress);

extern void seek(float percent);

extern void pause();

extern void resume();

extern void update_surface(ANativeWindow *window);

extern void release();

#endif //TESTPLAYER_FFUTILS_H
