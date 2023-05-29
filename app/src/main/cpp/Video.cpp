//
// Created by Administrator on 2019/9/12.
//

#include "Video.h"

PthreadSleep Video::pthread_sleep;

Video::Video(GLThread* glThread,UpdateTimeFun *fun) {
    this->glThread=  glThread;
    dst_data[0] = NULL;
    updateTimeFun = fun;
}

Video::~Video() {
}

uint Video::synchronize_video(double pkt_duration) { // us
    uint wanted_delay = 0;
    double diff_ms;
    double duration;
    bool is_seeking;
    AVRational rational = av_stream->r_frame_rate;
    int video_base = (int) 1000.0 * rational.den / rational.num;
    if (video_base) {
        duration = video_base;
    } else {
        duration = pkt_duration;
    }
    duration = duration < 200 ? duration : 200;
    pthread_mutex_lock(&seek_mutex);
    is_seeking = audio_seeking;
    pthread_mutex_unlock(&seek_mutex);

    if (!is_seeking) {
        if (has_audio) {
            diff_ms = get_video_pts_clock() - get_audio_clock();
        } else {
            diff_ms = get_video_pts_clock() - get_master_clock();
        }
    } else { // 在seek的时候，音频是不可靠的，音频也在变，这时候尽量丢弃，反正都是没用的
        diff_ms = duration * 0.3;
    }

    if (!has_audio && fabs(diff_ms) >= duration * 2) {  // 当只有视频的时候，已经明显失去了同步，校准一下
        double time = av_gettime_relative() / 1000.0;
        set_master_clock(time - get_video_pts_clock());
        return (uint) (duration * 0.7);
    }

    if (diff_ms < 0) { // 视频落后时间
        if (fabs(diff_ms) < duration * 0.1) { // 如果差距比较少就不管了
            wanted_delay = 0;
        } else {
            wanted_delay = (uint) (duration * 0.1); // 减少时间，尽量追上
        }
    } else { // 视频超过时间
        if (fabs(diff_ms) < duration * 0.15) { // 如果差距比较少就不管了
            wanted_delay = 0;
        } else { // 增加时间，尽量等待时间追上
            if (diff_ms < duration * 0.8) {
                wanted_delay = (uint) (diff_ms * 0.8);
            } else {
                wanted_delay = (uint) (duration * 0.8); // 最大不超过，不然卡顿非常明显
            }
        }
    }
//    LOGI("video diff_ms: %f wanted_delay: %f pkt_duration: %f get_video_pts_clock: %f", diff_ms, wanted_delay,pkt_duration,get_video_pts_clock());
    return wanted_delay;
}

