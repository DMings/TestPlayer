//
// Created by Administrator on 2019/9/12.
//

#include "Video.h"

extern bool ntpSuccess;
extern int64_t ntpDeltaMs;

Video::Video(GLThread *glThread, AVClock *avClock) : avClock_(avClock),
                                                     pthreadSleep_() {
    this->glThread_ = glThread;
    threadFinish_ = false;
}

int Video::StreamIndex() const {
    return streamId_;
}

AVStream *Video::Stream() const {
    return avStream_;
}

uint Video::SynchronizeVideo(int64_t lastPts, int64_t pktDuration) { // ms
    uint wanted_delay;
    int64_t diff_ms;
    int64_t duration;
    AVRational rational = avStream_->r_frame_rate;
    if (pktDuration) {
        duration = pktDuration;
    } else {
        if (rational.den * rational.num > 0) {
            duration = (int64_t) (1000.0 * rational.den / rational.num);
        } else {
            duration = 50;
        }
    }
    duration = duration < 200 ? duration : 200;

    if (avClock_->GetAudioPtsClock() == INT64_MIN) {
        diff_ms = avClock_->GetVideoPtsClock() - lastPts;
    } else {
        diff_ms = (avClock_->GetVideoPtsClock() - avClock_->GetAudioPtsClock()) / 1000;
    }

    if (diff_ms < 0) {
        wanted_delay = 0;
    } else {
        wanted_delay = std::min((int64_t) ((float) duration * 1.5f), diff_ms);
    }
    return wanted_delay;
}

void Video::PutAvPacket(FPacket *pkt) {
    if (streamId_ != -1) {
        pthread_mutex_lock(&cMutex_);
        pktList_.push_back(pkt);
        pthread_cond_signal(&cCond_);
        pthread_mutex_unlock(&cMutex_);
    }
}

uint64_t Video::GetAvPacketSize() {
    uint64_t size;
    pthread_mutex_lock(&cMutex_);
    size = pktList_.size();
    pthread_mutex_unlock(&cMutex_);
    return size;
}

int64_t Video::GetVideoNTPDelta() const {
    return videoNTPTimeMs_;
}

void *Video::VideoThreadProcess(void *arg) {
    AVFrame *frame = av_frame_alloc();
    int ret;
    auto *video = (Video *) arg;
    FPacket *videoPacket;
    video->glThread_->setParams(video->dstData_[0], video->avDecCtx_->width,
                                video->avDecCtx_->height);
    LOGI("VideoThreadProcess: run!!!");
    video->pthreadSleep_.reset();
    while (!video->threadFinish_) {
        do {
            videoPacket = nullptr;
            pthread_mutex_lock(&video->cMutex_);
            if (!video->pktList_.empty()) {
                videoPacket = video->pktList_.front();
                video->pktList_.pop_front();
            } else {
                if (!video->threadFinish_) {
                    pthread_cond_wait(&video->cCond_, &video->cMutex_);
                }
                if (!video->pktList_.empty()) {
                    videoPacket = video->pktList_.front();
                    video->pktList_.pop_front();
                }
            }
            pthread_mutex_unlock(&video->cMutex_);
        } while (videoPacket == nullptr && !video->threadFinish_);
        if (videoPacket != nullptr) {
            if (!video->threadFinish_) {
                if (videoPacket->checkout_time) {
                    avcodec_flush_buffers(video->avDecCtx_);
                }
                ret = avcodec_send_packet(video->avDecCtx_, videoPacket->avPacket);
                if (ret < 0) {
                    LOGE("Error video sending a packet for decoding video->pkt_list size: %ld",
                         video->pktList_.size());
                    av_packet_free(&videoPacket->avPacket);
                    free_packet(videoPacket);
                    LOGE("Error video end");
                    break;
                }
                while (ret >= 0) {
                    ret = avcodec_receive_frame(video->avDecCtx_, frame);
                    if (ret == AVERROR(EAGAIN)) {
//                LOGE("ret == AVERROR(EAGAIN)");
                        break;
                    } else if (ret == AVERROR_EOF || ret == AVERROR(EINVAL) ||
                               ret == AVERROR_INPUT_CHANGED) {
                        LOGE("video some err!");
                        break;
                    } else if (ret < 0) {
                        LOGE("video legitimate decoding errors");
                        break;
                    }

                    if (ntpSuccess) {
                        AVFrameSideData *sideData = av_frame_get_side_data(frame,
                                                                           AV_FRAME_DATA_SEI_UNREGISTERED);
                        if (sideData != nullptr && sideData->size > 16) {
                            char *cStr = reinterpret_cast<char *>(sideData->data);
                            std::string timeStr(cStr, 16, sideData->size - 16);
                            if (timeStr.substr(0, 4) == "NTP:") {
                                std::string t = timeStr.substr(4, timeStr.size());
                                try {
                                    long timeMs = std::stol(t);
                                    video->videoNTPTimeMs_ =
                                            GetCurrentTimeMs() + ntpDeltaMs - timeMs;
//                                    LOGI("timeMs: %ld videoNTPTimeMs_: %ld", timeMs,
//                                         video->videoNTPTimeMs_);
                                } catch (...) {
                                }
                            }
                        }
                    }

                    //  用 st 上的时基才对 av_stream
                    int64_t pts = av_rescale_q(frame->pts,
                                               video->avStream_->time_base,
                                               AV_TIME_BASE_Q);// us

                    int64_t pkt_duration = av_rescale_q(frame->duration,
                                                        video->avStream_->time_base,
                                                        AV_TIME_BASE_Q);// us
                    video->glThread_->lockDraw();
                    ret = sws_scale(video->swsContext_,
                                    (const uint8_t *const *) frame->data, frame->linesize,
                                    0, video->avDecCtx_->height,
                                    video->dstData_, video->dstLineSize_); // lock
                    video->glThread_->unlockDraw();

                    video->avClock_->SetVideoClock(pts);

                    uint delay = video->SynchronizeVideo(video->lastPts_ / 1000,
                                                         pkt_duration / 1000); // ms
//                    LOGI("audio GetAudioPtsClock: %ld GetVideoPtsClock: %ld delay: %d real diff: %ld listSize: %ld",
//                         video->avClock->GetAudioPtsClock() / 1000,
//                         video->avClock->GetVideoPtsClock() / 1000,
//                         delay,
//                         (video->avClock->GetVideoPtsClock() - video->avClock->GetAudioPtsClock()) /
//                         1000,
//                         listSize);
                    video->lastPts_ = pts;
                    video->pthreadSleep_.sleep((uint) delay);
                    video->glThread_->draw();
                }
            }
            av_packet_free(&videoPacket->avPacket);
            free_packet(videoPacket);
        }
    }
    video->glThread_->setParams(nullptr, 1, 1);
    video->glThread_->drawBackground();
    av_frame_free(&frame);
    LOGI("VideoThreadProcess end video->pkt_list size: %ld", video->pktList_.size());
    return nullptr;
}


