//
// Created by Administrator on 2019/9/12.
//

#include "Audio.h"

int Audio::must_feed = 0;
pthread_mutex_t Audio::a_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t Audio::a_cond = PTHREAD_COND_INITIALIZER;

Audio::Audio(UpdateTimeFun *fun) {
    LOGI("Audio must_feed: %d ",must_feed)
    must_feed = 0;
    updateTimeFun = fun;
}

int Audio::synchronize_audio(int nb_samples) {
    int wanted_nb_samples;
    double diff_ms;
    double nb_time_ms;
//    LOGI("get_audio_clock: %f get_master_clock: %f nb_samples: %d", get_audio_clock(), get_master_clock(), nb_samples);
    diff_ms = get_audio_pts_clock() - get_master_clock();
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

/**
 * 为了实现双缓冲
 */
uint8_t **Audio::getDstData() {
    if (chooseDstData) {
        chooseDstData = false;
        return &dst_data_1;
    } else {
        chooseDstData = true;
        return &dst_data_2;
    }
}

void Audio::slBufferCallback() {
//    LOGI("slBufferCallback start")
    pthread_mutex_lock(&a_mutex);
    if(must_feed != -1){
        must_feed = 1;
        pthread_cond_signal(&a_cond); //通知
        pthread_cond_wait(&a_cond, &a_mutex); // 等待回调
    }
    pthread_mutex_unlock(&a_mutex);
    //在这里之后必须要有数据
    LOGI("slBufferCallback end")
}


void *Audio::audioProcess(void *arg) {
    int ret = 0;
    int wanted_nb_samples = 0;
    bool checkout_time = false;
    uint8_t **cur_dst_data;
    FPacket *audio_packet = NULL;
    Audio *audio = (Audio *) arg;
    AVFrame *frame = av_frame_alloc();
    if (audio->updateTimeFun) {
        audio->updateTimeFun->jvm_attach_fun();
    }
    LOGI("audioProcess: run!!!")
    while (true) {
        do {
//            LOGI("audio_pkt_list size: %d", audio_pkt_list.size())
            pthread_mutex_lock(&c_mutex);
            if (!audio_pkt_list.empty()) {
                audio_packet = audio_pkt_list.front();
                audio_pkt_list.pop_front();
                if (audio_pkt_list.size() <= 1) {
                    pthread_cond_broadcast(&c_cond);
                }
            } else {
                if (!audio->thread_finish) {
                    pthread_cond_broadcast(&c_cond);
                    pthread_cond_wait(&audio_cond, &c_mutex);
                } else {
                    LOGI("audioProcess: end")
                    pthread_mutex_unlock(&c_mutex);
                    goto end; // 线程不再运行，结束
                }
                if (!audio_pkt_list.empty()) {
                    audio_packet = audio_pkt_list.front();
                    audio_pkt_list.pop_front();
                } else {
                    if (audio->thread_finish) {
                        LOGI("audioProcess: end")
                        pthread_mutex_unlock(&c_mutex);
                        goto end;// 线程不再运行，结束
                    }
                }
            }
            pthread_mutex_unlock(&c_mutex);
        } while (audio_packet == NULL);
        if (audio_packet->checkout_time) {
            checkout_time = true;
            pthread_mutex_lock(&seek_mutex);
            avcodec_flush_buffers(audio->av_dec_ctx);
            swr_convert(audio->swr_context, frame->data, frame->sample_rate, NULL, 0);
            pthread_mutex_unlock(&seek_mutex);
        }
        // 解码
//        pthread_mutex_lock(&seek_mutex);
        ret = avcodec_send_packet(audio->av_dec_ctx, audio_packet->avPacket);
//        pthread_mutex_unlock(&seek_mutex);
        if (ret < 0) {
            LOGE("Error audio sending a packet for decoding audio_pkt_list size: %d", audio_pkt_list.size());
            av_packet_free(&audio_packet->avPacket);
            free_packet(audio_packet);
            pthread_mutex_lock(&c_mutex);
            crash_error = true;
            pthread_cond_signal(&c_cond);
            pthread_mutex_unlock(&c_mutex);
            LOGE("Error Audio end");
            break;
        }
        while (ret >= 0) {
//            audio->test_audio_time = (av_gettime_relative() / 1000.0);
            ret = avcodec_receive_frame(audio->av_dec_ctx, frame);
//            LOGI("avcodec_receive_frame cast time: %f",  ((av_gettime_relative() / 1000.0) - audio->test_audio_time));

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
            double pts = av_q2d(audio->av_stream->time_base) * frame->pts * 1000.0;
            if (checkout_time) {
                checkout_time = false;
                pthread_mutex_lock(&seek_mutex);
                audio_seeking = false;
//                if (stream_idx == -1) { // 校准时间
//                    double time = av_gettime_relative() / 1000.0;
//                    set_master_clock(time - pts);
//                }
                pthread_mutex_unlock(&seek_mutex);
            }

            if (audio_seeking) {
//                LOGE("check_audio_is_seek copy_pkg:%p", audio_packet);
                continue;
            }


//            LOGI("audio pts: %f cast time: %f", pts, ((av_gettime_relative() / 1000.0) - audio->test_audio_time));
//            audio->test_audio_time = (av_gettime_relative() / 1000.0);

            ff_sec_time = (int32_t) (pts / 1000);
            if (ff_last_sec_time != ff_sec_time && audio->updateTimeFun) {
                audio->updateTimeFun->update_time_fun();
            }
//            LOGI("updateTimeFun cast time: %f",  ((av_gettime_relative() / 1000.0) - audio->test_audio_time));

            ff_last_sec_time = ff_sec_time;

            wanted_nb_samples = frame->nb_samples;
            cur_dst_data = audio->getDstData();
            ret = swr_convert(audio->swr_context, cur_dst_data, wanted_nb_samples,
                              (const uint8_t **) frame->data, frame->nb_samples);

            pthread_mutex_lock(&a_mutex);
            if (must_feed == 0) {
                pthread_cond_wait(&a_cond, &a_mutex); // 等待回调
            }
            must_feed = 0;
            // 到这里必须要有sl数据
            set_audio_clock(pts);// ms
//            LOGI("audio pts: %f get_master_clock: %f", pts, get_master_clock());
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
//                                         av_dec_ctx->sample_rate / frame->sample_rate),
//                                         wanted_nb_samples *
//                                         av_dec_ctx->sample_rate / frame->sample_rate) < 0) {
//                    LOGE("swr_set_compensation() failed");
//                    continue;
//                }
//            }
            if (ret > 0) {
                audio->openSL.setEnqueueBuffer(*cur_dst_data, (uint32_t) ret * 4);
//                LOGI("swr_convert len: %d wanted_nb_samples: %d", ret, wanted_nb_samples);
            } else {
                LOGE("swr_convert err = %d", ret);
            }
            pthread_cond_signal(&a_cond);
            pthread_mutex_unlock(&a_mutex);

            pthread_mutex_lock(&audio->pause_mutex);
            if (audio->is_pause) {
                pthread_cond_wait(&audio->pause_cond, &audio->pause_mutex);
            }
            pthread_mutex_unlock(&audio->pause_mutex);
        }
        av_packet_free(&audio_packet->avPacket);
        free_packet(audio_packet);
        audio_packet = NULL;
    }
    end:
    if (audio->updateTimeFun) {
        audio->updateTimeFun->jvm_detach_fun();
    }
    av_frame_free(&frame);
    LOGI("audioProcess end audio_pkt_list size: %d", audio_pkt_list.size())
    return 0;
}

int Audio::open_stream() {
    static SLConfigure slConfigure;
    stream_id = -1;
    pthread_cond_init(&audio_cond, NULL);
    pthread_cond_init(&pause_cond, NULL);
    pthread_mutex_init(&pause_mutex, NULL);
    int ret = open_codec_context(&stream_id, &av_dec_ctx,
                                 fmt_ctx, AVMEDIA_TYPE_AUDIO);
    LOGI("stream_id: %d", stream_id);
    if (ret >= 0) {
        av_stream = fmt_ctx->streams[stream_id];
        int out_sample_rate = av_dec_ctx->sample_rate;
        LOGI("ffplay -ac %d -ar %d byte %d fmt_name:%s", av_dec_ctx->channels, av_dec_ctx->sample_rate,
             av_get_bytes_per_sample(av_dec_ctx->sample_fmt), av_get_sample_fmt_name(av_dec_ctx->sample_fmt));
        int sr = openSL.getSupportSampleRate(av_dec_ctx->sample_rate);
        if (!sr) {
            out_sample_rate = 44100;
        }
        int len_1 = av_samples_alloc(&dst_data_1, NULL,
                                     av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO),
                                     out_sample_rate,
                                     AV_SAMPLE_FMT_S16, 1);
        av_samples_alloc(&dst_data_2, NULL,
                                     av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO),
                                     out_sample_rate,
                                     AV_SAMPLE_FMT_S16, 1);
        LOGI("out_sample_rate: %d s: %d nb_channels:%d", out_sample_rate, len_1,
             av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO));
        slConfigure.channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
        slConfigure.sampleRate = out_sample_rate;
        slConfigure.signSlBufferCallback = slBufferCallback;
        openSL.createPlayer(&slConfigure);
        swr_context = swr_alloc();
        swr_alloc_set_opts(swr_context,
                           AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, out_sample_rate,
                           av_dec_ctx->channel_layout, av_dec_ctx->sample_fmt, av_dec_ctx->sample_rate,
                           1, NULL);
        ret = swr_init(swr_context);
        if (ret == 0) {
            LOGI("swr_init success create Thread");
            openSL.play();
            pthread_create(&p_audio_tid, NULL, audioProcess, this);
        } else {
            stream_id = -1;
            LOGE("swr_init failed");
        }
    }else {
        stream_id = -1;
    }
    return ret;
}

