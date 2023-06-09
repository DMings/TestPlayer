//
// Created by Administrator on 2019/9/12.
//

#include "Audio.h"
#include "./render/AudioSpeed.h"

Audio::Audio(AVClock *avClock, uint cacheTime) : cacheSetTime_((float) cacheTime), avClock(avClock),
                                                 speedList_() {
    threadFinish_ = false;
    playAudio_ = nullptr;
    cacheTime_ = cacheMaxTime_ = cacheSetTime_;
}

uint Audio::GetMaxCacheTime() const {
    return (uint) cacheMaxTime_;
}

uint Audio::GetCacheTime() const {
    return (uint) cacheTime_;
}

int Audio::StreamIndex() const {
    return streamId_;
}

AVStream *Audio::Stream() const {
    return avStream_;
}

void Audio::PutAvPacket(FPacket *pkt) {
    if (streamId_ != -1) {
        pthread_mutex_lock(&cMutex_);
        pktList_.push_back(pkt);
        pthread_cond_signal(&cCond_);
        pthread_mutex_unlock(&cMutex_);
    }
}

uint64_t Audio::GetAvPacketSize() {
    uint64_t size;
    pthread_mutex_lock(&cMutex_);
    size = pktList_.size();
    pthread_mutex_unlock(&cMutex_);
    return size;
}

void *Audio::AudioThreadProcess(void *arg) {
    int ret;
    FPacket *audioPacket;
    auto *audio = (Audio *) arg;
    AVFrame *frame = av_frame_alloc();
    int sampleRate = audio->avDecCtx_->sample_rate;
    int nbChannels = audio->avDecCtx_->ch_layout.nb_channels;
    int bytes = av_get_bytes_per_sample(audio->avDecCtx_->sample_fmt);
    auto *audioSpeed = new AudioSpeed(sampleRate, nbChannels);
    auto audioData = static_cast<uint8_t *>(memalign(64, sampleRate * nbChannels * bytes));
    LOGI("AudioThreadProcess: run!!!");
    float lastPlaySpeed = 1.0f;
    while (!audio->threadFinish_) {
        do {
            audioPacket = nullptr;
            pthread_mutex_lock(&audio->cMutex_);
            if (!audio->pktList_.empty()) {
                audioPacket = audio->pktList_.front();
                audio->pktList_.pop_front();
            } else {
                if (!audio->threadFinish_) {
                    LOGI("audio pthread_cond_wait");
                    pthread_cond_wait(&audio->cCond_, &audio->cMutex_);
                }
                if (!audio->pktList_.empty()) {
                    audioPacket = audio->pktList_.front();
                    audio->pktList_.pop_front();
                }
            }
            pthread_mutex_unlock(&audio->cMutex_);
        } while (audioPacket == nullptr && !audio->threadFinish_);
        if (audioPacket != nullptr) {
            if (!audio->threadFinish_) {
                if (audioPacket->checkout_time) {
                    avcodec_flush_buffers(audio->avDecCtx_);
                }
                ret = avcodec_send_packet(audio->avDecCtx_, audioPacket->avPacket);
                if (ret < 0) {
                    LOGE("Error audio sending a packet for decoding pkt_list size: %ld",
                         audio->pktList_.size());
                    av_packet_free(&audioPacket->avPacket);
                    free_packet(audioPacket);
                    LOGE("Error Audio end");
                    break;
                }
                while (ret >= 0) {
                    ret = avcodec_receive_frame(audio->avDecCtx_, frame);

                    if (ret == AVERROR(EAGAIN)) {
//                LOGE("ret == AVERROR(EAGAIN)");
                        break;
                    } else if (ret == AVERROR_EOF || ret == AVERROR(EINVAL) ||
                               ret == AVERROR_INPUT_CHANGED) {
                        LOGE("audio some err!");
                        break;
                    } else if (ret < 0) {
                        LOGE("audio legitimate decoding errors");
                        break;
                    }
                    size_t pktListSize = audio->pktList_.size();
                    float pktListTime = audio->GetPktListTime();
                    auto speedUnprocessedSampleTime = (float) (1000.0 *
                                                               audioSpeed->NumUnprocessedSamples() /
                                                               audio->avDecCtx_->sample_rate);
                    audio->cacheTime_ = pktListTime + speedUnprocessedSampleTime + (float) (1000.0 *
                                                                                            audio->playAudio_->SampleCount() /
                                                                                            audio->avDecCtx_->sample_rate);
                    audio->CalcPlaySpeed(pktListTime + speedUnprocessedSampleTime);
                    if (lastPlaySpeed != audio->curSpeed_) {
                        audioSpeed->SetSpeed(audio->curSpeed_);
                        lastPlaySpeed = audio->curSpeed_;
//                        LOGI("curSpeed_: %f targetSpeed_: %f numSamples: %d pkt_list size: %ld pktListTime: %f pktListSize: %ld speedSamplesTime: %f",
//                             audio->curSpeed_, audio->targetSpeed_,
//                             frame->nb_samples,
//                             audio->pktList_.size(), pktListTime, pktListSize,
//                             speedUnprocessedSampleTime);
                    }

                    int sendSamples = frame->nb_samples;
                    do {
                        uint32_t numSamples = audioSpeed->GetSamples(
                                reinterpret_cast<soundtouch::SAMPLETYPE *>(frame->data[0]),
                                sendSamples,
                                reinterpret_cast<soundtouch::SAMPLETYPE *>(audioData), 512);
                        sendSamples = 0;
                        if (numSamples <= 0) {
                            break;
                        }
                        if (numSamples > 0 && audio->playAudio_) {
                            if (audio->playAudio_->SampleCount() < 400) {
                                LOGE("SampleCount: %d", audio->playAudio_->SampleCount());
                            }
                            audio->playAudio_->PutSamples(audioData, (int) numSamples);
                        }
                    } while (true);

                    int64_t pts = av_rescale_q(audioPacket->avPacket->pts,
                                               audioPacket->avPacket->time_base,
                                               AV_TIME_BASE_Q);
                    pts -= (int64_t) speedUnprocessedSampleTime * 1000;
                    audio->avClock->curTimeUs = pts;
                    audio->avClock->SetAudioClock(pts);

                }
            }
            av_packet_free(&audioPacket->avPacket);
            free_packet(audioPacket);
        }
    }
    av_frame_free(&frame);
    audioSpeed->Flush();
    free(audioData);
    LOGI("AudioThreadProcess end pkt_list size: %ld", audio->pktList_.size());
    return nullptr;
}

