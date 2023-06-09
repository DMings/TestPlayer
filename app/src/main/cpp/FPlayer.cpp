//
// Created by Administrator on 2019/9/12.
//

#include <unistd.h>
#include "FPlayer.h"

FPlayer::FPlayer() : lockMutex_(), avIOInterruptCB_(), timeoutMS_() {
    glThread_ = new GLThread();
    avClock_ = new AVClock();
    pkt_ = nullptr;
    fmtCtx_ = nullptr;
    avIOInterruptCB_ = {.callback = &FPlayer::TimeoutInterruptCb, .opaque = this};
}

GLThread *FPlayer::GetGLThread() {
    return glThread_;
}

int FPlayer::Open(const char *url) {
    running_ = true;
    video_ = new Video(glThread_, avClock_);
    audio_ = new Audio(avClock_, cacheTimeMs_);
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

    fmtCtx_ = avformat_alloc_context();
    timeoutMS_ = GetCurrentTimeMs();
    fmtCtx_->interrupt_callback = avIOInterruptCB_;
    if (avformat_open_input(&fmtCtx_, url, fmt, &dic) < 0) {
        LOGE("Could not open source file %s", url);
        return -1;
    }
    LOGE("avformat_open_input... cost time: %ld", (av_gettime_relative() / 1000 - timeMs));
    timeMs = av_gettime_relative() / 1000;

    timeoutMS_ = GetCurrentTimeMs();
    AVDictionary *find_dic = nullptr;
    if (avformat_find_stream_info(fmtCtx_, &find_dic) < 0) {
        LOGE("Could not find stream information");
        return -2;
    }
    LOGE("avformat_find_stream_info... cost time: %ld", (av_gettime_relative() / 1000 - timeMs));
    audio_->OpenStream(fmtCtx_);
    video_->OpenStream(fmtCtx_);
    pkt_ = av_packet_alloc();
    // reset
    avClock_->Reset();
    findKeyFrame_ = false;
    for (auto itr = pktList_.begin(); itr != pktList_.end();) {
        auto *cPkt = (*itr);
        av_packet_free(&cPkt);
        itr = pktList_.erase(itr);
    }
    reachCacheTime_ = false;
    vPts_ = -1;
    aPts_ = -1;
    return 0;
}

int FPlayer::TimeoutInterruptCb(void *ctx) {
    auto player = static_cast<FPlayer *>(ctx);
    if ((GetCurrentTimeMs() - player->timeoutMS_) > FPlayer::READ_TIMEOUT_MS) {
        LOGE("TimeoutInterruptCb.... timeout: %ld", (GetCurrentTimeMs() - player->timeoutMS_));
        return AVERROR_EXIT;
    }
    if (!player->running_) {
        LOGE("TimeoutInterruptCb quit!!!");
    }
    return player->running_ ? 0 : AVERROR_EXIT;
}