void Audio::pause() {
    pthread_mutex_lock(&pause_mutex);
    is_pause = true;
    pthread_mutex_unlock(&pause_mutex);
}

void Audio::resume() {
    LOGI("Audio resume start")
    pthread_mutex_lock(&pause_mutex);
    is_pause = false;
    pthread_cond_signal(&pause_cond);
    pthread_mutex_unlock(&pause_mutex);
    LOGI("Audio resume end")
}

void Audio::release() {
    LOGI("audio pthread_join wait");
    pthread_mutex_lock(&c_mutex);
    thread_finish = true;
    pthread_cond_broadcast(&audio_cond);
    pthread_cond_signal(&c_cond);
    pthread_mutex_unlock(&c_mutex);
    if(p_audio_tid){
        pthread_join(p_audio_tid, 0);
    }
    LOGI("audio openSL.unlock");
    pthread_mutex_lock(&a_mutex);
    must_feed = -1;
    pthread_cond_broadcast(&a_cond);
    pthread_mutex_unlock(&a_mutex);
    LOGI("audio openSL.release");
    // 内部必定有等待 slBufferCallback结束的，应该是让内部线程结束，所以释放之前要确保不堵塞它的线程
    // 先解锁，再释放
    openSL.release();
    LOGI("audio pthread_join done");
    if (swr_context != NULL) {
        swr_free(&swr_context);
        swr_context = NULL;
    }
    if (dst_data_1 != NULL) {
        av_freep(&dst_data_1);
        dst_data_1 = NULL;
    }
    if (dst_data_2 != NULL) {
        av_freep(&dst_data_2);
        dst_data_2 = NULL;
    }
    if (av_dec_ctx) {
        avcodec_free_context(&av_dec_ctx);
        av_dec_ctx = NULL;
    }
    pthread_cond_destroy(&audio_cond);
    pthread_mutex_destroy(&pause_mutex);
    pthread_cond_destroy(&pause_cond);
    stream_id = 0;
    av_stream = NULL;
    av_dec_ctx = NULL;
    LOGI("audio release");
}