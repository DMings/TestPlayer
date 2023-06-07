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

uint Video::synchronize_video(int64_t lastPts, int64_t pktDuration) { // ms
    uint wanted_delay = 0;
    int64_t diff_ms;
    int64_t duration;
    AVRational rational = av_stream->r_frame_rate;
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

    if (avClock->GetAudioPtsClock() == INT64_MIN) {
        diff_ms = avClock->GetVideoPtsClock() - lastPts;
    } else {
        diff_ms = (avClock->GetVideoPtsClock() - avClock->GetAudioPtsClock()) / 1000;
    }

    if (diff_ms < 0) {
        wanted_delay = 0;
    } else {
        wanted_delay = std::min((int64_t) ((float) duration * 1.5f), diff_ms);
    }
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
    auto *video = (Video *) arg;
    FPacket *video_packet = nullptr;
    video->glThread->setParams(video->dst_data[0], video->av_dec_ctx->width,
                               video->av_dec_ctx->height);
    LOGI("videoProcess: run!!!");
    size_t listSize = 0;
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
            listSize = video->pkt_list.size();
            pthread_mutex_unlock(&video->c_mutex);
        } while (video_packet == nullptr && !video->thread_finish);
        if (video_packet != nullptr) {
            if (!video->thread_finish) {
                if (video_packet->checkout_time) {
                    avcodec_flush_buffers(video->av_dec_ctx);
                }
                ret = avcodec_send_packet(video->av_dec_ctx, video_packet->avPacket);
                if (ret < 0) {
                    LOGE("Error video sending a packet for decoding video->pkt_list size: %ld",
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
                    int64_t pts = av_rescale_q(frame->pts,
                                               video->av_stream->time_base,
                                               AV_TIME_BASE_Q);// us

                    int64_t pkt_duration = av_rescale_q(frame->duration,
                                                        video->av_stream->time_base,
                                                        AV_TIME_BASE_Q);// us
                    video->glThread->lockDraw();
                    ret = sws_scale(video->sws_context,
                                    (const uint8_t *const *) frame->data, frame->linesize,
                                    0, video->av_dec_ctx->height,
                                    video->dst_data, video->dst_line_size); // lock
                    video->glThread->unlockDraw();

                    video->avClock->SetVideoClock(pts);

                    int delay = video->synchronize_video(video->lastPts / 1000,
                                                         pkt_duration / 1000); // ms
//                    LOGI("audio GetAudioPtsClock: %ld GetVideoPtsClock: %ld delay: %d real diff: %ld listSize: %ld",
//                         video->avClock->GetAudioPtsClock() / 1000,
//                         video->avClock->GetVideoPtsClock() / 1000,
//                         delay,
//                         (video->avClock->GetVideoPtsClock() - video->avClock->GetAudioPtsClock()) /
//                         1000,
//                         listSize);
                    video->lastPts = pts;
                    pthread_sleep.sleep((uint) delay);
                    video->glThread->draw();
                }
            }
            av_packet_free(&video_packet->avPacket);
            free_packet(video_packet);
        }
    }
    video->glThread->setParams(nullptr, 1, 1);
    video->glThread->drawBackground();
    av_frame_free(&frame);
    LOGI("videoProcess end video->pkt_list size: %ld", video->pkt_list.size());
    return nullptr;
}


int Video::open_stream(AVFormatContext *fmt_ctx) {
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

void Video::release() {
    LOGI("video pthread_join wait");
    pthread_sleep.interrupt();
    pthread_mutex_lock(&c_mutex);
    thread_finish = true;
    pthread_cond_signal(&c_cond);
    pthread_mutex_unlock(&c_mutex);
    if (p_video_tid) {
        pthread_join(p_video_tid, nullptr);
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
    if (av_dec_ctx) {
//        avcodec_close(av_dec_ctx);
        avcodec_free_context(&av_dec_ctx);
    }
    stream_id = 0;
    av_stream = nullptr;
    av_dec_ctx = nullptr;
    LOGI("video Release");
}