int Video::OpenStream(AVFormatContext *fmtCtx) {
    int ret = OpenCodecContext(&streamId_, &avDecCtx_,
                               fmtCtx, AVMEDIA_TYPE_VIDEO);
    if (ret >= 0) {
//        int nb_cpu_s = av_cpu_count();
//        if (nb_cpu_s == 0) {
//            nb_cpu_s = 8;
//        }
//        av_dec_ctx->thread_count = nb_cpu_s;
//        LOGI("nb_cpu_s: %d", nb_cpu_s);
        avStream_ = fmtCtx->streams[streamId_];
        LOGI("Video -pix_fmt %s -video_size %d x %d",
             av_get_pix_fmt_name(avDecCtx_->pix_fmt), avDecCtx_->width, avDecCtx_->height);
    } else {
        streamId_ = -1;
    }
    if (avStream_) {
        AVRational rational = avStream_->r_frame_rate;
        LOGI("rational: %d => %d", rational.den, rational.num);
        swsContext_ = sws_getContext(
                avDecCtx_->width, avDecCtx_->height, avDecCtx_->pix_fmt,
                avDecCtx_->width, avDecCtx_->height, AV_PIX_FMT_RGBA,
                SWS_BILINEAR, nullptr, nullptr, nullptr);
        if ((ret = av_image_alloc(dstData_, dstLineSize_,
                                  avDecCtx_->width, avDecCtx_->height,
                                  AV_PIX_FMT_RGBA, 1)) < 0) {
            LOGE("Could not allocate destination image: %d", ret);
            streamId_ = -1;
        } else {
            LOGI("dst_data size: %d", ret);
            pthread_create(&pVideoTid_, nullptr, Video::VideoThreadProcess, this);
        }
    }
    return ret;
}

Video::~Video() {
    LOGI("video pthread_join wait");
    pthreadSleep_.interrupt();
    pthread_mutex_lock(&cMutex_);
    threadFinish_ = true;
    pthread_cond_signal(&cCond_);
    pthread_mutex_unlock(&cMutex_);
    if (pVideoTid_) {
        pthread_join(pVideoTid_, nullptr);
    }
    ClearList();
    LOGI("video pthread_join done");
    if (dstData_[0] != nullptr) {
        av_freep(&dstData_[0]);
        dstData_[0] = nullptr;
    }
    if (swsContext_ != nullptr) {
        sws_freeContext(swsContext_);
        swsContext_ = nullptr;
    }
    if (avDecCtx_) {
//        avcodec_close(av_dec_ctx);
        avcodec_free_context(&avDecCtx_);
    }
    streamId_ = 0;
    avStream_ = nullptr;
    avDecCtx_ = nullptr;
    LOGI("video Release");
}