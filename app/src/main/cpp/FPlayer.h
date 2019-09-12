//
// Created by Administrator on 2019/9/12.
//

#ifndef TESTPLAYER_FFUTILS_H
#define TESTPLAYER_FFUTILS_H

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
}

struct Clock {
    double pts;           /* clock base */
    double last_updated;
};

#include "Audio.h"
#include "Video.h"

class FPlayer {
public:
    static void startPlayer(const char *src_filename, ANativeWindow *window);

    static int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx,
                           AVFormatContext *fmt_ctx, AVMediaType type);

    static double get_master_clock();

    static AVFormatContext *fmt_ctx;

    static int video_stream_id;
    static int audio_stream_id;

    static pthread_cond_t c_cond;
    static pthread_mutex_t c_mutex;

    static std::list<AVPacket *> audio_pkt_list;
    static std::list<AVPacket *> video_pkt_list;


private:
    static void decode_packet(AVPacket *pkt, bool clear_cache);
    static Clock master_clk;
};

#endif //TESTPLAYER_FFUTILS_H
