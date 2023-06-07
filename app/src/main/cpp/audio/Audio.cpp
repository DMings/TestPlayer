//
// Created by Administrator on 2019/9/12.
//

#include "Audio.h"
#include "./render/AudioSpeed.h"

Audio::Audio(AVClock *avClock, uint cacheTime) : cacheSetTime((float) cacheTime), avClock(avClock),
                                                 speedList() {
    thread_finish = false;
    playAudio = nullptr;
    cacheCurTime = cacheSetTime;
}

uint Audio::getCacheTime() {
    return (uint) cacheCurTime;
}

void Audio::putAvPacket(FPacket *pkt) {
    if (stream_id != -1) {
        pthread_mutex_lock(&c_mutex);
        pkt_list.push_back(pkt);
        pthread_cond_signal(&c_cond);
        pthread_mutex_unlock(&c_mutex);
    }
}

uint64_t Audio::getAvPacketSize() {
    uint64_t size;
    pthread_mutex_lock(&c_mutex);
    size = pkt_list.size();
    pthread_mutex_unlock(&c_mutex);
    return size;
}

void *Audio::audioProcess(void *arg) {
    int ret = 0;
    FPacket *audio_packet = nullptr;
    auto *audio = (Audio *) arg;
    AVFrame *frame = av_frame_alloc();
    int sampleRate = audio->av_dec_ctx->sample_rate;
    int nbChannels = audio->av_dec_ctx->ch_layout.nb_channels;
    int bytes = av_get_bytes_per_sample(audio->av_dec_ctx->sample_fmt);
    auto *audioSpeed = new AudioSpeed(sampleRate, nbChannels);
    auto audioData = static_cast<uint8_t *>(memalign(64, sampleRate * nbChannels * bytes));
    LOGI("audioProcess: run!!!");
    float lastPlaySpeed = 1.0f;
    float playSpeed = 1.0f;
    float pktListTime = 0.f;
    while (!audio->thread_finish) {
        do {
            audio_packet = nullptr;
            pthread_mutex_lock(&audio->c_mutex);
            if (!audio->pkt_list.empty()) {
                audio_packet = audio->pkt_list.front();
                audio->pkt_list.pop_front();
            } else {
                pthread_cond_wait(&audio->c_cond, &audio->c_mutex);
                if (!audio->pkt_list.empty()) {
                    audio_packet = audio->pkt_list.front();
                    audio->pkt_list.pop_front();
                }
            }
            pktListTime = audio->getPktListTime();
            pthread_mutex_unlock(&audio->c_mutex);
        } while (audio_packet == nullptr && !audio->thread_finish);
        if (audio_packet != nullptr) {
            if (!audio->thread_finish) {
                if (audio_packet->checkout_time) {
                    avcodec_flush_buffers(audio->av_dec_ctx);
                }
                ret = avcodec_send_packet(audio->av_dec_ctx, audio_packet->avPacket);
                if (ret < 0) {
                    LOGE("Error audio sending a packet for decoding pkt_list size: %ld",
                         audio->pkt_list.size());
                    av_packet_free(&audio_packet->avPacket);
                    free_packet(audio_packet);
                    LOGE("Error Audio end");
                    break;
                }
                while (ret >= 0) {
                    ret = avcodec_receive_frame(audio->av_dec_ctx, frame);

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

                    auto speedUnprocessedSampleTime = (float) (1000.0 * audioSpeed->NumUnprocessedSamples() /
                                                     audio->av_dec_ctx->sample_rate);
                    playSpeed = audio->getSpeed(pktListTime + speedUnprocessedSampleTime);
                    if (lastPlaySpeed != playSpeed) {
                        audioSpeed->SetSpeed(playSpeed);
                        lastPlaySpeed = playSpeed;
//                        LOGI("playSpeed: %f numSamples: %d pkt_list size: %ld pktListTime: %f speedSamplesTime: %f",
//                             playSpeed,
//                             frame->nb_samples,
//                             audio->pkt_list.size(), pktListTime, speedUnprocessedSampleTime);
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
                        if (numSamples > 0 && audio->playAudio) {
                            while (audio->playAudio->SampleCount() > 512 * 4) {
                                usleep(1000);
                            }
                            if (audio->playAudio->SampleCount() < 400) {
                                LOGE("SampleCount: %d", audio->playAudio->SampleCount());
                            }

                            audio->playAudio->PutSamples(audioData, (int) numSamples);
                        }
                    } while (true);

                    int64_t pts = av_rescale_q(audio_packet->avPacket->pts,
                                               audio_packet->avPacket->time_base,
                                               AV_TIME_BASE_Q);
                    pts -= (int64_t) speedUnprocessedSampleTime * 1000;
                    audio->avClock->curTimeUs = pts;
                    audio->avClock->SetAudioClock(pts);

                }
            }
            av_packet_free(&audio_packet->avPacket);
            free_packet(audio_packet);
        }
    }
    av_frame_free(&frame);
    audioSpeed->Flush();
    free(audioData);
    LOGI("audioProcess end pkt_list size: %ld", audio->pkt_list.size());
    return nullptr;
}

