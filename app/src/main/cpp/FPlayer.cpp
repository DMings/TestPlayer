//
// Created by Administrator on 2019/9/12.
//

#include <unistd.h>
#include "FPlayer.h"

FPlayer::FPlayer() {
    glThread = new GLThread();
    avClock = new AVClock();
    pkt = nullptr;
    fmt_ctx = nullptr;
}

GLThread *FPlayer::GetGLThread() {
    return glThread;
}

int FPlayer::Start(const char *url) {
    running = true;
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
    std::list<AVPacket *> pktList;
    int64_t cacheTime = 200 * 1000;
    bool reachCacheTime = false;
    int64_t vPts = -1;
    int64_t aPts = -1;
    do {
        ret = av_read_frame(fmt_ctx, pkt);
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
                        LOGE("videoTime: %ld", videoMaxTime - videoMinTime);
                        LOGE("audioTime: %ld", audioMaxTime - audioMinTime);

                        if (cacheTime == 0 ||
                            (videoMaxTime != INT64_MIN && videoMinTime != INT64_MAX &&
                             videoMaxTime - videoMinTime > cacheTime) &&
                            (audioMaxTime != INT64_MIN && audioMinTime != INT64_MAX &&
                             audioMaxTime - audioMinTime > cacheTime)) {
                            reachCacheTime = false;
                            LOGE("pktList send: %ld", pktList.size());
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
            break;
        }
    } while (running);
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
    if (fmt_ctx) {
//        avformat_flush(fmt_ctx); //ff_read_frame_flush
        av_packet_free(&pkt);
        avformat_close_input(&fmt_ctx);
    }
    LOGI("avformat_close_input");
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
    running = false;
}

FPlayer::~FPlayer() {
    delete avClock;
    delete glThread;
}
