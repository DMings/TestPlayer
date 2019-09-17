//
// Created by Administrator on 2019/9/12.
//

#include "Audio2.h"

bool Audio::must_feed;
pthread_mutex_t Audio::a_mutex;
pthread_cond_t Audio::a_cond;

int Audio::synchronize_audio(int nb_samples) {
    int wanted_nb_samples;
    double diff_ms;
    double nb_time_ms;
//    LOGI("get_audio_clock: %f get_master_clock: %f nb_samples: %d", get_audio_clock(), get_master_clock(), nb_samples);
    diff_ms = get_audio_pts_clock() - get_master_clock();
    nb_time_ms = 1.0 * nb_samples / audio_dec_ctx->sample_rate * 1000; // ms
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

void Audio::slBufferCallback() {
    pthread_mutex_lock(&a_mutex);
    must_feed = true;
    pthread_cond_signal(&a_cond); //通知
    pthread_cond_wait(&a_cond, &a_mutex); // 等待回调
    pthread_mutex_unlock(&a_mutex);
    //在这里之后必须要有数据
}


void *Audio::audioProcess(void *arg) {
    int ret = 0;
    int wanted_nb_samples = 0;
    Audio *audio = (Audio *) arg;
    AVFrame *frame = av_frame_alloc();
    while (audio->thread_flag) {
        do {
            LOGI("audio_pkt_list size: %d", audio_pkt_list.size())
            pthread_mutex_lock(&c_mutex);
            if (!audio_pkt_list.empty()) {
                audio_packet = audio_pkt_list.front();
                audio_pkt_list.pop_front();
                if (audio_pkt_list.size() <= 1) {
                    pthread_cond_broadcast(&c_cond);
                }
            } else {
                pthread_cond_broadcast(&c_cond);
                pthread_cond_wait(&c_cond, &c_mutex);
                if (!audio_pkt_list.empty()) {
                    audio_packet = audio_pkt_list.front();
                    audio_pkt_list.pop_front();
                }
            }
            pthread_mutex_unlock(&c_mutex);
        } while (audio_packet == NULL && audio->thread_flag);
        if(audio_packet == NULL){
            break;
        }
        // 解码
        ret = avcodec_send_packet(audio_dec_ctx, audio_packet->avPacket);
        if (ret < 0) {
            LOGE("Error audio sending a packet for decoding");
            break;
        }
        if(audio_packet->checkout_time){ // 刷新
            avcodec_flush_buffers(audio_dec_ctx);
        }
        while (ret >= 0) {
            ret = avcodec_receive_frame(audio_dec_ctx, frame);
            if (ret == AVERROR(EAGAIN)) {
//                LOGE("ret == AVERROR(EAGAIN)");
                break;
            } else if (ret == AVERROR_EOF || ret == AVERROR(EINVAL) || ret == AVERROR_INPUT_CHANGED) {
                LOGE("audio some err!");
                break;
            } else if (ret < 0) {
                LOGE("audio legitimate decoding errors");
                break;
            }
            if (check_audio_is_seek()) {
                continue;
            }
            pthread_mutex_lock(&a_mutex);
            if (!must_feed && audio->thread_flag) {
                pthread_cond_wait(&a_cond, &a_mutex); // 等待回调
            }
            must_feed = false;

            // 到这里必须要有sl数据
            double pts = av_q2d(audio_stream->time_base) * frame->pts * 1000.0;
            set_audio_clock(pts);// ms
            if(audio_packet->checkout_time){
                LOGE("audio pts: checkout_time------------");
                want_audio_seek = false;
            }
            LOGI("audio pts: %f get_master_clock: %f", pts,
                 get_master_clock());
            wanted_nb_samples = frame->nb_samples;
//            wanted_nb_samples = synchronize_audio(frame->nb_samples);
//            if (!test) {
//                test = true;
//                wanted_nb_samples = 1024;
//            } else {
//                test = false;
//                wanted_nb_samples = 768;
//            }
//            wanted_nb_samples = 768;
//            if (wanted_nb_samples != last_wanted_nb_samples) {  // 没有这个，输入通道数量不会变
//                last_wanted_nb_samples = wanted_nb_samples;
//                LOGE("wanted_nb_samples != frame->nb_samples %d", wanted_nb_samples);
//                if (swr_set_compensation(swr_context,
//                                         (int) (1.0 * (wanted_nb_samples - frame->nb_samples) *
//                                         audio_dec_ctx->sample_rate / frame->sample_rate),
//                                         wanted_nb_samples *
//                                         audio_dec_ctx->sample_rate / frame->sample_rate) < 0) {
//                    LOGE("swr_set_compensation() failed");
//                    continue;
//                }
//            }
            ret = swr_convert(audio->swr_context, &audio->dst_data, wanted_nb_samples,
                              (const uint8_t **) frame->data, frame->nb_samples);
            if (ret > 0) {
                audio->openSL.setEnqueueBuffer(audio->dst_data, (uint32_t) ret * 4);
                pthread_cond_signal(&a_cond);
//                LOGI("swr_convert len: %d wanted_nb_samples: %d", ret, wanted_nb_samples);
            } else {
                LOGE("swr_convert err = %d", ret);
            }
            pthread_mutex_unlock(&a_mutex);
        }
        pthread_mutex_lock(&c_mutex);
        av_packet_free(&audio_packet->avPacket);
        free_packet(audio_packet);
        audio_packet = NULL;
        pthread_mutex_unlock(&c_mutex);
    }
    LOGI("pthread_join return");
    av_frame_free(&frame);
    return 0;
}

int Audio::open_stream() {
    static SLConfigure slConfigure;
    int ret = open_codec_context(&audio_stream_id, &audio_dec_ctx,
                                 fmt_ctx, AVMEDIA_TYPE_AUDIO);
    if (ret >= 0 & audio_stream_id != -1) {
        audio_stream = fmt_ctx->streams[audio_stream_id];
    }
    if (audio_stream) {
        int out_sample_rate = audio_dec_ctx->sample_rate;
        LOGI("ffplay -ac %d -ar %d byte %d fmt_name:%s", audio_dec_ctx->channels, audio_dec_ctx->sample_rate,
             av_get_bytes_per_sample(audio_dec_ctx->sample_fmt), av_get_sample_fmt_name(audio_dec_ctx->sample_fmt));
        int sr = openSL.getSupportSampleRate(audio_dec_ctx->sample_rate);
        if (!sr) {
            out_sample_rate = 44100;
        }
        int len = av_samples_alloc(&dst_data, NULL,
                                   av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO),
                                   out_sample_rate,
                                   AV_SAMPLE_FMT_S16, 1);
        LOGI("out_sample_rate: %d s: %d nb_channels:%d", out_sample_rate, len,
             av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO));
        slConfigure.channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
        slConfigure.sampleRate = out_sample_rate;
        slConfigure.signSlBufferCallback = slBufferCallback;
        openSL.createPlayer(&slConfigure);
        swr_context = swr_alloc();
        swr_alloc_set_opts(swr_context,
                           AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, out_sample_rate,
                           audio_dec_ctx->channel_layout, audio_dec_ctx->sample_fmt, audio_dec_ctx->sample_rate,
                           1, NULL);
        ret = swr_init(swr_context);
        if (ret == 0) {
            LOGI("swr_init success create Thread");
            pthread_mutex_init(&a_mutex, NULL);
            pthread_cond_init(&a_cond, NULL);
            openSL.play();
            pthread_create(&p_audio_tid, NULL, audioProcess, this);
        } else {
            audio_stream_id = -1;
            LOGE("swr_init failed");
        }
    }
    return ret;
}

void Audio::release() {
    if (audio_stream_id != -1) {
        thread_flag = false;
        LOGI("audio pthread_join wait");
        pthread_mutex_lock(&a_mutex);
        openSL.pause();
        pthread_cond_broadcast(&a_cond);
        pthread_mutex_unlock(&a_mutex);
        pthread_mutex_lock(&c_mutex);
        pthread_cond_broadcast(&c_cond);
        pthread_mutex_unlock(&c_mutex);
        pthread_join(p_audio_tid, 0);
        LOGI("audio pthread_join done");
        swr_free(&swr_context);
        av_freep(&dst_data);
    }
    if (audio_dec_ctx) {
        avcodec_free_context(&audio_dec_ctx);
    }
    LOGI("audio openSL.release()");
    openSL.release();
    pthread_mutex_destroy(&a_mutex);
    pthread_cond_destroy(&a_cond);
    LOGI("audio openSL.release()----");
}