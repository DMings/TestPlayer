//
// Created by Administrator on 2019/9/12.
//

#include "FPlayer.h"

AVFormatContext *FPlayer::fmt_ctx = NULL;

int FPlayer::video_stream_id = -1;
int FPlayer::audio_stream_id = -1;

pthread_cond_t FPlayer::c_cond;
pthread_mutex_t FPlayer::c_mutex;

std::list<AVPacket *> FPlayer::audio_pkt_list;
std::list<AVPacket *> FPlayer::video_pkt_list;

Clock FPlayer::master_clk;

int FPlayer::open_codec_context(int *stream_idx, AVCodecContext **dec_ctx,
                                AVFormatContext *fmt_ctx, enum AVMediaType type) {
    int ret, stream_index;
    AVCodec *dec = NULL;
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

double FPlayer::get_master_clock() {
    double time = av_gettime_relative() / 1000.0;
    if (master_clk.last_updated == 0) {
        master_clk.last_updated = time;
    }
    return time - master_clk.last_updated; // ms
}

void FPlayer::decode_packet(AVPacket *pkt, bool clear_cache) {
    const char *stream_id = "-";
    if (clear_cache) {
        pkt->data = NULL;
        pkt->size = 0;
    }
    if (pkt->stream_index == video_stream_id) {
        stream_id = "video";
        AVPacket *copy_pkg = av_packet_clone(pkt);
        if (copy_pkg != NULL) {
            video_pkt_list.push_back(copy_pkg);
        }
    } else if (pkt->stream_index == audio_stream_id) {
        stream_id = "audio";
        AVPacket *copy_pkg = av_packet_clone(pkt);
        if (copy_pkg != NULL) {
            audio_pkt_list.push_back(copy_pkg);
        }
    }
    LOGI("stream_id ->: %s", stream_id)
    pthread_mutex_lock(&c_mutex);
    pthread_cond_broadcast(&c_cond);
    if (video_stream_id != -1 && audio_stream_id != -1) {
        if (video_pkt_list.size() >= 2 && audio_pkt_list.size() >= 1) {
            pthread_cond_wait(&c_cond, &c_mutex);
        }
    } else if (video_stream_id != -1) {
        if (video_pkt_list.size() >= 2) {
            pthread_cond_wait(&c_cond, &c_mutex);
        }
    }
    if (audio_stream_id != -1) {
        if (audio_pkt_list.size() >= 1) {
            pthread_cond_wait(&c_cond, &c_mutex);
        }
    }
    pthread_mutex_unlock(&c_mutex);
}

void FPlayer::startPlayer(const char *src_filename, ANativeWindow *window) {
    AVPacket pkt;
    Audio audio;
    Video video;

    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
        LOGI("Could not open source file %s", src_filename);
        exit(1);
    }
    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        LOGI("Could not find stream information");
        exit(1);
    }

    pthread_mutex_init(&c_mutex, NULL);
    pthread_cond_init(&c_cond, NULL);
    // ->>
    if (audio.open_stream() < 0 && video.open_stream(window) < 0) {
        LOGI("Could not find audio or video stream in the input, aborting");
        goto end;
    }

    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        decode_packet(&pkt, false);
    }
    LOGI("flush cached frames.");
    decode_packet(&pkt, true);
    LOGI("Demuxing succeeded.");
    end:
    //
    audio.release();
    video.release();
    avformat_close_input(&fmt_ctx);
    pthread_cond_destroy(&c_cond);
    pthread_mutex_destroy(&c_mutex);
}