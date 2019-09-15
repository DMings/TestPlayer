//
// Created by DMing on 2019/9/14.
//

#ifndef TESTPLAYER_FFAV_H
#define TESTPLAYER_FFAV_H


#include "log.h"
#include <pthread.h>
#include <list>
#include <android/native_window.h>
#include <android/native_window_jni.h>

extern "C" {
#include <libavutil/log.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>
#include <libavutil/samplefmt.h>
#include <libavutil/time.h>
#include <libavcodec/avcodec.h>
}

struct Clock {
    double pts;           /* clock base */
    double last_updated;
};

extern void startPlayer(const char *src_filename, ANativeWindow *window);

extern int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx,
                              AVFormatContext *fmt_ctx, AVMediaType type);

extern double get_master_clock();

extern AVFormatContext *fmt_ctx;

extern int video_stream_id;
extern int audio_stream_id;

extern pthread_cond_t c_cond;
extern pthread_mutex_t c_mutex;

extern std::list<AVPacket *> audio_pkt_list;
extern std::list<AVPacket *> video_pkt_list;


extern void decode_packet(AVPacket *pkt, bool clear_cache);

extern Clock master_clk;


#endif //TESTPLAYER_FFAV_H
