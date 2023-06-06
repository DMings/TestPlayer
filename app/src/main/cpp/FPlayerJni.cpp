//
// Created by odm on 2023/6/1.
//
#include <unistd.h>
#include <jni.h>
#include <pthread.h>
#include <cstring>
#include <cstdlib>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "utils/FLog.h"
#include "FPlayer.h"

extern "C" {
#include <libavcodec/jni.h>
#include <libavutil/log.h>
}


jlong NewInstance(JNIEnv *env, jclass type) {
    auto ptr = new FPlayer();
    return reinterpret_cast<jlong>(ptr);
}

jint Open(JNIEnv *env, jclass type, jlong ptr, jstring path_) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    const char *path = env->GetStringUTFChars(path_, nullptr);
    int ret = player->Open(path);
    env->ReleaseStringUTFChars(path_, path);
    return ret;
}

int Handle(JNIEnv *env, jclass type, jlong ptr) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    return player->Handle();
}

void Close(JNIEnv *env, jclass type, jlong ptr) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    return player->Close();
}

void Pause(JNIEnv *env, jclass type, jlong ptr) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    player->Pause();
}

void Resume(JNIEnv *env, jclass type, jlong ptr) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    player->Resume();
}

void SurfaceCreated(JNIEnv *env, jclass type, jlong ptr, jobject surface) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    player->GetGLThread()->surfaceCreated(window);
}

void
SurfaceChanged(JNIEnv *env, jclass type, jlong ptr, jobject surface, jint width, jint height) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    player->GetGLThread()->surfaceChanged(window, width, height);
}

void SurfaceDestroyed(JNIEnv *env, jclass type, jlong ptr) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    player->GetGLThread()->surfaceDestroyed();
}

void Stop(JNIEnv *env, jclass type, jlong ptr) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    player->Stop();
}

jlong GetCurrentTime(JNIEnv *env, jclass type, jlong ptr) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    return player->GetCurTimeMs();
}

jlong GetAudioCacheTime(JNIEnv *env, jclass type, jlong ptr) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    return player->GetAudioCacheTimeMs();
}

void DeleteInstance(JNIEnv *env, jclass type, jlong ptr) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    delete player;
}

// 动态注册需要制定具体类型名，静态又不用
//Java_com_dming_testplayer_gl_TestActivity_play(JNIEnv *env, jobject instance, jstring path_, jobject surface,jobject onProgressListener)
//"play",              "(Ljava/lang/String;Landroid/view/Surface;Lcom/dming/testplayer/OnProgressListener;)V"


JNINativeMethod method[] = {
        {"newInstance",       "()J",                          (void *) NewInstance},
        {"open",              "(JLjava/lang/String;)I",       (void *) Open},
        {"handle",            "(J)I",                         (void *) Handle},
        {"close",             "(J)V",                         (void *) Close},
        {"pause",             "(J)V",                         (void *) Pause},
        {"resume",            "(J)V",                         (void *) Resume},
        {"stop",              "(J)V",                         (void *) Stop},
        {"surfaceCreated",    "(JLandroid/view/Surface;)V",   (void *) SurfaceCreated},
        {"surfaceChanged",    "(JLandroid/view/Surface;II)V", (void *) SurfaceChanged},
        {"surfaceDestroyed",  "(J)V",                         (void *) SurfaceDestroyed},
        {"getAudioCacheTime", "(J)J",                         (void *) GetAudioCacheTime},
        {"getCurrentTime",    "(J)J",                         (void *) GetCurrentTime},
        {"deleteInstance",    "(J)V",                         (void *) DeleteInstance},
};

jint registerNativeMethod(JNIEnv *env) {
    jclass cl = env->FindClass("com/dming/testplayer/FPlayer");
    if ((env->RegisterNatives(cl, method, sizeof(method) / sizeof(method[0]))) < 0) {
        return -1;
    }
    return 0;
}

jint unRegisterNativeMethod(JNIEnv *env) {
    jclass cl = env->FindClass("com/dming/testplayer/FPlayer");
    env->UnregisterNatives(cl);
    return 0;
}

static char buffer[1024];
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

static void syslog_print(void *ptr, int level, const char *fmt, va_list vl) {
//    lock.lock();
    pthread_mutex_lock(&log_lock);
    memset(buffer, 0, 1024);
    vsprintf(buffer, fmt, vl);
    switch (level) {
        case AV_LOG_DEBUG:
            LOGD("FFmpeg: %s", buffer);
            break;
        case AV_LOG_VERBOSE:
            LOGV("FFmpeg: %s", buffer);
            break;
        case AV_LOG_INFO:
            LOGI("FFmpeg: %s", buffer);
            break;
        case AV_LOG_WARNING:
            LOGW("FFmpeg: %s", buffer);
            break;
        case AV_LOG_ERROR:
            LOGE("FFmpeg: %s", buffer);
            break;
    }
    pthread_mutex_unlock(&log_lock);
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGI("JNI_OnLoad");
    JNIEnv *env;
    av_jni_set_java_vm(vm, nullptr);
//    av_log_set_level(AV_LOG_INFO);
//    av_log_set_callback(syslog_print);
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) == JNI_OK) {
        registerNativeMethod(env);
        return JNI_VERSION_1_6;
    } else if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) == JNI_OK) {
        registerNativeMethod(env);
        return JNI_VERSION_1_4;
    }
    return JNI_ERR;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {
    LOGI("JNI_OnUnload");
    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) == JNI_OK) {
        unRegisterNativeMethod(env);
    } else if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) == JNI_OK) {
        unRegisterNativeMethod(env);
    }
}