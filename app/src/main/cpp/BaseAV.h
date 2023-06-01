//
// Created by Administrator on 2019/9/17.
//

#ifndef TESTPLAYER_BASEAV_H
#define TESTPLAYER_BASEAV_H
#include "AVClock.h"
#include <list>

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
#include <libavutil/cpu.h>
#include "FPacket.h"
}

class BaseAV {
public:

    BaseAV();

    ~BaseAV();

    virtual void putAvPacket(FPacket* pkt) = 0;

    virtual void release() = 0;

    virtual void pause() = 0;

    virtual void resume() = 0;
public:
    pthread_cond_t c_cond;
    pthread_mutex_t c_mutex;
    std::list<FPacket *> pkt_list;

public:

     int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx,
                                  AVFormatContext *fmt_ctx, AVMediaType type);

     void clearList();

};

#endif //TESTPLAYER_BASEAV_H
