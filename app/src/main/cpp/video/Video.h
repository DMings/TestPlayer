//
// Created by Administrator on 2019/9/12.
//

#ifndef TESTPLAYER_VIDEO_H
#define TESTPLAYER_VIDEO_H

#include "./render/GLThread.h"
#include "../audio/Audio.h"
#include "../av/BaseAV.h"
#include "../utils/PthreadSleep.h"

class Video : BaseAV {
public:

    Video(GLThread *glThread, AVClock *avClock);

    int OpenStream(AVFormatContext *fmtCtx);

    void PutAvPacket(FPacket *pkt);

    uint64_t GetAvPacketSize();

    int StreamIndex() const;

    AVStream *Stream() const;

    ~Video();

private:
    AVCodecContext *avDecCtx_ = nullptr;
    AVClock *avClock_ = nullptr;
    int streamId_ = -1;
    AVStream *avStream_ = nullptr;
    SwsContext *swsContext_ = nullptr;
    GLThread *glThread_;
    uint8_t *dstData_[4]{};
    int dstLineSize_[4]{};
    pthread_t pVideoTid_ = 0;
    std::atomic_bool threadFinish_ = false;
    int64_t lastPts_ = 0;
    PthreadSleep pthreadSleep_;

    uint SynchronizeVideo(int64_t lastPts, int64_t pktDuration);

    static void *VideoThreadProcess(void *arg);
};


#endif //TESTPLAYER_VIDEO_H
