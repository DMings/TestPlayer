//
// Created by Administrator on 2019/9/12.
//

#include "Video.h"

PthreadSleep Video::pthread_sleep;

Video::Video(GLThread *glThread, AVClock *avClock) : avClock(avClock) {
    this->glThread = glThread;
    dst_data[0] = nullptr;
    thread_finish = false;
}

Video::~Video() {
}

uint Video::synchronize_video(double pkt_duration) { // us
    uint wanted_delay = 0;
    double diff_ms;
    double duration;
    AVRational rational = av_stream->r_frame_rate;
    int video_base = (int) 1000.0 * rational.den / rational.num;
    if (video_base) {
        duration = video_base;
    } else {
        duration = pkt_duration;
    }
    duration = duration < 200 ? duration : 200;

    diff_ms = avClock->get_video_pts_clock() - avClock->get_audio_clock();

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

void Video::putAvPacket(FPacket *pkt) {
    if (stream_id != -1) {
        pthread_mutex_lock(&c_mutex);
        pkt_list.push_back(pkt);
        pthread_cond_signal(&c_cond);
        pthread_mutex_unlock(&c_mutex);
    }
}

uint64_t Video::getAvPacketSize() {
    uint64_t size;
    pthread_mutex_lock(&c_mutex);
    size = pkt_list.size();
    pthread_mutex_unlock(&c_mutex);
    return size;
}

void *Video::videoProcess(void *arg) {
    AVFrame *frame = av_frame_alloc();
    int ret = 0;
    Video *video = (Video *) arg;
    FPacket *video_packet = nullptr;
    video->glThread->setParams(video->dst_data[0], video->av_dec_ctx->width,
                               video->av_dec_ctx->height);
    LOGI("videoProcess: run!!!");
    pthread_sleep.reset();
    while (!video->thread_finish) {
        do {
            video_packet = nullptr;
            pthread_mutex_lock(&video->c_mutex);
            if (!video->pkt_list.empty()) {
                video_packet = video->pkt_list.front();
                video->pkt_list.pop_front();
            } else {
                pthread_cond_wait(&video->c_cond, &video->c_mutex);
                if (!video->pkt_list.empty()) {
                    video_packet = video->pkt_list.front();
                    video->pkt_list.pop_front();
                }
            }
            pthread_mutex_unlock(&video->c_mutex);
        } while (video_packet == nullptr && !video->thread_finish);
        if (video_packet != nullptr) {
            if (!video->thread_finish) {
                if (video_packet->checkout_time) {
                    avcodec_flush_buffers(video->av_dec_ctx);
                }
                ret = avcodec_send_packet(video->av_dec_ctx, video_packet->avPacket);
                if (ret < 0) {
                    LOGE("Error video sending a packet for decoding video->pkt_list size: %d",
                         video->pkt_list.size());
                    av_packet_free(&video_packet->avPacket);
                    free_packet(video_packet);
                    LOGE("Error video end");
                    break;
                }
                while (ret >= 0) {
                    ret = avcodec_receive_frame(video->av_dec_ctx, frame);
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
                    //  用 st 上的时基才对 av_stream
                    double pts = (int64_t) (av_q2d(video->av_stream->time_base) * frame->pts *
                                            1000.0); // ms

//                LOGI("video pts: %f get_master_clock: %f", pts, video->avClock->get_master_clock());
                    double pkt_duration = (int64_t) (av_q2d(video->av_stream->time_base) *
                                                     frame->pkt_duration * 1000.0); // ms
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
                    video->avClock->set_video_clock(pts);
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
            }
            av_packet_free(&video_packet->avPacket);
            free_packet(video_packet);
        }
    }
    end:
    video->glThread->setParams(nullptr, 1, 1);
    video->glThread->drawBackground();
    av_frame_free(&frame);
    LOGI("videoProcess end video->pkt_list size: %ld", video->pkt_list.size());
    return 0;
}


int Video::open_stream(AVFormatContext *fmt_ctx) {
    pthread_cond_init(&pause_cond, nullptr);
    pthread_mutex_init(&pause_mutex, nullptr);
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
        LOGI("Video -pix_fmt %s -video_size %d x %d",
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
                SWS_BILINEAR, nullptr, nullptr, nullptr);
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
    LOGI("Video Resume Start");
    pthread_mutex_lock(&pause_mutex);
    is_pause = false;
    pthread_cond_signal(&pause_cond);
    pthread_mutex_unlock(&pause_mutex);
    LOGI("Video Resume end");
}

void Video::release() {
    LOGI("video pthread_join wait");
    pthread_sleep.interrupt();
    pthread_mutex_lock(&c_mutex);
    thread_finish = true;
    pthread_cond_signal(&c_cond);
    pthread_mutex_unlock(&c_mutex);
    if (p_video_tid) {
        pthread_join(p_video_tid, 0);
    }
    clearList();
    LOGI("video pthread_join done");
    if (dst_data[0] != nullptr) {
        av_freep(&dst_data[0]);
        dst_data[0] = nullptr;
    }
    if (sws_context != nullptr) {
        sws_freeContext(sws_context);
        sws_context = nullptr;
    }
    pthread_cond_destroy(&pause_cond);
    pthread_mutex_destroy(&pause_mutex);
    if (av_dec_ctx) {
//        avcodec_close(av_dec_ctx);
        avcodec_free_context(&av_dec_ctx);
    }
    stream_id = 0;
    av_stream = nullptr;
    av_dec_ctx = nullptr;
    LOGI("video Release");
}