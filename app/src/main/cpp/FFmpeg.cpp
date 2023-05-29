//
// Created by DMing on 2019/9/14.
//

#include "FFmpeg.h"

AVFormatContext *fmt_ctx = NULL;

pthread_cond_t c_cond;
pthread_mutex_t c_mutex;
pthread_mutex_t seek_mutex;
pthread_cond_t audio_cond;
pthread_cond_t video_cond;

std::list<FPacket *> audio_pkt_list;
std::list<FPacket *> video_pkt_list;

Clock master_clk = {0};
Clock video_clk = {0};
Clock audio_clk = {0};

bool audio_seeking = false;
bool video_seeking = false;
bool want_seek = false;

bool want_audio_seek_inner = false;
bool want_video_seek_inner = false;

double seek_time = 0;

bool crash_error = false;

int32_t ff_sec_time = 0; // 当前时间
int32_t ff_last_sec_time = 0; // 上次当前时间，用于减少call java次数
int32_t ff_sec_duration = 0; // 总时间

int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx,
                       AVFormatContext *fmt_ctx, enum AVMediaType type) {
    int ret, stream_index;
    const AVCodec *dec = NULL;
    AVDictionary *opts = NULL;
    AVStream *st;
    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        LOGI("Could not find %s stream in input file", av_get_media_type_string(type));
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            LOGI("Failed to find %s codec",
                 av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }
        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            LOGI("Failed to allocate the %s codec context",
                 av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
            LOGI("Failed to copy %s codec parameters to decoder context",
                 av_get_media_type_string(type));
            return ret;
        }
        if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0) {
            LOGI("Failed to open %s codec",
                 av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
        LOGI("(*dec_ctx) => %d %d", (*dec_ctx)->time_base.num, (*dec_ctx)->time_base.den);
    }
    return 0;
}

double get_master_clock() {
    double time = av_gettime_relative() / 1000.0;
    if (master_clk.last_updated == 0) {
        master_clk.last_updated = time;
    }
    return time - master_clk.last_updated; // ms
}

void set_master_clock(double time) {
    if (time < 0) {
        master_clk.last_updated = 0;
    }
    master_clk.last_updated = time;
}

double get_video_pts_clock() {
    return video_clk.pts; // ms
}

void set_video_clock(double pts) {
    double time = av_gettime_relative() / 1000.0;
    video_clk.pts = pts; // ms
    video_clk.last_updated = time;
}

double get_video_clock() {
    return video_clk.pts; // ms
}

double get_audio_clock() {
    if (audio_clk.pts == 0) {
        return 0;
    }
    double time = av_gettime_relative() / 1000.0;
    if (audio_clk.last_updated == 0) {
        audio_clk.last_updated = time;
    }
    return audio_clk.pts + (time - audio_clk.last_updated); // ms
}

double get_audio_pts_clock() {
    return audio_clk.pts; // ms
}

void set_audio_clock(double pts) {
    double time = av_gettime_relative() / 1000.0;
    audio_clk.pts = pts; // ms
    audio_clk.last_updated = time;
}

