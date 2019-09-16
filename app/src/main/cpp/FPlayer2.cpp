//
// Created by Administrator on 2019/9/12.
//

#include "FPlayer2.h"


int startPlayer(const char *src_filename, ANativeWindow *window) {
    AVPacket pkt;
    Audio audio;
    Video video;

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
    // ->>
    int video_ret = video.open_stream(window);
//    int video_ret = 1;
    int audio_ret = audio.open_stream();
    if (video_ret >= 0 || audio_ret >= 0) {
        av_init_packet(&pkt);
        pkt.data = NULL;
        pkt.size = 0;
        while (av_read_frame(fmt_ctx, &pkt) >= 0) {
            seek_frame_if_need();
            decode_packet(&pkt, false);
        }
        LOGI("flush cached frames.");
        decode_packet(&pkt, true);
        LOGI("Demuxing succeeded.");
    } else {
        LOGE("Could not find audio or video stream in the input, aborting");
    }
    //
    audio.release();
    video.release();
    avformat_close_input(&fmt_ctx);
    pthread_cond_destroy(&c_cond);
    pthread_mutex_destroy(&c_mutex);
    return 0;
}