float Audio::getPktListTime() {
    int64_t audioMaxTime = INT64_MIN;
    int64_t audioMinTime = INT64_MAX;
    for (auto pkt: pkt_list) {
        audioMaxTime = std::max(audioMaxTime,
                                av_rescale_q(pkt->avPacket->pts, pkt->avPacket->time_base,
                                             AV_TIME_BASE_Q));
        audioMinTime = std::min(audioMinTime,
                                av_rescale_q(pkt->avPacket->pts, pkt->avPacket->time_base,
                                             AV_TIME_BASE_Q));
    }
    if (audioMaxTime != INT64_MIN && audioMinTime != INT64_MAX &&
        audioMaxTime - audioMinTime != 0) {
        return (float) (audioMaxTime - audioMinTime) / 1000.f;
    } else {
        return (float) pkt_list.size() * pktDuration;
    }
}

float Audio::getSpeed(float listTime) {
    float maxTime = (float) cacheCurTime + 30;
    float minTime = std::max((float) cacheCurTime / 2.5f, 146.f);
    minTime = std::min(minTime, 500.f);
    float speed = 1.0f;
    if (listTime > maxTime) {
        float t = listTime - maxTime;
        float v = t / maxTime;
        v = std::min(v, 3.f);
        v *= 0.5f;
        speed *= (1.f + v);
    } else if (listTime < minTime) {
        float v = listTime / minTime;
        v = 1.f - (1.f - v) * 0.6f;
        v = std::max(v, 0.3f);
        speed *= v;
    }
    speedList.push_back(speed);
    for (;;) {
        if (speedList.size() > 2) {
            speedList.pop_front();
        } else {
            break;
        }
    }
    float avgSpeed = 0.f;
    for (auto s: speedList) {
        avgSpeed += s;
    }
    avgSpeed = avgSpeed / (float) speedList.size();
    if (avgSpeed < 0.9f) {
        reachMinTimeCount++;
    }
    if (avgSpeed > 0.98f && avgSpeed < 1.02f) {
        reachNormalTimeCount++;
    }
    if (reachMinTimeCount >= 3) {
        reachMinTimeCount = 0;
        cacheCurTime *= 1.3f;
        cacheCurTime = std::min(cacheCurTime, 1500.f);
    }
    if (reachNormalTimeCount >= 20) {
        reachNormalTimeCount = 0;
        reachMinTimeCount = 0;
        if (cacheCurTime + 20.f > cacheSetTime) {
            cacheCurTime -= (cacheCurTime - cacheSetTime) * 0.2f;
        }
    }
    return avgSpeed;
}


int Audio::open_stream(AVFormatContext *fmt_ctx) {
    stream_id = -1;
    int ret = open_codec_context(&stream_id, &av_dec_ctx,
                                 fmt_ctx, AVMEDIA_TYPE_AUDIO);
    LOGI("stream_id: %d", stream_id);
    if (ret >= 0) {
        av_stream = fmt_ctx->streams[stream_id];
    } else {
        stream_id = -1;
    }
    if (av_stream && isSampleRateValid(av_dec_ctx->sample_rate)
        && av_dec_ctx->sample_fmt == AV_SAMPLE_FMT_S16) {
        pktDuration = 1024000.f / (float) av_dec_ctx->sample_rate;
        int channels = av_dec_ctx->ch_layout.nb_channels;
        LOGI("Audio -ac %d -ar %d byte %d fmt_name:%s", channels,
             av_dec_ctx->sample_rate,
             av_get_bytes_per_sample(av_dec_ctx->sample_fmt),
             av_get_sample_fmt_name(av_dec_ctx->sample_fmt));
        playAudio = new AudioPlay(av_dec_ctx->sample_rate, channels);
        playAudio->Start();
        pthread_create(&p_audio_tid, nullptr, audioProcess, this);
    } else {
        stream_id = -1;
        LOGI("isSampleRateValid: %d AV_SAMPLE_FMT_S16: %d", av_dec_ctx->sample_rate,
             (av_dec_ctx->sample_fmt == AV_SAMPLE_FMT_S16));
    }
    return ret;
}

void Audio::release() {
    LOGI("audio pthread_join wait");
    if (playAudio) {
        playAudio->Stop();
    }
    pthread_mutex_lock(&c_mutex);
    thread_finish = true;
    pthread_cond_signal(&c_cond);
    pthread_mutex_unlock(&c_mutex);
    if (p_audio_tid) {
        pthread_join(p_audio_tid, 0);
    }
    clearList();
    delete playAudio;
    LOGI("audio pthread_join done");
    if (av_dec_ctx) {
        avcodec_free_context(&av_dec_ctx);
        av_dec_ctx = nullptr;
    }
    stream_id = 0;
    av_stream = nullptr;
    av_dec_ctx = nullptr;
    LOGI("audio Release");
}

bool Audio::isSampleRateValid(int sampleRate) {
    bool success = false;
    switch (sampleRate) {
        case 8000:
            success = true;
            break;
        case 11025:
            success = true;
            break;
        case 16000:
            success = true;
            break;
        case 22050:
            success = true;
            break;
        case 24000:
            success = true;
            break;
        case 32000:
            success = true;
            break;
        case 44100:
            success = true;
            break;
        case 48000:
            success = true;
            break;
        case 64000:
            success = true;
            break;
        case 88200:
            success = true;
            break;
        case 96000:
            success = true;
            break;
        case 192000:
            success = true;
            break;
    }
    return success;
}