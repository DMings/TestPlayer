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
#include "ntp/ntp_client.h"
#include <thread>

extern "C" {
#include <libavcodec/jni.h>
#include <libavutil/log.h>
}

std::atomic_bool active(true);

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

void Release(JNIEnv *env, jclass type, jlong ptr) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    player->Release();
    active = false;
}

jlong GetCurrentTime(JNIEnv *env, jclass type, jlong ptr) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    return player->GetCurTimeMs();
}

jlong GetVideoCacheTime(JNIEnv *env, jclass type, jlong ptr) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    return player->GetVideoCacheTimeMs();
}

jlong GetAudioCacheTime(JNIEnv *env, jclass type, jlong ptr) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    return player->GetAudioCacheTimeMs();
}

jlong GetAudioMaxCacheTime(JNIEnv *env, jclass type, jlong ptr) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    return player->GetAudioMaxCacheTimeMs();
}

jlong GetVideoNTPDelta(JNIEnv *env, jclass clazz, jlong ptr) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    return player->GetVideoNTPDelta();
}

void DeleteInstance(JNIEnv *env, jclass type, jlong ptr) {
    auto player = reinterpret_cast<FPlayer *>(ptr);
    delete player;
}

bool ntpSuccess = false;
int64_t ntpDeltaMs = 0;
std::atomic_bool ntpSync(false);

void SyncNTP(JNIEnv *env, jclass clazz, jlong ptr) {
    if (ntpSync) {
        return;
    }
    ntpSync = true;
    ntpDeltaMs = 0;
    std::thread t([] {
        int count = INT_MAX;
        xntp_cliptr_t xntp_this = X_NULL;
        xtime_vnsec_t xtm_vnsec = XTIME_INVALID_VNSEC;
        xtime_vnsec_t xtm_ltime = XTIME_INVALID_VNSEC;
        xtime_descr_t xtm_descr = {0};
        xtime_descr_t xtm_local = {0};

        for (int i = 0; i < count; ++i) {
            xntp_this = ntpcli_open();
            if (X_NULL == xntp_this) {
                LOGI("ntpcli_open() return X_NULL, errno : %d", errno);
                break;
            }
            ntpcli_config(xntp_this, "ntp.tencent.com", NTP_PORT);
            xtm_vnsec = ntpcli_req_time(xntp_this, 3000);
            if (XTMVNSEC_IS_VALID(xtm_vnsec)) {
                xtm_ltime = time_vnsec();
                xtm_descr = time_vtod(xtm_vnsec);
                xtm_local = time_vtod(xtm_ltime);

                LOGI("[%d] %s:%d : ",
                     i + 1,
                     xntp_this->xszt_host,
                     xntp_this->xut_port);
                LOGI("NTP response : [ %04d-%02d-%02d %d %02d:%02d:%02d.%03d ]",
                     xtm_descr.ctx_year,
                     xtm_descr.ctx_month,
                     xtm_descr.ctx_day,
                     xtm_descr.ctx_week,
                     xtm_descr.ctx_hour,
                     xtm_descr.ctx_minute,
                     xtm_descr.ctx_second,
                     xtm_descr.ctx_msec);

                LOGI("Local time   : [ %04d-%02d-%02d %d %02d:%02d:%02d.%03d ]",
                     xtm_local.ctx_year,
                     xtm_local.ctx_month,
                     xtm_local.ctx_day,
                     xtm_local.ctx_week,
                     xtm_local.ctx_hour,
                     xtm_local.ctx_minute,
                     xtm_local.ctx_second,
                     xtm_local.ctx_msec);

                ntpDeltaMs = ((x_int64_t) (xtm_vnsec - xtm_ltime)) / 10000LL;
                ntpSuccess = true;
                LOGI("Deviation    : %ld ms", ntpDeltaMs);
                break;
            } else {
                LOGI("[%d] %s:%d : errno = %d",
                     i + 1,
                     xntp_this->xszt_host,
                     xntp_this->xut_port,
                     errno);
            }
            if (X_NULL != xntp_this) {
                ntpcli_close(xntp_this);
                xntp_this = X_NULL;
            }
            if (!active) {
                break;
            }
            if (1 != count) {
                usleep(1000000);
            }
        }
        //======================================
        ntpSync = false;
    });
    t.detach();
}


jlong GetNTPDelta(JNIEnv *env, jclass clazz, jlong ptr) {
    return ntpDeltaMs;
}

jboolean HasNTP(JNIEnv *env, jclass clazz, jlong ptr) {
    return ntpSuccess ? JNI_TRUE : JNI_FALSE;
}

// 动态注册需要制定具体类型名，静态又不用
//Java_com_dming_testplayer_gl_TestActivity_play(JNIEnv *env, jobject instance, jstring path_, jobject surface,jobject onProgressListener)
//"play",              "(Ljava/lang/String;Landroid/view/Surface;Lcom/dming/testplayer/OnProgressListener;)V"


JNINativeMethod method[] = {
        {"newInstance",          "()J",                          (void *) NewInstance},
        {"open",                 "(JLjava/lang/String;)I",       (void *) Open},
        {"handle",               "(J)I",                         (void *) Handle},
        {"close",                "(J)V",                         (void *) Close},
        {"release",              "(J)V",                         (void *) Release},
        {"surfaceCreated",       "(JLandroid/view/Surface;)V",   (void *) SurfaceCreated},
        {"surfaceChanged",       "(JLandroid/view/Surface;II)V", (void *) SurfaceChanged},
        {"surfaceDestroyed",     "(J)V",                         (void *) SurfaceDestroyed},
        {"getAudioCacheTime",    "(J)J",                         (void *) GetAudioCacheTime},
        {"getVideoCacheTime",    "(J)J",                         (void *) GetVideoCacheTime},
        {"getAudioMaxCacheTime", "(J)J",                         (void *) GetAudioMaxCacheTime},
        {"getCurrentTime",       "(J)J",                         (void *) GetCurrentTime},
        {"syncNTP",              "(J)V",                         (void *) SyncNTP},
        {"getNTPDelta",          "(J)J",                         (void *) GetNTPDelta},
        {"getVideoNTPDelta",     "(J)J",                         (void *) GetVideoNTPDelta},
        {"hasNTP",               "(J)Z",                         (void *) HasNTP},
        {"deleteInstance",       "(J)V",                         (void *) DeleteInstance},
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