void *Video::videoProcess(void *arg) {
    AVFrame *frame = av_frame_alloc();
    int ret = 0;
    bool checkout_time = false;
    Video *video = (Video *) arg;
    FPacket *video_packet = NULL;
    video->glThread->setParams(video->dst_data[0],video->av_dec_ctx->width, video->av_dec_ctx->height);
    if (!video->has_audio && video->updateTimeFun) {
        video->updateTimeFun->jvm_attach_fun();
    }
    LOGI("videoProcess: run!!!");
    pthread_sleep.reset();
    while (true) {
        do {
//            LOGE("video_pkt_list size: %d", video_pkt_list.size())
            pthread_mutex_lock(&c_mutex);
            video_packet = NULL;
            if (!video_pkt_list.empty()) {
                video_packet = video_pkt_list.front();
                video_pkt_list.pop_front();
                if (video_pkt_list.size() <= 1) {
                    pthread_cond_signal(&c_cond);
                }
            } else {
                if (!video->thread_finish) {
                    pthread_cond_signal(&c_cond);
                    pthread_cond_wait(&video_cond, &c_mutex);
                } else {
                    pthread_mutex_unlock(&c_mutex);
                    goto end; // 线程不再运行，结束
                }
                if (!video_pkt_list.empty()) {
                    video_packet = video_pkt_list.front();
                    video_pkt_list.pop_front();
                } else {
                    if (video->thread_finish) {
                        pthread_mutex_unlock(&c_mutex);
                        goto end;// 线程不再运行，结束
                    }
                }
            }
            pthread_mutex_unlock(&c_mutex);
        } while (video_packet == NULL);

        if (video_packet->checkout_time) {
            checkout_time = true;
            avcodec_flush_buffers(video->av_dec_ctx);
        }

//        LOGE("video copy_pkg:%p %X", video_packet);
        // 解码
//        pthread_mutex_lock(&seek_mutex);
        ret = avcodec_send_packet(video->av_dec_ctx, video_packet->avPacket);
//        pthread_mutex_unlock(&seek_mutex);
        if (ret < 0) {
            LOGE("Error video sending a packet for decoding video_pkt_list size: %d", video_pkt_list.size());
            av_packet_free(&video_packet->avPacket);
            free_packet(video_packet);
            pthread_mutex_lock(&c_mutex);
            crash_error = true;
            pthread_cond_signal(&c_cond);
            pthread_mutex_unlock(&c_mutex);
            LOGE("Error video end");
            break;
        }
        while (ret >= 0) {
//            pthread_mutex_lock(&seek_mutex);
            ret = avcodec_receive_frame(video->av_dec_ctx, frame);
//            pthread_mutex_unlock(&seek_mutex);
            if (ret == AVERROR(EAGAIN)) {
//                LOGE("ret == AVERROR(EAGAIN)");
                break;
            } else if (ret == AVERROR_EOF || ret == AVERROR(EINVAL) || ret == AVERROR_INPUT_CHANGED) {
                LOGE("video some err!");
                break;
            } else if (ret < 0) {
                LOGE("video legitimate decoding errors");
                break;
            }
            //  用 st 上的时基才对 av_stream
            double pts = (int64_t) (av_q2d(video->av_stream->time_base) * frame->pts * 1000.0); // ms
            if (checkout_time) {
                checkout_time = false;
                pthread_mutex_lock(&seek_mutex);
                video_seeking = false;
                if (!video->has_audio) { // 校准时间
                    double time = av_gettime_relative() / 1000.0;
                    set_master_clock(time - pts);
                }
                pthread_mutex_unlock(&seek_mutex);
            }
//            LOGI("video pts: %f get_master_clock: %f", pts, get_master_clock());
            if (video_seeking) {
//                LOGE("check_video_is_seek copy_pkg:%p", video_packet);
                continue;
            }
            double pkt_duration = (int64_t) (av_q2d(video->av_stream->time_base) * frame->pkt_duration * 1000.0); // ms
            video->glThread->lockDraw();
            ret = sws_scale(video->sws_context,
                            (const uint8_t *const *) frame->data, frame->linesize,
                            0, video->av_dec_ctx->height,
                            video->dst_data, video->dst_line_size); // lock
            video->glThread->unlockDraw();

//            LOGI("video pts: %f get_audio_clock: %f get_master_clock: %f pkt_duration: %f", pts,
//                 get_audio_clock(),
//                 get_master_clock(),
//                 pkt_duration);
            set_video_clock(pts);
            if (!video->has_audio) { // 更新当前时间
                ff_sec_time = (int32_t) (pts / 1000);
                if (ff_last_sec_time != ff_sec_time && video->updateTimeFun) {
                    video->updateTimeFun->update_time_fun();
                }
                ff_last_sec_time = ff_sec_time;
            }
            int delay = video->synchronize_video(pkt_duration); // ms
//            LOGI("video delay->%d pts: %f get_master_clock: %f video_time: %f", delay, pts, get_master_clock(),
//                 (get_master_clock() - video->test_video_time));
            if (delay >= 0) {
//                av_usleep((uint) delay * 1000); // us
                pthread_sleep.msleep((uint) delay);
//                LOGI("video delay->%d get_master_clock: %f video_time: %f", delay, get_master_clock(),
//                     (get_master_clock() - video->test_video_time));
            }
//            video->test_video_time = get_master_clock();

            video->glThread->draw();

            pthread_mutex_lock(&video->pause_mutex);
            if (video->is_pause) {
                pthread_cond_wait(&video->pause_cond, &video->pause_mutex);
            }
            pthread_mutex_unlock(&video->pause_mutex);

        }
        av_packet_free(&video_packet->avPacket);
        free_packet(video_packet);
    }
    end:
    video->glThread->setParams(NULL,0, 0);
    video->glThread->drawBackground();
    if (!video->has_audio && video->updateTimeFun) {
        video->updateTimeFun->jvm_detach_fun();
    }
    av_frame_free(&frame);
    LOGI("videoProcess end video_pkt_list size: %d", video_pkt_list.size());
    return 0;
}


