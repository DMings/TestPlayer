//
// Created by Administrator on 2019/9/12.
//

#ifndef TESTPLAYER_FFUTILS_H
#define TESTPLAYER_FFUTILS_H

#include "Audio.h"
#include "Video.h"
#include "SearchFile.h"

extern void scan_file(JNIEnv *env,jobject dnPlayer,jmethodID updateFile, const char* path);

#endif //TESTPLAYER_FFUTILS_H
