//
// Created by odm on 2023/6/1.
//
#include "BaseAV.h"
#include "../utils/FLog.h"

BaseAV::BaseAV() : cCond_(), cMutex_(), pktList_() {
    pthread_mutex_init(&cMutex_, nullptr);
    pthread_cond_init(&cCond_, nullptr);
}

int BaseAV::OpenCodecContext(int *stream_idx, AVCodecContext **dec_ctx,
                             AVFormatContext *fmt_ctx, enum AVMediaType type) {
    int ret, stream_index;
    const AVCodec *dec = nullptr;
    AVDictionary *opts = nullptr;
    AVStream *st;
    ret = av_find_best_stream(fmt_ctx, type, -1, -1, nullptr, 0);
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
        LOGI("OpenCodecContext type: %d => %d %d", type, (*dec_ctx)->time_base.num,
             (*dec_ctx)->time_base.den);
    }
    return 0;
}

void BaseAV::ClearList() {
    std::list<FPacket *>::iterator it;
    for (it = pktList_.begin(); it != pktList_.end();) {
        av_packet_free(&(*it)->avPacket);
        free_packet(*it);
        pktList_.erase(it++);
    }
}

BaseAV::~BaseAV() {
    pthread_cond_destroy(&cCond_);
}
