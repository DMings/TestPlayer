//
// Created by Administrator on 2019/9/12.
//

#include "FPlayer2.h"


int startPlayer(const char *src_filename, ANativeWindow *window) {
    int ret = 0;
    AVPacket *pkt = av_packet_alloc();
    Audio audio;
    Video video;
    LOGI("avformat_open_input 111 %s %lld", src_filename, fmt_ctx);
    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
        LOGE("Could not open source file %s", src_filename);
        return -1;
    }
    LOGI("--->Could not open source file %s", src_filename);
    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        LOGE("Could not find stream information");
        return -2;
    }

    pthread_mutex_init(&c_mutex, NULL);
    pthread_cond_init(&c_cond, NULL);
    // ->>
    int video_ret = video.open_stream(window);
//    int video_ret = 1;
//    int audio_ret = audio.open_stream();
    int audio_ret = 1;
    if (video_ret >= 0 || audio_ret >= 0) {
        pkt->data = NULL;
        pkt->size = 0;
        do {
            seek_frame_if_need(pkt);
            ret = av_read_frame(fmt_ctx, pkt);
            decode_packet(pkt);
        } while (ret >= 0);
        LOGI("flush cached frames.");
        pkt->data = NULL;
        pkt->size = 0;
        av_read_frame(fmt_ctx, pkt);
        decode_packet(pkt);
        LOGI("Demuxing succeeded.");
    } else {
        LOGE("Could not find audio or video stream in the input, aborting");
    }
    //
    audio.release();
    video.release();
    LOGI("audio.release()  video.release()");
    av_packet_free(&pkt);
    avformat_close_input(&fmt_ctx);
    pthread_cond_destroy(&c_cond);
    pthread_mutex_destroy(&c_mutex);
    fmt_ctx = NULL;
    LOGI("avformat_close_input")
    return 0;
}
