//
// Created by Administrator on 2019/9/12.
//

#include <unistd.h>
#include "FPlayer.h"

FPlayer::FPlayer() : lock_mutex(), avIOInterruptCB_(), timeoutMS_() {
    glThread = new GLThread();
    avClock = new AVClock();
    pkt = nullptr;
    fmt_ctx = nullptr;
    avIOInterruptCB_ = {.callback = &FPlayer::TimeoutInterruptCb, .opaque = this};
}

GLThread *FPlayer::GetGLThread() {
    return glThread;
}

int FPlayer::Open(const char *url) {
    running = true;
    video = new Video(glThread, avClock);
    audio = new Audio(avClock, cacheTimeMs);
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

    fmt_ctx = avformat_alloc_context();
    timeoutMS_ = GetCurrentTimeMs();
    fmt_ctx->interrupt_callback = avIOInterruptCB_;
    if (avformat_open_input(&fmt_ctx, url, fmt, &dic) < 0) {
        LOGE("Could not open source file %s", url);
        return -1;
    }
    LOGE("avformat_open_input... cost time: %ld", (av_gettime_relative() / 1000 - timeMs));
    timeMs = av_gettime_relative() / 1000;

    timeoutMS_ = GetCurrentTimeMs();
    AVDictionary *find_dic = nullptr;
    if (avformat_find_stream_info(fmt_ctx, &find_dic) < 0) {
        LOGE("Could not find stream information");
        return -2;
    }
    LOGE("avformat_find_stream_info... cost time: %ld", (av_gettime_relative() / 1000 - timeMs));
    audio->open_stream(fmt_ctx);
    video->open_stream(fmt_ctx);
    pkt = av_packet_alloc();

    int ret = 0;

    return 0;
}

int FPlayer::TimeoutInterruptCb(void *ctx) {
    auto player = static_cast<FPlayer *>(ctx);
//    if ((GetCurrentTimeMs() - streamsPush->timeoutMS_) > FPlayer::SEND_TIMEOUT_MS) {
//        LOGE("TimeoutInterruptCb.... timeout: %ld", (GetCurrentTimeMs() - streamsPush->timeoutMS_));
//        return AVERROR_EXIT;
//    }
    return player->running ? 0 : AVERROR_EXIT;
}

