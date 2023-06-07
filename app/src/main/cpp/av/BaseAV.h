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

    virtual void PutAvPacket(FPacket* pkt) = 0;

    virtual int StreamIndex() const = 0;

    virtual AVStream* Stream() const = 0;

    int OpenCodecContext(int *stream_idx, AVCodecContext **dec_ctx,
                         AVFormatContext *fmt_ctx, AVMediaType type);

    void ClearList();

protected:
    pthread_cond_t cCond_;
    pthread_mutex_t cMutex_;
    std::list<FPacket *> pktList_;

};

#endif //TESTPLAYER_BASEAV_H
