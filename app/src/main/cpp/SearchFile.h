//
// Created by Administrator on 2019/9/19.
//

#ifndef TESTPLAYER_SEARCHFILE_H
#define TESTPLAYER_SEARCHFILE_H

#include <string.h>
#include <malloc.h>
#include <dirent.h>
#include <sys/stat.h>
#include <jni.h>

void scan_file(JNIEnv *env,jobject dnPlayer,jmethodID updateFile, const char* path);

#endif //TESTPLAYER_SEARCHFILE_H