int Video::open_stream(bool hasAudio) {
    has_audio = hasAudio;
    pthread_cond_init(&video_cond, NULL);
    pthread_cond_init(&pause_cond, NULL);
    pthread_mutex_init(&pause_mutex, NULL);
    int ret = open_codec_context(&stream_id, &av_dec_ctx,
                                 fmt_ctx, AVMEDIA_TYPE_VIDEO);
    if (ret >= 0) {
//        int nb_cpu_s = av_cpu_count();
//        if (nb_cpu_s == 0) {
//            nb_cpu_s = 8;
//        }
//        av_dec_ctx->thread_count = nb_cpu_s;
//        LOGI("nb_cpu_s: %d", nb_cpu_s);
        av_stream = fmt_ctx->streams[stream_id];
        LOGI("ffplay -f rawvideo -pix_fmt %s -video_size %d x %d",
             av_get_pix_fmt_name(av_dec_ctx->pix_fmt), av_dec_ctx->width, av_dec_ctx->height);
    } else {
        stream_id = -1;
    }
    if (av_stream) {
        AVRational rational = av_stream->r_frame_rate;
        LOGI("rational: %d => %d", rational.den, rational.num);
        sws_context = sws_getContext(
                av_dec_ctx->width, av_dec_ctx->height, av_dec_ctx->pix_fmt,
                av_dec_ctx->width, av_dec_ctx->height, AV_PIX_FMT_RGBA,
                SWS_BILINEAR, NULL, NULL, NULL);
        if ((ret = av_image_alloc(dst_data, dst_line_size,
                                  av_dec_ctx->width, av_dec_ctx->height,
                                  AV_PIX_FMT_RGBA, 1)) < 0) {
            LOGE("Could not allocate destination image: %d", ret);
            stream_id = -1;
        } else {
            LOGI("dst_data size: %d", ret);
            pthread_create(&p_video_tid, 0, Video::videoProcess, this);
        }
    }
    return ret;
}

void Video::pause() {
    pthread_mutex_lock(&pause_mutex);
    is_pause = true;
    pthread_mutex_unlock(&pause_mutex);
}

void Video::resume() {
    LOGI("Video resume start");
    pthread_mutex_lock(&pause_mutex);
    is_pause = false;
    pthread_cond_signal(&pause_cond);
    pthread_mutex_unlock(&pause_mutex);
    LOGI("Video resume end");
}

void Video::release() {
    LOGI("video pthread_join wait");
    pthread_sleep.interrupt();
    pthread_mutex_lock(&c_mutex);
    thread_finish = true;
    pthread_cond_signal(&video_cond);
    pthread_cond_signal(&c_cond);
    pthread_mutex_unlock(&c_mutex);
    if (p_video_tid) {
        pthread_join(p_video_tid, 0);
    }
    LOGI("video pthread_join done");
    if (dst_data[0] != NULL) {
        av_freep(&dst_data[0]);
        dst_data[0] = NULL;
    }
    if (sws_context != NULL) {
        sws_freeContext(sws_context);
        sws_context = NULL;
    }
    pthread_cond_destroy(&pause_cond);
    pthread_cond_destroy(&video_cond);
    pthread_mutex_destroy(&pause_mutex);
    if (av_dec_ctx) {
//        avcodec_close(av_dec_ctx);
        avcodec_free_context(&av_dec_ctx);
    }
    updateTimeFun = NULL;
    stream_id = 0;
    has_audio = false;
    av_stream = NULL;
    av_dec_ctx = NULL;
    LOGI("video release");
}