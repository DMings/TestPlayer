//
// Created by Administrator on 2019/9/12.
//

#include "FPlayer.h"

enum PlayStatus {
    IDLE, PREPARE, PLAYING, STOPPING,
};
PlayStatus play_status = IDLE;
pthread_mutex_t play_mutex = PTHREAD_MUTEX_INITIALIZER;

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

void refresh_finish(JNIEnv *env, jobject p_obj, jmethodID progress) {
    ff_sec_time = 0;
    ff_sec_duration = 0;
    env->CallVoidMethod(p_obj, progress, -1, ff_sec_duration);
}

int start_player(const char *src_filename, ANativeWindow *window,
                 JNIEnv *env, jobject progressObj, jmethodID onProgress) {
    int ret = 0;
    bool cr;
    bool p;
    pthread_mutex_lock(&play_mutex);
    p = play_status != IDLE;
    play_status = PREPARE;
    pthread_mutex_unlock(&play_mutex);
    if (p) {
        return -3;
    }
    update_java_time_init(env, progressObj, onProgress);
    UpdateTimeFun updateTimeFun;
    updateTimeFun.jvm_attach_fun = jvm_attach_fun;
    updateTimeFun.update_time_fun = update_time_fun;
    updateTimeFun.jvm_detach_fun = jvm_detach_fun;
    video = new Video(&updateTimeFun);
    audio = new Audio(&updateTimeFun);
    AVPacket *pkt = av_packet_alloc();
    LOGI("avformat_open_input %s", src_filename);
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
        LOGE("Could not open source file %s", src_filename);
        pthread_mutex_lock(&play_mutex);
        play_status = IDLE;
        pthread_mutex_unlock(&play_mutex);
        return -1;
    }
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        LOGE("Could not find stream information");
        pthread_mutex_lock(&play_mutex);
        play_status = IDLE;
        pthread_mutex_unlock(&play_mutex);
        return -2;
    }
    pthread_mutex_init(&c_mutex, NULL);
    pthread_cond_init(&c_cond, NULL);
    //
    ff_init();
    // ->>
    ff_sec_duration = (int32_t) fmt_ctx->duration / AV_TIME_BASE; //ms
    if (ff_sec_duration <= 0) {
        ff_sec_duration = 1; //ms
    }
    int audio_ret = audio->open_stream();
//    int audio_ret = 1;
    int video_ret = video->open_stream(window, audio_ret >= 0);
//    int video_ret = 1;
    if (video_ret >= 0 || audio_ret >= 0) {
        pthread_mutex_lock(&play_mutex);
        play_status = PLAYING;
        pthread_mutex_unlock(&play_mutex);
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
            decode_packet(pkt, audio->stream_id, video->stream_id);
        } while (ret >= 0);
        LOGI("Demuxing succeeded.");
    } else {
        LOGE("Could not find audio or video stream in the input, aborting");
    }
    pthread_mutex_lock(&play_mutex);
    play_status = STOPPING;
    LOGE("release play_status: %d", play_status);
    pthread_mutex_unlock(&play_mutex);
    audio->release();
    video->release();
    delete audio;
    delete video;
    audio = NULL;
    video = NULL;
    LOGI("audio.release()  video.release()");
    refresh_finish(env, progressObj, onProgress);
    avformat_flush(fmt_ctx);
    av_packet_free(&pkt);
    avformat_close_input(&fmt_ctx);
    ff_release();
    LOGI("avformat_close_input")
    pthread_mutex_lock(&play_mutex);
    play_status = IDLE;
    pthread_mutex_unlock(&play_mutex);
    return 0;
}

void seek(float percent) {
    pthread_mutex_lock(&play_mutex);
    if (play_status == PLAYING) {
        seek_frame(percent);
    }
    pthread_mutex_unlock(&play_mutex);
}

void pause() {
    pthread_mutex_lock(&play_mutex);
    if (play_status == PLAYING) {
        if (video && video->stream_id != -1) {
            video->pause();
        }
        if (audio && audio->stream_id != -1) {
            audio->pause();
        }
    }
    pthread_mutex_unlock(&play_mutex);
}

void resume() {
    pthread_mutex_lock(&play_mutex);
    if (play_status == PLAYING) {
        if (video && video->stream_id != -1) {
            video->resume();
        }
        if (audio && audio->stream_id != -1) {
            audio->resume();
        }
    }
    pthread_mutex_unlock(&play_mutex);
}

