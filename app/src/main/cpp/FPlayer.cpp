//
// Created by Administrator on 2019/9/12.
//

#include <unistd.h>
#include "FPlayer.h"

FPlayer::FPlayer() {
    glThread = new GLThread();
    avClock = new AVClock();
    pkt = nullptr;
}

GLThread *FPlayer::GetGLThread() {
    return glThread;
}

int FPlayer::Start(const char *url) {
    video = new Video(glThread, avClock);
    audio = new Audio(avClock);
    //
    auto protocolName = avio_find_protocol_name(url);
    LOGI("protocolName: %s", protocolName);
    LOGI("avformat_open_input %s", url);
    const AVInputFormat *fmt = av_find_input_format("flv");
    int64_t timeMs = av_gettime_relative() / 1000;
    AVDictionary *dic = nullptr;
    av_dict_set_int(&dic, "tcp_nodelay", 1, 0);
    av_dict_set_int(&dic, "rtmp_buffer", 2000, 0);

    av_dict_set_int(&dic, "analyzeduration", 1 * AV_TIME_BASE, 0);
    av_dict_set_int(&dic, "fpsprobesize", 0, 0);

    if (avformat_open_input(&fmt_ctx, url, fmt, &dic) < 0) {
        LOGE("Could not open source file %s", url);
        return -1;
    }
    LOGE("avformat_open_input... cost time: %ld", (av_gettime_relative() / 1000 - timeMs));
    timeMs = av_gettime_relative() / 1000;

    AVDictionary *find_dic = nullptr;
    if (avformat_find_stream_info(fmt_ctx, &find_dic) < 0) {
        LOGE("Could not find stream information");
        return -2;
    }
    LOGE("avformat_find_stream_info... cost time: %ld", (av_gettime_relative() / 1000 - timeMs));
    audio->open_stream(fmt_ctx);
    video->open_stream(fmt_ctx);
    pkt = av_packet_alloc();
    return 0;
}

int FPlayer::Loop() {
    int ret = 0;
    bool findKeyFrame = false;
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
    return 0;
}

void FPlayer::Pause() {
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

void FPlayer::Resume() {
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

int64_t FPlayer::GetCurTimeMs() {
    return avClock->ff_sec_time;
}

void FPlayer::Stop() {
    LOGE("play Stop");
    if (audio) {
        audio->release();
    }
    if (video) {
        video->release();
    }
    delete audio;
    delete video;
    audio = nullptr;
    video = nullptr;
    LOGI("audio.Stop()  video.Release()");
    avformat_flush(fmt_ctx);
    av_packet_free(&pkt);
    avformat_close_input(&fmt_ctx);
    LOGI("avformat_close_input");
}

FPlayer::~FPlayer() {
    delete avClock;
    delete glThread;
}
