//
// Created by Administrator on 2019/9/12.
//

#include <unistd.h>
#include "FPlayer.h"

int FPlayer::start_player(const char *src_filename) {
    int ret_play = 0;
    int ret = 0;
    bool p;
    bool findKeyFrame = false;
    pthread_mutex_lock(&play_mutex);
    p = play_status != IDLE;
    play_status = PREPARE;
    pthread_mutex_unlock(&play_mutex);
    if (p) {
        return -3;
    }
    video = new Video(&glThread, &avClock);
    audio = new Audio(&avClock);
    AVPacket *pkt = av_packet_alloc();
    //
    auto protocolName = avio_find_protocol_name(src_filename);
    LOGI("protocolName: %s", protocolName);
    LOGI("avformat_open_input %s", src_filename);
    const AVInputFormat *fmt = av_find_input_format("flv");
    int64_t timeMs = av_gettime_relative() / 1000;
    AVDictionary *dic = nullptr;
    av_dict_set_int(&dic, "tcp_nodelay", 1, 0);
//    av_dict_set_int(&dic, "timeout", 3, 0);
    av_dict_set_int(&dic, "rtmp_buffer", 2000, 0);

    av_dict_set_int(&dic, "analyzeduration", 1 * AV_TIME_BASE, 0);
    av_dict_set_int(&dic, "fpsprobesize", 0, 0);

    if (avformat_open_input(&fmt_ctx, src_filename, fmt, &dic) < 0) {
        LOGE("Could not open source file %s", src_filename);
        pthread_mutex_lock(&play_mutex);
        play_status = IDLE;
        pthread_mutex_unlock(&play_mutex);
        return -1;
    }
    LOGE("avformat_open_input... cost time: %ld", (av_gettime_relative() / 1000 - timeMs));
    timeMs = av_gettime_relative() / 1000;

    AVDictionary *find_dic = nullptr;
    if (avformat_find_stream_info(fmt_ctx, &find_dic) < 0) {
        LOGE("Could not find stream information");
        pthread_mutex_lock(&play_mutex);
        play_status = IDLE;
        pthread_mutex_unlock(&play_mutex);
        return -2;
    }
    LOGE("avformat_find_stream_info... cost time: %ld", (av_gettime_relative() / 1000 - timeMs));
    audio->open_stream(fmt_ctx);
    video->open_stream(fmt_ctx);
    pthread_mutex_lock(&play_mutex);
    play_status = PLAYING;
    pthread_mutex_unlock(&play_mutex);
    do {
        ret = av_read_frame(fmt_ctx, pkt);
        if (pkt->stream_index == video->stream_id || pkt->stream_index == audio->stream_id) {
            if (pkt->stream_index == video->stream_id) {
                pkt->time_base = video->av_stream->time_base;
                if (pkt->flags == AV_PKT_FLAG_KEY) {
                    findKeyFrame = true;
                    LOGE("findKeyFrame key pkt: %d pkt->pts: %ld pts: %ld", pkt->stream_index,
                         pkt->pts,
                         av_rescale_q(pkt->pts, pkt->time_base, AV_TIME_BASE_Q));
                }
            } else {
                pkt->time_base = audio->av_stream->time_base;
            }
            if (findKeyFrame) {
                AVPacket *c_pkt = av_packet_alloc();
                av_packet_move_ref(c_pkt, pkt);
                FPacket *f_pkt = alloc_packet();
                f_pkt->avPacket = c_pkt;
                if (c_pkt->stream_index == video->stream_id) {
                    video->putAvPacket(f_pkt);
                } else {
                    audio->putAvPacket(f_pkt);
                }
            }
        }
        av_packet_unref(pkt);
    } while (ret >= 0);
    pthread_mutex_lock(&play_mutex);
    play_status = STOPPING;
    LOGE("Release play_status: %d", play_status);
    pthread_mutex_unlock(&play_mutex);
    audio->release();
    video->release();
    delete audio;
    delete video;
    audio = nullptr;
    video = nullptr;
    LOGI("audio.release()  video.Release()");
    avformat_flush(fmt_ctx);
    av_packet_free(&pkt);
    avformat_close_input(&fmt_ctx);
    LOGI("avformat_close_input");
    pthread_mutex_lock(&play_mutex);
    play_status = IDLE;
    pthread_mutex_unlock(&play_mutex);
    return ret_play;
}

void FPlayer::pause() {
    pthread_mutex_lock(&play_mutex);
    if (play_status == PLAYING) {
        if (fmt_ctx) {
            av_read_pause(fmt_ctx);
        }
        if (video && video->stream_id != -1) {
            video->pause();
        }
        if (audio && audio->stream_id != -1) {
            audio->pause();
        }
    }
    pthread_mutex_unlock(&play_mutex);
}

void FPlayer::resume() {
    pthread_mutex_lock(&play_mutex);
    if (play_status == PLAYING) {
        if (fmt_ctx) {
            av_read_play(fmt_ctx);
        }
        if (video && video->stream_id != -1) {
            video->resume();
        }
        if (audio && audio->stream_id != -1) {
            audio->resume();
        }
    }
    pthread_mutex_unlock(&play_mutex);
}

int64_t FPlayer::get_current_time() {
    return avClock.ff_sec_time;
}

int FPlayer::get_play_state() {
    int state;
    pthread_mutex_lock(&play_mutex);
    state = play_status;
    pthread_mutex_unlock(&play_mutex);
    return state;
}

void FPlayer::release() {
}
