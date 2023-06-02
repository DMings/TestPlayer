//
// Created by Administrator on 2019/9/12.
//

#include "Audio.h"

Audio::Audio(AVClock *avClock) : avClock(avClock), pause_cond(), pause_mutex() {
    thread_finish = false;
    playAudio = nullptr;
}

int Audio::synchronize_audio(int nb_samples) {
    int wanted_nb_samples;
    double diff_ms;
    double nb_time_ms;
//    LOGI("get_audio_clock: %f get_master_clock: %f nb_samples: %d", get_audio_clock(), get_master_clock(), nb_samples);
    diff_ms = avClock->get_audio_pts_clock() - avClock->get_master_clock();
    nb_time_ms = 1.0 * nb_samples / av_dec_ctx->sample_rate * 1000; // ms
//    LOGI("diff_ms: %f nb_time_ms: %f", diff_ms, nb_time_ms);
    if (fabs(diff_ms) > nb_time_ms * 0.2) { // 超过阀值
        if (diff_ms < 0) { // 音频落后时间
            if (-diff_ms < nb_time_ms) { // 音频比较慢，要丢弃帧，加快下一次pts超过时基
                wanted_nb_samples = (int) ((1 + diff_ms / nb_time_ms) * nb_samples);
            } else {
                wanted_nb_samples = 0;
            }
//            LOGI("wanted_nb_samples: %d diff_ms / nb_time_ms: %.6f,nb_samples: %d", wanted_nb_samples,(-diff_ms / nb_time_ms),nb_samples);
        } else { // 音频超过时间
            if (diff_ms < nb_time_ms) { // 音频比较快，要增加帧，减慢下一次的pts,让时基跟上
                wanted_nb_samples = (int) (diff_ms / nb_time_ms * nb_samples + nb_samples);
            } else {
                wanted_nb_samples = nb_samples + nb_samples;
            }
        }
        if (wanted_nb_samples < 0.2 * nb_samples) {  // 给上阀值，不能少于某一个采样率
            wanted_nb_samples = (int) (0.2 * nb_samples);
        }
    } else {
        wanted_nb_samples = nb_samples;
    }
//    wanted_nb_samples = ((wanted_nb_samples & 1) == 0) ? wanted_nb_samples : (wanted_nb_samples + 1);
    LOGI("diff_ms: %f wanted_nb_samples: %d", diff_ms, wanted_nb_samples);
    return wanted_nb_samples;
//    min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
//    max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
//    wanted_nb_samples = av_clip(wanted_nb_samples, min_nb_samples, max_nb_samples);
}


void Audio::putAvPacket(FPacket *pkt) {
    if (stream_id != -1) {
        pthread_mutex_lock(&c_mutex);
        pkt_list.push_back(pkt);
        pthread_cond_signal(&c_cond);
        pthread_mutex_unlock(&c_mutex);
    }
}

void *Audio::audioProcess(void *arg) {
    int ret = 0;
    int wanted_nb_samples = 0;
    FPacket *audio_packet = nullptr;
    auto *audio = (Audio *) arg;
    AVFrame *frame = av_frame_alloc();
    LOGI("audioProcess: run!!!");
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
            pthread_mutex_unlock(&audio->c_mutex);
        } while (audio_packet == nullptr && !audio->thread_finish);
        if (audio_packet != nullptr) {
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
                double pts = av_q2d(audio->av_stream->time_base) * frame->pts * 1000.0;

                LOGI("audio pts: %f get_master_clock: %f", pts, audio->avClock->get_master_clock());

                audio->avClock->ff_sec_time = (int32_t) (pts / 1000);
//            LOGI("updateTimeFun cast time: %f",  ((av_gettime_relative() / 1000.0) - audio->test_audio_time));

                wanted_nb_samples = frame->nb_samples;
                if (audio->playAudio) {
                    audio->playAudio->PutData(frame->data[0],
                                              frame->nb_samples *
                                              audio->av_dec_ctx->ch_layout.nb_channels * 2);
                }

                // 到这里必须要有sl数据
                audio->avClock->set_audio_clock(pts);// ms

                pthread_mutex_lock(&audio->pause_mutex);
                if (audio->is_pause) {
                    pthread_cond_wait(&audio->pause_cond, &audio->pause_mutex);
                }
                pthread_mutex_unlock(&audio->pause_mutex);
            }
            av_packet_free(&audio_packet->avPacket);
            free_packet(audio_packet);
        }
    }
    end:
    av_frame_free(&frame);
    LOGI("audioProcess end pkt_list size: %ld", audio->pkt_list.size());
    return 0;
}

int Audio::open_stream(AVFormatContext *fmt_ctx) {
    stream_id = -1;
    pthread_cond_init(&pause_cond, nullptr);
    pthread_mutex_init(&pause_mutex, nullptr);
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

void Audio::pause() {
    pthread_mutex_lock(&pause_mutex);
    is_pause = true;
    pthread_mutex_unlock(&pause_mutex);
}

void Audio::resume() {
    LOGI("Audio Resume Start");
    pthread_mutex_lock(&pause_mutex);
    is_pause = false;
    pthread_cond_signal(&pause_cond);
    pthread_mutex_unlock(&pause_mutex);
    LOGI("Audio Resume end");
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
    pthread_cond_destroy(&c_cond);
    pthread_mutex_destroy(&pause_mutex);
    pthread_cond_destroy(&pause_cond);
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