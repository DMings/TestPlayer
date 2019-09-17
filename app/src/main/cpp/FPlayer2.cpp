//
// Created by Administrator on 2019/9/12.
//

#include "FPlayer2.h"

Video *video = NULL;

int start_player(const char *src_filename, ANativeWindow *window, JNIEnv *env, jobject onProgressListener,
                 jmethodID onProgress) {
    int ret = 0;
    bool cr;
    AVPacket *pkt = av_packet_alloc();
    video = new Video();
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
            seek_frame_if_need(pkt);
            ret = av_read_frame(fmt_ctx, pkt);

            if (pkt->stream_index == video_stream_id) {
                double pts = (int64_t) (av_q2d(video_stream->time_base) * pkt->pts * 1000.0); // ms
//                float percent = pts /
                env->CallVoidMethod(onProgressListener, onProgress,0.8);
//        LOGE("seek_frame_if_need video>>%f",pts);
            } else if (pkt->stream_index == audio_stream_id) {
                double pts = (int64_t) (av_q2d(audio_stream->time_base) * pkt->pts * 1000.0); // ms
//        LOGE("seek_frame_if_need audio>>%f",pts);
            }

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
    resume();
    pthread_mutex_lock(&c_mutex);
    crash_error = true;
    pthread_mutex_unlock(&c_mutex);
}