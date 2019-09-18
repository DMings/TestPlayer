//
// Created by Administrator on 2019/9/12.
//

#include "FPlayer2.h"

Video *video = NULL;
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
    if (!video_seeking) {
        native_env->CallVoidMethod(progress_obj, on_progress, ff_time, ff_duration);
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
    AVPacket *pkt = av_packet_alloc();
//    Audio audio;
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
    ff_duration = fmt_ctx->duration / 1000; //ms
    if (ff_duration <= 0) {
        ff_duration = 1000; //ms
    }
    int video_ret = video->open_stream(window);
//    int video_ret = 1;
//    int audio_ret = audio.open_stream();
    int audio_ret = 1;
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
//    audio.release();
    video->release();
    delete video;
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
}

void resume() {
    if (video && video_stream_id != -1) {
        video->resume();
    }
}

void update_surface(ANativeWindow *window) {
    if (video && video_stream_id != -1) {
        video->update_surface(window);
    }
}

void release() {
    if (!crash_error && video_stream_id != -1 || audio_stream_id != -1) {
        resume();
        pthread_mutex_lock(&c_mutex);
        crash_error = true;
        pthread_mutex_unlock(&c_mutex);
    }
}

int64_t get_current_time() {
    return ff_time;
}

int64_t get_duration_time() {
    return ff_duration;
}