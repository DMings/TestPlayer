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

struct FPacket {
    AVPacket *avPacket;
//    bool is_seek;
    bool checkout_time;
};

typedef void (*update_time)();

typedef void (*jvm_attach)();

typedef void (*jvm_detach)();

struct UpdateTimeFun {
    jvm_attach jvm_attach_fun;
    jvm_detach jvm_detach_fun;
    update_time update_time_fun;
};

extern FPacket *alloc_packet();

extern void free_packet(FPacket *packet);

extern int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx,
                              AVFormatContext *fmt_ctx, AVMediaType type);

extern double get_master_clock();

extern double get_video_pts_clock();

extern void set_video_clock(double pts);

extern double get_video_clock();

extern double get_audio_clock();

extern double get_audio_pts_clock();

extern void set_audio_clock(double pts);

extern void set_master_clock(double time);

extern void seek_frame(float percent);

extern AVFormatContext *fmt_ctx;

extern pthread_cond_t c_cond;
extern pthread_cond_t video_cond;
extern pthread_mutex_t c_mutex;
extern pthread_mutex_t seek_mutex;
extern pthread_cond_t audio_cond;

extern std::list<FPacket *> audio_pkt_list;
extern std::list<FPacket *> video_pkt_list;

extern void decode_packet(AVPacket *pkt, int audio_stream_id, int video_stream_id);

extern bool audio_seeking;
extern bool video_seeking;

extern void seek_frame_if_need();

extern bool crash_error;

extern int32_t ff_sec_time;

extern int32_t ff_last_sec_time;

extern int32_t ff_sec_duration;

extern void clearAllList();

extern void ff_init();

extern void ff_release();

#endif //TESTPLAYER_FFAV_H
