//
// Created by Administrator on 2019/9/12.
//

#include "Video2.h"

Video::Video(UpdateTimeFun *fun) {
    updateTimeFun = fun;
}

int Video::synchronize_video(double pkt_duration) { // us
    double wanted_delay = -1;
    double diff_ms;
    double duration;
    bool is_seeking;
    AVRational rational = video_stream->r_frame_rate;
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
        if (audio_stream_id != -1) {
            diff_ms = get_video_pts_clock() - get_audio_clock();
        } else {
            diff_ms = get_video_pts_clock() - get_master_clock();
        }
    } else {
        diff_ms = duration * 0.7;
    }
    if (fabs(diff_ms) >= duration * 2) {
        double time = av_gettime_relative() / 1000.0;
        set_master_clock(time - get_video_pts_clock());
        return (uint) (duration * 0.7);
    }
    if (diff_ms < 0) { // 视频落后时间
        if (duration > 0 && wanted_delay < duration * 0.3) {
            wanted_delay = 0;
        }
    } else { // 视频超过时间
        wanted_delay = diff_ms * 0.7;
    }
//    LOGI("video diff_ms: %f wanted_delay: %f pkt_duration: %f get_video_pts_clock: %f", diff_ms, wanted_delay,pkt_duration,get_video_pts_clock());
    return (uint) wanted_delay;
}

void *Video::videoProcess(void *arg) {
    AVFrame *frame = av_frame_alloc();
    int ret = 0;
    bool checkout_time = false;
    Video *video = (Video *) arg;
    FPacket *video_packet = NULL;
    video->openGL.createEgl(video->mWindow, NULL, video_dec_ctx->width, video_dec_ctx->height);
    if (audio_stream_id == -1 && video->updateTimeFun) {
        video->updateTimeFun->jvm_attach_fun();
    }
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
            avcodec_flush_buffers(video_dec_ctx);
        }

//        LOGE("video copy_pkg:%d", video_packet);
        // 解码
        pthread_mutex_lock(&seek_mutex);
        ret = avcodec_send_packet(video_dec_ctx, video_packet->avPacket);
        pthread_mutex_unlock(&seek_mutex);
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
            pthread_mutex_lock(&seek_mutex);
            ret = avcodec_receive_frame(video_dec_ctx, frame);
            pthread_mutex_unlock(&seek_mutex);
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
            //  用 st 上的时基才对 video_stream
            double pts = (int64_t) (av_q2d(video_stream->time_base) * frame->pts * 1000.0); // ms
            if (checkout_time) {
                checkout_time = false;
                pthread_mutex_lock(&seek_mutex);
                video_seeking = false;
                if (audio_stream_id == -1) { // 校准时间
                    double time = av_gettime_relative() / 1000.0;
                    set_master_clock(time - pts);
                }
                pthread_mutex_unlock(&seek_mutex);
            }
//            LOGI("video pts: %f get_master_clock: %f", pts, get_master_clock());
            if (video_seeking) {
                LOGE("check_video_is_seek copy_pkg:%lld", video_packet);
                continue;
            }
            double pkt_duration = (int64_t) (av_q2d(video_stream->time_base) * frame->pkt_duration * 1000.0); // ms
            ret = sws_scale(video->sws_context,
                            (const uint8_t *const *) frame->data, frame->linesize,
                            0, video_dec_ctx->height,
                            video->dst_data, video->dst_line_size);
//            LOGI("video pts: %f get_audio_clock: %f get_master_clock: %f pkt_duration: %f", pts,
//                 get_audio_clock(),
//                 get_master_clock(),
//                 pkt_duration);
            set_video_clock(pts);
            if (audio_stream_id == -1) { // 更新当前时间
                ff_time = (int64_t) (pts);
                if (video->updateTimeFun) {
                    video->updateTimeFun->update_time_fun();
                }
            }
            int delay = video->synchronize_video(pkt_duration); // ms
//            LOGI("video delay->%d get_master_clock: %f video_time: %f", delay, get_master_clock(),
//                 (get_master_clock() - video->test_video_time));
            if (delay >= 0) {
                av_usleep((uint) delay * 1000); // us
//                LOGI("video delay->%d get_master_clock: %f video_time: %f", delay, get_master_clock(),
//                     (get_master_clock() - video->test_video_time));
            }
            video->test_video_time = get_master_clock();

            pthread_mutex_lock(&c_mutex);
            if (video->will_update_surface) {
                video->will_update_surface = false;
                video->openGL.updateEgl(video->mWindow);
            }
            pthread_mutex_unlock(&c_mutex);

            video->openGL.draw(video->dst_data[0]);

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
    if (audio_stream_id == -1 && video->updateTimeFun) {
        video->updateTimeFun->jvm_detach_fun();
    }
    av_frame_free(&frame);
    video->openGL.release();
    LOGE("video videoProcess");
    LOGE("video_pkt_list size: %d", video_pkt_list.size())
    return 0;
}

int Video::open_stream(ANativeWindow *window) {
    mWindow = window;
    int ret = open_codec_context(&video_stream_id, &video_dec_ctx,
                                 fmt_ctx, AVMEDIA_TYPE_VIDEO);
    if (ret >= 0) {
        video_stream = fmt_ctx->streams[video_stream_id];
        /* allocate image where the decoded image will be put */
        LOGI("ffplay -f rawvideo -pix_fmt %s -video_size %d x %d",
             av_get_pix_fmt_name(video_dec_ctx->pix_fmt), video_dec_ctx->width, video_dec_ctx->height);
    } else {
        video_stream_id = -1;
    }
    pthread_cond_init(&video_cond, NULL);
    pthread_cond_init(&pause_cond, NULL);
    pthread_mutex_init(&pause_mutex, NULL);
    if (video_stream) {
        AVRational rational = video_stream->r_frame_rate;
        LOGI("rational: %d => %d", rational.den, rational.num);
        sws_context = sws_getContext(
                video_dec_ctx->width, video_dec_ctx->height, video_dec_ctx->pix_fmt,
                video_dec_ctx->width, video_dec_ctx->height, AV_PIX_FMT_RGBA,
                SWS_BILINEAR, NULL, NULL, NULL);
        if ((ret = av_image_alloc(dst_data, dst_line_size,
                                  video_dec_ctx->width, video_dec_ctx->height,
                                  AV_PIX_FMT_RGBA, 1)) < 0) {
            LOGE("Could not allocate destination image: %d", ret);
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
    pthread_mutex_lock(&pause_mutex);
    is_pause = false;
    pthread_cond_signal(&pause_cond);
    pthread_mutex_unlock(&pause_mutex);
}


void Video::update_surface(ANativeWindow *window) {
    mWindow = window;
    pthread_mutex_lock(&c_mutex);
    will_update_surface = true;
    pthread_mutex_unlock(&c_mutex);
}

void Video::release() {
    if (video_stream_id != -1) {
        LOGI("video pthread_join wait");
        pthread_mutex_lock(&c_mutex);
        thread_finish = true;
        pthread_cond_signal(&video_cond);
        pthread_mutex_unlock(&c_mutex);
        pthread_join(p_video_tid, 0);
        LOGI("video pthread_join done");
        av_freep(&dst_data[0]);
        sws_freeContext(sws_context);
    }
    updateTimeFun = NULL;
    pthread_cond_destroy(&pause_cond);
    pthread_cond_destroy(&video_cond);
    pthread_mutex_destroy(&pause_mutex);
    if (video_dec_ctx) {
        avcodec_free_context(&video_dec_ctx);
    }
}