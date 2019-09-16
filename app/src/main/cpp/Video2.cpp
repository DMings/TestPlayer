//
// Created by Administrator on 2019/9/12.
//

#include "Video2.h"

int Video::synchronize_video(double pkt_duration) { // us
    double wanted_delay = -1;
    double diff_ms;
    double duration;
    int video_base = (int) av_q2d(video_stream->r_frame_rate) * 1000;
    if (video_base) {
        duration = video_base;
    } else {
        duration = pkt_duration;
    }
    duration = duration < 200 ? duration : 200;
//    pthread_mutex_lock(&c_mutex);
    if (!video_packet->no_checkout_time) {
        if (audio_stream_id != -1) {
            diff_ms = get_video_pts_clock() - get_audio_clock();
        } else {
            diff_ms = get_video_pts_clock() - get_master_clock();
        }
    }else {
        diff_ms = duration;
    }
//    pthread_mutex_unlock(&c_mutex);
    if (diff_ms < 0) { // 视频落后时间
        if (duration > 0 && wanted_delay < duration * 0.3) {
            wanted_delay = 0;
        }
    } else { // 视频超过时间
        wanted_delay = diff_ms;
    }
//    LOGI("video diff_ms: %f wanted_delay: %f pkt_duration: %f get_video_pts_clock: %f", diff_ms, wanted_delay,pkt_duration,get_video_pts_clock());
    return (uint) wanted_delay;
}

void *Video::videoProcess(void *arg) {
    AVFrame *frame = av_frame_alloc();
    int ret = 0;
    Video *video = (Video *) arg;
    int texture = video->openGL.init(video->mWindow, NULL, video->video_dec_ctx->width, video->video_dec_ctx->height);
    LOGI("texture: %d", texture)
    while (video->thread_flag) {
        do {
            LOGE("video_pkt_list size: %d", video_pkt_list.size())
            pthread_mutex_lock(&c_mutex);
            video_packet = NULL;
            if (!video_pkt_list.empty()) {
                video_packet = video_pkt_list.front();
                video_pkt_list.pop_front();
                if (video_pkt_list.size() <= 1) {
                    pthread_cond_broadcast(&c_cond);
                }
            } else {
                pthread_cond_broadcast(&c_cond);
                pthread_cond_wait(&c_cond, &c_mutex);
                if (!video_pkt_list.empty()) {
                    video_packet = video_pkt_list.front();
                    video_pkt_list.pop_front();
                }
            }
            pthread_mutex_unlock(&c_mutex);
        } while (video_packet == NULL);
        // 解码
        ret = avcodec_send_packet(video->video_dec_ctx, video_packet->avPacket);
        if (ret < 0) {
            LOGE("Error video sending a packet for decoding");
            pthread_mutex_lock(&c_mutex);
            av_packet_free(&video_packet->avPacket);
            free_packet(video_packet);
            pthread_mutex_unlock(&c_mutex);
            continue;
        }
//        if (!check_video_is_seek()) {
            while (ret >= 0) {
                ret = avcodec_receive_frame(video->video_dec_ctx, frame);
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
                double pkt_duration = (int64_t) (av_q2d(video_stream->time_base) * frame->pkt_duration *
                                                 1000.0); // ms
                ret = sws_scale(video->sws_context,
                                (const uint8_t *const *) frame->data, frame->linesize,
                                0, video->video_dec_ctx->height,
                                video->dst_data, video->dst_line_size);
                LOGI("video pts: %f get_audio_clock: %f get_master_clock: %f pkt_duration: %f", pts,
                     get_audio_clock(),
                     get_master_clock(),
                     pkt_duration);
                set_video_clock(pts);
                int delay = video->synchronize_video(pkt_duration); // ms
//            LOGI("video show-> width: %d height: %d pts: %f delay: %f pkt_duration: %f", frame->width, frame->height,
//                 pts, delay, pkt_duration);
                if (delay >= 0) {
                    av_usleep((uint) delay * 1000); // us
                    LOGI("video delay->%d get_master_clock: %f video_time: %f", delay, get_master_clock(),
                         (get_master_clock() - video->test_video_time));
                } else {
                    LOGI("video delay->skip get_master_clock: %f video_time: %f", get_master_clock(),
                         (get_master_clock() - video->test_video_time));
                }
                video->test_video_time = get_master_clock();
//            LOGI("height: %d video_dec_ctx->height %d",ret,video_dec_ctx->height);
                video->openGL.draw(video->dst_data[0]);
//            env->CallVoidMethod(updateObject, updateMethod);
            }
//        }
        pthread_mutex_lock(&c_mutex);
        av_packet_free(&video_packet->avPacket);
        free_packet(video_packet);
        pthread_mutex_unlock(&c_mutex);
    }
    av_frame_free(&frame);
    video->openGL.release();
//    env->DeleteGlobalRef(initObject);
//    env->DeleteGlobalRef(updateObject);
//    jvm->DetachCurrentThread();
    return 0;
}

int Video::open_stream(ANativeWindow *window) {
    mWindow = window;
    int ret = open_codec_context(&video_stream_id, &video_dec_ctx,
                                 fmt_ctx, AVMEDIA_TYPE_VIDEO);
    if (ret >= 0 && video_stream_id != -1) {
        video_stream = fmt_ctx->streams[video_stream_id];
        /* allocate image where the decoded image will be put */
        LOGI("ffplay -f rawvideo -pix_fmt %s -video_size %d x %d",
             av_get_pix_fmt_name(video_dec_ctx->pix_fmt), video_dec_ctx->width, video_dec_ctx->height);
    }
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

void Video::release() {
    if (video_stream_id != -1) {
        pthread_join(p_video_tid, 0);
        av_freep(&dst_data[0]);
        sws_freeContext(sws_context);
    }
    avcodec_free_context(&video_dec_ctx);
}