float Audio::GetPktListTime() {
    pthread_mutex_lock(&cMutex_);
    int64_t audioMaxTime = INT64_MIN;
    int64_t audioMinTime = INT64_MAX;
    for (auto pkt: pktList_) {
        audioMaxTime = std::max(audioMaxTime,
                                av_rescale_q(pkt->avPacket->pts, pkt->avPacket->time_base,
                                             AV_TIME_BASE_Q));
        audioMinTime = std::min(audioMinTime,
                                av_rescale_q(pkt->avPacket->pts, pkt->avPacket->time_base,
                                             AV_TIME_BASE_Q));
    }
    if (audioMaxTime != INT64_MIN && audioMinTime != INT64_MAX &&
        audioMaxTime - audioMinTime != 0) {
        float t = (float) (audioMaxTime - audioMinTime) / 1000.f;
        if (t < (float) pktList_.size() * pktDuration_ / 2.f) {
            LOGE("pkt_list Size calc: %f  t=> %f", (float) pktList_.size() * pktDuration_, t);
        }
        pthread_mutex_unlock(&cMutex_);
        return t;
    } else {
        pthread_mutex_unlock(&cMutex_);
        return (float) pktList_.size() * pktDuration_;
    }
}

void Audio::CalcPlaySpeed(float listTime) {
    float maxTime = (float) cacheMaxTime_ + 30;
    float minTime = std::max((float) cacheMaxTime_ / 2.5f, 146.f);
    minTime = std::min(minTime, 500.f);
    float speed = 1.0f;
    if (listTime > maxTime) {
        float t = listTime - maxTime;
        float v = t / maxTime;
        v = std::min(v, 2.f);
        speed *= (1.f + v);
    } else if (listTime < minTime) {
        float v = listTime / minTime;
        v = 1.f - (1.f - v);
        v = std::max(v, 0.5f);
        speed *= v;
    }
    int speedMaxCount = 3;
    speedList_.push_back(speed);
    for (;;) {
        if (speedList_.size() > speedMaxCount) {
            speedList_.pop_front();
        } else {
            break;
        }
    }
    float avgSpeed = 0.f;
    for (auto s: speedList_) {
        avgSpeed += s;
    }
    if (speedList_.size() == speedMaxCount) {
        avgSpeed = avgSpeed / (float) speedList_.size();
    } else {
        avgSpeed = avgSpeed / (float) speedList_.size();
        avgSpeed = std::min(avgSpeed, 1.1f);
        avgSpeed = std::max(avgSpeed, 0.9f);
    }
    targetSpeed_ = avgSpeed;
    if (std::abs(targetSpeed_ - curSpeed_) > 0.0001) {
        if (targetSpeed_ - curSpeed_ > 0) {
            curSpeed_ += std::min(targetSpeed_ - curSpeed_, 0.05f);
        } else {
            curSpeed_ -= std::min(curSpeed_ - targetSpeed_, 0.08f);
        }
    }
    if (minTime > listTime) {
        reachMinTimeCount_++;
        reachNormalTimeCount_ = 0;
    }
    if (listTime > minTime * 1.1f) {
        reachNormalTimeCount_++;
    }
    if (reachMinTimeCount_ >= 3) {
        reachMinTimeCount_ = 0;
        cacheMaxTime_ *= 1.1f;
        cacheMaxTime_ = std::min(cacheMaxTime_, 1500.f);
    }
    if (reachNormalTimeCount_ >= 10) {
        reachNormalTimeCount_ = 0;
        reachMinTimeCount_ = 0;
        if (cacheMaxTime_ > cacheSetTime_) {
            cacheMaxTime_ -= (cacheMaxTime_ - cacheSetTime_) * 0.05f;
        }
    }
}


