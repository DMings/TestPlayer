//
// Created by Administrator on 2019/9/12.
//

#include "FPlayer2.h"

Video *video = NULL;
Audio *audio = NULL;
JavaVM *native_jvm = NULL;
JNIEnv *native_env = NULL;
jobject progress_obj = NULL;
jmethodID on_progress = NULL;

void jvm_attach_fun() {
    native_env = NULL;
    native_jvm->AttachCurrentThread(&native_env, NULL);
}

void update_time_fun() {
    pthread_mutex_lock(&seek_mutex);
    if (!video_seeking && !audio_seeking) {
        native_env->CallVoidMethod(progress_obj, on_progress, ff_sec_time, ff_sec_duration);
    }
    pthread_mutex_unlock(&seek_mutex);
}

void jvm_detach_fun() {
    native_env->DeleteGlobalRef(progress_obj);
    native_jvm->DetachCurrentThread();
    progress_obj = NULL;
    native_env = NULL;
}

void update_java_time_init(JNIEnv *env, jobject onProgressListener, jmethodID onProgress) {
    progress_obj = env->NewGlobalRef(onProgressListener);
    on_progress = onProgress;
}

int start_player(const char *src_filename, ANativeWindow *window,
         JNIEnv *env, jobject onProgressListener, jmethodID onProgress) {
    int ret = 0;
    bool cr;
    if (video != NULL) {
        return -3;
    }
    update_java_time_init(env, onProgressListener, onProgress);
    UpdateTimeFun updateTimeFun;
    updateTimeFun.jvm_attach_fun = jvm_attach_fun;
    updateTimeFun.update_time_fun = update_time_fun;
    updateTimeFun.jvm_detach_fun = jvm_detach_fun;
    video = new Video(&updateTimeFun);
    audio = new Audio(&updateTimeFun);
    AVPacket *pkt = av_packet_alloc();
    LOGI("avformat_open_input %s", src_filename);
    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
        LOGE("Could not open source file %s", src_filename);
        return -1;
    }
    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        LOGE("Could not find stream information");
        return -2;
    }
    pthread_mutex_init(&c_mutex, NULL);
    pthread_cond_init(&c_cond, NULL);
    //
    ff_init();
    // ->>
    ff_sec_duration = fmt_ctx->duration / AV_TIME_BASE; //ms
    if (ff_sec_duration <= 0) {
        ff_sec_duration = 1; //ms
    }
    int video_ret = video->open_stream(window);
//    int video_ret = 1;
    int audio_ret = audio->open_stream();
//    int audio_ret = 1;
    if (video_ret >= 0 || audio_ret >= 0) {
        pkt->data = NULL;
        pkt->size = 0;
        do {
            pthread_mutex_lock(&c_mutex);
            cr = crash_error;
            pthread_mutex_unlock(&c_mutex);
            if (cr) {
                clearAllList();
                break;
            }
            seek_frame_if_need();
            ret = av_read_frame(fmt_ctx, pkt);
            decode_packet(pkt);
        } while (ret >= 0);
        LOGI("Demuxing succeeded.");
    } else {
        LOGE("Could not find audio or video stream in the input, aborting");
    }
    audio->release();
    video->release();
    delete audio;
    delete video;
    audio = NULL;
    video = NULL;
    avformat_flush(fmt_ctx);
    LOGI("audio.release()  video.release()");
    av_packet_free(&pkt);
    avformat_close_input(&fmt_ctx);
    pthread_cond_destroy(&c_cond);
    pthread_mutex_destroy(&c_mutex);
    ff_release();
    LOGI("avformat_close_input")
    return 0;
}

void seek(float percent) {
    if (video_stream_id != -1 || audio_stream_id != -1) {
        seek_frame(percent);
    }
}

void pause() {
    if (video && video_stream_id != -1) {
        video->pause();
    }
    if (audio && audio_stream_id != -1) {
        audio->pause();
    }
}

void resume() {
    if (video && video_stream_id != -1) {
        video->resume();
    }
    if (audio && audio_stream_id != -1) {
        audio->resume();
    }
}

void update_surface(ANativeWindow *window) {
    if (video && video_stream_id != -1) {
        video->update_surface(window);
    }
}

void release() {
    if (!crash_error && (video_stream_id != -1 || audio_stream_id != -1)) {
        resume();
        pthread_mutex_lock(&c_mutex);
        crash_error = true;
        pthread_mutex_unlock(&c_mutex);
    }
}

int64_t get_current_time() {
    return ff_sec_duration;
}

int64_t get_duration_time() {
    return ff_sec_duration;
}


void play_jni(JNIEnv *env, jclass type, jstring path_, jobject surface, jobject onProgressListener) {
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    const char *path = env->GetStringUTFChars(path_, NULL);
    jclass plClass = env->GetObjectClass(onProgressListener);
    jmethodID onProgress = env->GetMethodID(plClass, "onProgress", "(JJ)V");
    start_player(path, window, env, onProgressListener, onProgress);
    env->ReleaseStringUTFChars(path_, path);
}

void seek_jni(JNIEnv *env, jclass type, jfloat percent) {
    LOGE("percent: %f", percent);
    seek(percent);
}

void pause_jni(JNIEnv *env, jclass type) {
    pause();
}

void resume_jni(JNIEnv *env, jclass type) {
    resume();
}

void update_surface_jni(JNIEnv *env, jclass type, jobject surface) {
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    update_surface(window);
}

void release_jni(JNIEnv *env, jclass type) {
    release();
}

jlong get_current_time_jni(JNIEnv *env, jclass type) {
    return get_current_time();
}

jlong get_duration_time_jni(JNIEnv *env, jclass type) {
    return get_duration_time();
}

JNINativeMethod method[] = {{"play",              "(Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Object;)V", (void *) play_jni},
                            {"seek",              "(F)V",                                                      (void *) seek_jni},
                            {"pause",             "()V",                                                       (void *) pause_jni},
                            {"resume",            "()V",                                                       (void *) resume_jni},
                            {"update_surface",    "(Ljava/lang/Object;)V",                                     (void *) update_surface_jni},
                            {"release",           "()V",                                                       (void *) release_jni},
                            {"get_current_time",  "()J",                                                       (void *) get_current_time_jni},
                            {"get_duration_time", "()J",                                                       (void *) get_duration_time_jni}
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

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    native_jvm = vm;
    LOGE("JNI_OnLoad");
    JNIEnv *env;
//    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) == JNI_OK) {
//        registerNativeMethod(env);
//        LOGE("JNI_OnLoad JNI_VERSION_1_6");
//        return JNI_VERSION_1_6;
//    } // JNI_VERSION_1_4;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) == JNI_OK) {
        registerNativeMethod(env);
        LOGE("JNI_OnLoad JNI_VERSION_1_4");
        return JNI_VERSION_1_4;
    }
    return JNI_ERR;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {
    LOGE("JNI_OnUnload");
    native_jvm = NULL;
    JNIEnv *env;
//    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) == JNI_OK) {
//        unRegisterNativeMethod(env);
//    } // JNI_VERSION_1_4;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) == JNI_OK) {
        unRegisterNativeMethod(env);
    }
}