int FPlayer::Handle() {
    int ret = -1;
    if (running_) {
        timeoutMS_ = GetCurrentTimeMs();
//        __TSC__(av_read_frame);
        ret = av_read_frame(fmtCtx_, pkt_);
//        __TEC_LIMIT__(av_read_frame, 10);
        if (ret == 0) {
            if (pkt_->stream_index == video_->StreamIndex() ||
                pkt_->stream_index == audio_->StreamIndex()) {

                if (pkt_->stream_index == video_->StreamIndex()) {
                    if (pkt_->pts <= vPts_) {
                        vPts_++;
                        pkt_->pts = vPts_;
                    }
                    vPts_ = pkt_->pts;
                } else {
                    if (pkt_->pts <= aPts_) {
                        aPts_++;
                        pkt_->pts = aPts_;
                    }
                    aPts_ = pkt_->pts;
                }

                if (pkt_->stream_index == video_->StreamIndex()) {
                    pkt_->time_base = video_->Stream()->time_base;
                    if (pkt_->flags == AV_PKT_FLAG_KEY && !findKeyFrame_) {
                        findKeyFrame_ = true;
                        int64_t videoKeyPts = av_rescale_q(pkt_->pts, pkt_->time_base,
                                                           AV_TIME_BASE_Q);
                        LOGE("findKeyFrame key pkt: %d pkt->pts: %ld pts: %ld", pkt_->stream_index,
                             pkt_->pts, videoKeyPts);
                        LOGE("pktList now: %ld videoKeyPts: %ld", pktList_.size(), videoKeyPts);
                        for (auto it = pktList_.begin(); it != pktList_.end();) {
                            auto *d = (*it);
                            if (av_rescale_q(d->pts, d->time_base, AV_TIME_BASE_Q) < videoKeyPts) {
                                av_packet_free(&d);
                                it = pktList_.erase(it);
                            } else {
                                it++;
                            }
                        }
                        reachCacheTime_ = true;
                        LOGE("pktList.size finish: %ld", pktList_.size());
                    }
                } else {
                    pkt_->time_base = audio_->Stream()->time_base;
                }
                if (!findKeyFrame_ || reachCacheTime_) {

                    auto clonePkt = av_packet_alloc();
                    av_packet_move_ref(clonePkt, pkt_);
                    pktList_.push_back(clonePkt);

                    if (reachCacheTime_) {
                        int64_t videoMaxTime = INT64_MIN;
                        int64_t videoMinTime = INT64_MAX;
                        int64_t audioMaxTime = INT64_MIN;
                        int64_t audioMinTime = INT64_MAX;
                        for (auto cPkt: pktList_) {
                            if (cPkt->stream_index == video_->StreamIndex()) {
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

                        if (cacheTimeMs_ == 0 ||
                            (videoMaxTime != INT64_MIN && videoMinTime != INT64_MAX &&
                             videoMaxTime - videoMinTime > cacheTimeMs_ * 1000) &&
                            (audioMaxTime != INT64_MIN && audioMinTime != INT64_MAX &&
                             audioMaxTime - audioMinTime > cacheTimeMs_ * 1000)) {
                            reachCacheTime_ = false;
                            for (auto itr = pktList_.begin(); itr != pktList_.end();) {
                                auto *cPkt = (*itr);
                                FPacket *f_pkt = alloc_packet();
                                f_pkt->avPacket = cPkt;
                                if (cPkt->stream_index == video_->StreamIndex()) {
                                    video_->PutAvPacket(f_pkt);
                                } else {
                                    audio_->PutAvPacket(f_pkt);
                                }
                                itr = pktList_.erase(itr);
                            }
                            LOGE("videoTime: %ld", videoMaxTime - videoMinTime);
                            LOGE("audioTime: %ld", audioMaxTime - audioMinTime);
                            LOGE("video GetAvPacketSize send: %ld", video_->GetAvPacketSize());
                            LOGE("audio GetAvPacketSize send: %ld", audio_->GetAvPacketSize());
                        } else {
                            int videoCount = 0;
                            int audioCount = 0;
                            for (auto cPkt: pktList_) {
                                if (cPkt->stream_index == video_->StreamIndex()) {
                                    videoCount++;
                                } else {
                                    audioCount++;
                                }
                            }

                            if (videoCount >= 60 || audioCount >= 200) {
                                for (auto itr = pktList_.begin(); itr != pktList_.end();) {
                                    auto *cPkt = (*itr);
                                    av_packet_free(&cPkt);
                                    itr = pktList_.erase(itr);
                                }
                            }
                        }
                    }
                } else {
                    AVPacket *clonePkt = av_packet_alloc();
                    av_packet_move_ref(clonePkt, pkt_);
                    FPacket *f_pkt = alloc_packet();
                    f_pkt->avPacket = clonePkt;
                    if (clonePkt->stream_index == video_->StreamIndex()) {
                        video_->PutAvPacket(f_pkt);
                    } else {
                        audio_->PutAvPacket(f_pkt);
                    }
                }
            }
            av_packet_unref(pkt_);
        } else {
            ret = -1;
        }
    }
    return ret;
}

void FPlayer::Close() {
    LOGE("play Close---------------------->");
    delete audio_;
    delete video_;
    audio_ = nullptr;
    video_ = nullptr;
    LOGE("audio null  video null");
    pthread_mutex_lock(&lockMutex_);
    av_packet_free(&pkt_);
    if (fmtCtx_) {
        avformat_close_input(&fmtCtx_);
    }
    pthread_mutex_unlock(&lockMutex_);
    LOGI("avformat_close_input fmt_ctx: %p", fmtCtx_);
}

int FPlayer::GetAudioCacheTimeMs() {
    if (audio_ && audio_->StreamIndex() != -1) {
        return (int) audio_->GetCacheTime();
    }
    return (int) cacheTimeMs_;
}

int FPlayer::GetAudioMaxCacheTimeMs() {
    if (audio_ && audio_->StreamIndex() != -1) {
        return (int) audio_->GetMaxCacheTime();
    }
    return (int) cacheTimeMs_;
}

int64_t FPlayer::GetCurTimeMs() {
    return avClock_->curTimeUs;
}

int64_t FPlayer::GetVideoNTPDelta() {
    if (video_ && video_->StreamIndex() != -1) {
        return (int) video_->GetVideoNTPDelta();
    }
    return -999999;
}

void FPlayer::Release() {
    running_ = false;
}

FPlayer::~FPlayer() {
    delete avClock_;
    delete glThread_;
}