int Audio::OpenStream(AVFormatContext *fmtCtx) {
    streamId_ = -1;
    int ret = OpenCodecContext(&streamId_, &avDecCtx_,
                               fmtCtx, AVMEDIA_TYPE_AUDIO);
    LOGI("stream_id: %d", streamId_);
    if (ret >= 0) {
        avStream_ = fmtCtx->streams[streamId_];
    } else {
        streamId_ = -1;
    }
    if (avStream_ && SampleRateValid(avDecCtx_->sample_rate)
        && avDecCtx_->sample_fmt == AV_SAMPLE_FMT_S16) {
        pktDuration_ = 1024000.f / (float) avDecCtx_->sample_rate;
        int channels = avDecCtx_->ch_layout.nb_channels;
        LOGI("Audio -ac %d -ar %d byte %d fmt_name:%s", channels,
             avDecCtx_->sample_rate,
             av_get_bytes_per_sample(avDecCtx_->sample_fmt),
             av_get_sample_fmt_name(avDecCtx_->sample_fmt));
        playAudio_ = new AudioPlay(avDecCtx_->sample_rate, channels);
        playAudio_->Start();
        pthread_create(&pAudioTid_, nullptr, AudioThreadProcess, this);
    } else {
        streamId_ = -1;
        LOGI("SampleRateValid: %d AV_SAMPLE_FMT_S16: %d", avDecCtx_->sample_rate,
             (avDecCtx_->sample_fmt == AV_SAMPLE_FMT_S16));
    }
    return ret;
}

Audio::~Audio() {
    LOGI("audio pthread_join wait");
    if (playAudio_) {
        playAudio_->Stop();
    }
    pthread_mutex_lock(&cMutex_);
    threadFinish_ = true;
    pthread_cond_signal(&cCond_);
    pthread_mutex_unlock(&cMutex_);
    if (pAudioTid_) {
        pthread_join(pAudioTid_, nullptr);
    }
    ClearList();
    delete playAudio_;
    LOGI("audio pthread_join done");
    if (avDecCtx_) {
        avcodec_free_context(&avDecCtx_);
        avDecCtx_ = nullptr;
    }
    streamId_ = 0;
    avStream_ = nullptr;
    avDecCtx_ = nullptr;
    LOGI("audio Release");
}

bool Audio::SampleRateValid(int sampleRate) {
    bool success = false;
    switch (sampleRate) {
        case 8000:
        case 11025:
        case 16000:
        case 22050:
        case 24000:
        case 32000:
        case 44100:
        case 48000:
        case 64000:
        case 88200:
        case 96000:
        case 192000:
            success = true;
            break;
        default:
            break;
    }
    return success;
}