void decode_packet(AVPacket *pkt, int audio_stream_id, int video_stream_id) {
    if (pkt->stream_index == video_stream_id || pkt->stream_index == audio_stream_id) {
        if (pkt->stream_index == video_stream_id) {
            LOGI("video pkt: %d pkt->pts: %ld pts: %ld", pkt->stream_index, pkt->pts,
                 av_rescale_q(pkt->pts, AV_TIME_BASE_Q, pkt->time_base));
        } else {
            LOGI("audio pkt: %d pkt->pts: %ld pts: %ld", pkt->stream_index, pkt->pts,
                 av_rescale_q(pkt->pts, AV_TIME_BASE_Q, pkt->time_base));
        }
        return;
    }


    FPacket *f_pkt = nullptr;
    if (pkt->stream_index == video_stream_id || pkt->stream_index == audio_stream_id) {
        AVPacket *pkg = av_packet_alloc();
        av_packet_move_ref(pkg, pkt);
        f_pkt = alloc_packet();
        pthread_mutex_lock(&c_mutex);
        f_pkt->avPacket = pkg;
        if (want_audio_seek_inner && pkt->stream_index == audio_stream_id) { //
            want_audio_seek_inner = false;
            f_pkt->checkout_time = true;
        } else if (want_video_seek_inner && pkt->stream_index == video_stream_id) { //
            want_video_seek_inner = false;
            f_pkt->checkout_time = true;
            if (audio_stream_id == -1) {
                audio_seeking = false;
            }
        }
        pthread_mutex_unlock(&c_mutex);
    }
    if (f_pkt == nullptr) {
        return;
    }
//    LOGE("seek_frame222 audio: %d video: %d copy_pkg:%d",(pkt->stream_index == audio_stream_id),(pkt->stream_index == video_stream_id),copy_pkg);
    pthread_mutex_lock(&c_mutex);
    //
    if (pkt->stream_index == video_stream_id) {
        video_pkt_list.push_back(f_pkt);
        pthread_cond_signal(&video_cond); // 谁的数据通知谁
    } else if (pkt->stream_index == audio_stream_id) {
        audio_pkt_list.push_back(f_pkt);
        pthread_cond_signal(&audio_cond);
    }
    //
    if (video_stream_id != -1 && audio_stream_id != -1) {
        if (video_pkt_list.size() >= 200 && audio_pkt_list.size() >= 200) {
            pthread_cond_wait(&c_cond, &c_mutex);
        }
    } else if (video_stream_id != -1) {
        if (video_pkt_list.size() >= 200) {
            pthread_cond_wait(&c_cond, &c_mutex);
        }
    } else if (audio_stream_id != -1) {
        if (audio_pkt_list.size() >= 200) {
            pthread_cond_wait(&c_cond, &c_mutex);
        }
    }
    pthread_mutex_unlock(&c_mutex);
}

FPacket *alloc_packet() {
    FPacket *packet = (struct FPacket *) malloc(sizeof(struct FPacket));
    packet->checkout_time = false;
//    packet->is_seek = false;
    return packet;
}

void free_packet(FPacket *packet) {
    free(packet);
}

void clearAllList() {
    std::list<FPacket *>::iterator it;
    for (it = audio_pkt_list.begin(); it != audio_pkt_list.end();) {
        av_packet_free(&(*it)->avPacket);
        free_packet(*it);
        audio_pkt_list.erase(it++);
    }
    for (it = video_pkt_list.begin(); it != video_pkt_list.end();) {
        av_packet_free(&(*it)->avPacket);
        free_packet(*it);
        video_pkt_list.erase(it++);
    }
}

int seek_frame_if_need() {
    int ret = 0;
    pthread_mutex_lock(&c_mutex);
    if (want_seek) {
        clearAllList();
//        LOGE("seek_frame_if_need---------------------------------------------->");
        want_seek = false;
        want_audio_seek_inner = true;
        want_video_seek_inner = true;
        pthread_mutex_lock(&seek_mutex);
//        avformat_flush(fmt_ctx);
//        avcodec_flush_buffers(video_dec_ctx);
//        if (audio_dec_ctx != NULL) {
//            avcodec_flush_buffers(audio_dec_ctx);
//        }
//        | AVSEEK_FLAG_ANY
        if (av_seek_frame(fmt_ctx, AVMEDIA_TYPE_UNKNOWN, (int64_t) (seek_time),
                          AVSEEK_FLAG_BACKWARD)) {
            avformat_flush(fmt_ctx);
        }
        pthread_mutex_unlock(&seek_mutex);
        ret = 1;
    }
    pthread_mutex_unlock(&c_mutex);
    return ret;
}

void seek_frame(float percent) {
    pthread_mutex_lock(&seek_mutex);
    video_seeking = true;
    audio_seeking = true;
    pthread_mutex_unlock(&seek_mutex);
    pthread_mutex_lock(&c_mutex);
//    LOGI("seek_frame------------------------------------>v %d a %d", video_seeking, audio_seeking);
    want_seek = true;
    seek_time = percent * fmt_ctx->duration;
    pthread_mutex_unlock(&c_mutex);
}

void ff_init() {
    set_master_clock(0);
    crash_error = false;
    pthread_mutex_init(&seek_mutex, NULL);
}

void ff_release() {
    clearAllList();
    fmt_ctx = NULL;
    pthread_mutex_destroy(&seek_mutex);
    pthread_cond_destroy(&c_cond);
    pthread_mutex_destroy(&c_mutex);
}