void update_surface(ANativeWindow *window, int width, int height) {
    pthread_mutex_lock(&play_mutex);
    if (play_status == PLAYING) {
        if (video && video->stream_id != -1) {
            video->view_width = width;
            video->view_height = height;
            video->update_surface(window);
        }
    }
    pthread_mutex_unlock(&play_mutex);
}


void release() {
    pthread_mutex_lock(&play_mutex);
    LOGE("play_status: %d", play_status);
    if (play_status != IDLE && play_status != STOPPING) {
        if (!crash_error && (audio->stream_id != -1 || video->stream_id != -1)) {
            if (video && video->stream_id != -1) {
                video->resume();
            }
            if (audio && audio->stream_id != -1) {
                audio->resume();
            }
            pthread_mutex_lock(&c_mutex);
            crash_error = true;
            pthread_mutex_unlock(&c_mutex);
        }
    }
    pthread_mutex_unlock(&play_mutex);
}

int64_t get_current_time() {
    return ff_sec_duration;
}

int64_t get_duration_time() {
    return ff_sec_duration;
}

////// jni ///////////////////////////////////////////////////////////

void play_jni(JNIEnv *env, jclass type, jstring path_, jobject surface, jobject onProgressListener) {
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    const char *path = env->GetStringUTFChars(path_, NULL);
    jclass plClass = env->GetObjectClass(onProgressListener);
    jmethodID onProgress = env->GetMethodID(plClass, "onProgress", "(II)V");
    start_player(path, window, env, onProgressListener, onProgress);
    env->ReleaseStringUTFChars(path_, path);
}

void seek_jni(JNIEnv *env, jclass type, jfloat percent) {
    LOGI("percent: %f", percent);
    seek(percent);
}

void pause_jni(JNIEnv *env, jclass type) {
    pause();
}

void resume_jni(JNIEnv *env, jclass type) {
    resume();
}

void update_surface_jni(JNIEnv *env, jclass type, jobject surface, jint width, jint height) {
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    update_surface(window, width, height);
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

void scan_file_jni(JNIEnv *env, jclass type, jstring file_path_, jobject fa_obj) {
    const char *path = env->GetStringUTFChars(file_path_, 0);
    jclass fileAction = env->GetObjectClass(fa_obj);
    jmethodID updateFile = env->GetMethodID(fileAction, "update", "(Ljava/lang/String;)V");
    scan_file(env, fa_obj, updateFile, path);
    env->ReleaseStringUTFChars(file_path_, path);
}

jint get_play_state_jni(JNIEnv *env, jclass type) {
    int state;
    pthread_mutex_lock(&play_mutex);
    state = play_status;
    pthread_mutex_unlock(&play_mutex);
    return state;
}

// 动态注册需要制定具体类型名，静态又不用
//Java_com_dming_testplayer_gl_TestActivity_play(JNIEnv *env, jobject instance, jstring path_, jobject surface,jobject onProgressListener)
//"play",              "(Ljava/lang/String;Landroid/view/Surface;Lcom/dming/testplayer/OnProgressListener;)V"

JNINativeMethod method[] = {{"play",              "(Ljava/lang/String;Landroid/view/Surface;Lcom/dming/testplayer/OnProgressListener;)V", (void *) play_jni},
                            {"seek",              "(F)V",                                                                                 (void *) seek_jni},
                            {"pause",             "()V",                                                                                  (void *) pause_jni},
                            {"resume",            "()V",                                                                                  (void *) resume_jni},
                            {"update_surface",    "(Landroid/view/Surface;II)V",                                                          (void *) update_surface_jni},
                            {"release",           "()V",                                                                                  (void *) release_jni},
                            {"get_current_time",  "()J",                                                                                  (void *) get_current_time_jni},
                            {"get_duration_time", "()J",                                                                                  (void *) get_duration_time_jni},
                            {"scan_file",         "(Ljava/lang/String;Lcom/dming/testplayer/FileAction;)V",                               (void *) scan_file_jni},
                            {"get_play_state",    "()I",                                                                                  (void *) get_play_state_jni},
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
    LOGI("JNI_OnLoad");
    JNIEnv *env;
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
    native_jvm = NULL;
    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) == JNI_OK) {
        unRegisterNativeMethod(env);
    } else if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) == JNI_OK) {
        unRegisterNativeMethod(env);
    }
}