int FPlayer::Handle() {
    int ret = -1;
    if (running) {
        timeoutMS_ = GetCurrentTimeMs();
//        __TSC__(av_read_frame);
        ret = av_read_frame(fmt_ctx, pkt);
//        __TEC_LIMIT__(av_read_frame, 10);
        if (ret == 0) {
            if (pkt->stream_index == video->stream_id || pkt->stream_index == audio->stream_id) {

                if (pkt->stream_index == video->stream_id) {
                    if (pkt->pts <= vPts) {
                        vPts++;
                        pkt->pts = vPts;
                    }
                    vPts = pkt->pts;
                } else {
                    if (pkt->pts <= aPts) {
                        aPts++;
                        pkt->pts = aPts;
                    }
                    aPts = pkt->pts;
                }

                if (pkt->stream_index == video->stream_id) {
                    pkt->time_base = video->av_stream->time_base;
                    if (pkt->flags == AV_PKT_FLAG_KEY && !findKeyFrame) {
                        findKeyFrame = true;
                        int64_t videoKeyPts = av_rescale_q(pkt->pts, pkt->time_base,
                                                           AV_TIME_BASE_Q);
                        LOGE("findKeyFrame key pkt: %d pkt->pts: %ld pts: %ld", pkt->stream_index,
                             pkt->pts, videoKeyPts);
                        LOGE("pktList now: %ld videoKeyPts: %ld", pktList.size(), videoKeyPts);
                        for (auto it = pktList.begin(); it != pktList.end();) {
                            auto *d = (*it);
                            if (av_rescale_q(d->pts, d->time_base, AV_TIME_BASE_Q) < videoKeyPts) {
                                av_packet_free(&d);
                                it = pktList.erase(it);
                            } else {
                                it++;
                            }
                        }
                        reachCacheTime = true;
                        LOGE("pktList.size finish: %ld", pktList.size());
                    }
                } else {
                    pkt->time_base = audio->av_stream->time_base;
                }
                if (!findKeyFrame || reachCacheTime) {

                    auto clonePkt = av_packet_alloc();
                    av_packet_move_ref(clonePkt, pkt);
                    pktList.push_back(clonePkt);

                    if (reachCacheTime) {
                        int64_t videoMaxTime = INT64_MIN;
                        int64_t videoMinTime = INT64_MAX;
                        int64_t audioMaxTime = INT64_MIN;
                        int64_t audioMinTime = INT64_MAX;
                        for (auto cPkt: pktList) {
                            if (cPkt->stream_index == video->stream_id) {
                                videoMaxTime = std::max(videoMaxTime,
                                                        av_rescale_q(cPkt->pts, cPkt->time_base,
                                                                     AV_TIME_BASE_Q));
                                videoMinTime = std::min(videoMinTime,
                                                        av_rescale_q(cPkt->pts, cPkt->time_base,
                                                                     AV_TIME_BASE_Q));
                            } else {
                                audioMaxTime = std::max(audioMaxTime,
                                                        av_rescale_q(cPkt->pts, cPkt->time_base,
                                                                     AV_TIME_BASE_Q));
                                audioMinTime = std::min(audioMinTime,
                                                        av_rescale_q(cPkt->pts, cPkt->time_base,
                                                                     AV_TIME_BASE_Q));
                            }
                        }

                        if (cacheTimeMs == 0 ||
                            (videoMaxTime != INT64_MIN && videoMinTime != INT64_MAX &&
                             videoMaxTime - videoMinTime > cacheTimeMs * 1000) &&
                            (audioMaxTime != INT64_MIN && audioMinTime != INT64_MAX &&
                             audioMaxTime - audioMinTime > cacheTimeMs * 1000)) {
                            reachCacheTime = false;
                            for (auto itr = pktList.begin(); itr != pktList.end();) {
                                auto *cPkt = (*itr);
                                FPacket *f_pkt = alloc_packet();
                                f_pkt->avPacket = cPkt;
                                if (cPkt->stream_index == video->stream_id) {
                                    video->putAvPacket(f_pkt);
                                } else {
                                    audio->putAvPacket(f_pkt);
                                }
                                itr = pktList.erase(itr);
                            }
                            LOGE("videoTime: %ld", videoMaxTime - videoMinTime);
                            LOGE("audioTime: %ld", audioMaxTime - audioMinTime);
                            LOGE("video getAvPacketSize send: %ld", video->getAvPacketSize());
                            LOGE("audio getAvPacketSize send: %ld", audio->getAvPacketSize());
                        } else {
                            int videoCount = 0;
                            int audioCount = 0;
                            for (auto cPkt: pktList) {
                                if (cPkt->stream_index == video->stream_id) {
                                    videoCount++;
                                } else {
                                    audioCount++;
                                }
                            }

                            if (videoCount >= 60 || audioCount >= 200) {
                                for (auto itr = pktList.begin(); itr != pktList.end();) {
                                    auto *cPkt = (*itr);
                                    av_packet_free(&cPkt);
                                    itr = pktList.erase(itr);
                                }
                            }
                        }
                    }
                } else {
                    AVPacket *clonePkt = av_packet_alloc();
                    av_packet_move_ref(clonePkt, pkt);
                    FPacket *f_pkt = alloc_packet();
                    f_pkt->avPacket = clonePkt;
                    if (clonePkt->stream_index == video->stream_id) {
                        video->putAvPacket(f_pkt);
                    } else {
                        audio->putAvPacket(f_pkt);
                    }
                }
            }
            av_packet_unref(pkt);
        } else {
            ret = -1;
        }
    }
    return ret;
}

void FPlayer::Close() {
    LOGE("play Stop---------------------->");
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
    LOGE("audio.Stop()  video.Release()");
    pthread_mutex_lock(&lock_mutex);
    if (fmt_ctx) {
//        avformat_flush(fmt_ctx); //ff_read_frame_flush
        av_packet_free(&pkt);
        avformat_close_input(&fmt_ctx);
    }
    pthread_mutex_unlock(&lock_mutex);
    LOGI("avformat_close_input");
}

void FPlayer::Pause() {
    pthread_mutex_lock(&lock_mutex);
    if (fmt_ctx) {
        av_read_pause(fmt_ctx);
    }
    pthread_mutex_unlock(&lock_mutex);
    if (video && video->stream_id != -1) {
        video->pause();
    }
    if (audio && audio->stream_id != -1) {
        audio->pause();
    }
}

void FPlayer::Resume() {
    pthread_mutex_lock(&lock_mutex);
    if (fmt_ctx) {
        av_read_play(fmt_ctx);
    }
    pthread_mutex_unlock(&lock_mutex);
    if (video && video->stream_id != -1) {
        video->resume();
    }
    if (audio && audio->stream_id != -1) {
        audio->resume();
    }
}

int FPlayer::GetAudioCacheTimeMs() {
    if (audio && audio->stream_id != -1) {
        return (int) audio->getCacheTime();
    }
    return (int) cacheTimeMs;
}

int64_t FPlayer::GetCurTimeMs() {
    return avClock->curTimeUs;
}

void FPlayer::Stop() {
    running = false;
}

FPlayer::~FPlayer() {
    delete avClock;
    delete glThread;
}
