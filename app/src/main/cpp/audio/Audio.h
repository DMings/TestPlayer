//
// Created by Administrator on 2019/9/12.
//

#ifndef TESTPLAYER_AUDIO_H
#define TESTPLAYER_AUDIO_H

#include "render/AudioPlay.h"
#include "../av/BaseAV.h"
#include "../utils/PthreadSleep.h"

class Audio : BaseAV {
public:
    Audio(AVClock *avClock, uint cacheTime);

    void PutAvPacket(FPacket *pkt);

    uint64_t GetAvPacketSize();

    float GetPktListTime();

    void CalcPlaySpeed(float listTime);

    int OpenStream(AVFormatContext *fmtCtx);

    uint GetMaxCacheTime() const;

    uint GetCacheTime() const;

    int StreamIndex() const;

    AVStream *Stream() const;

    ~Audio();

private:
    AVCodecContext *avDecCtx_ = nullptr;
    AVStream *avStream_ = nullptr;
    int streamId_ = -1;
    AVClock *avClock = nullptr;
    AudioPlay *playAudio_ = nullptr;
    pthread_t pAudioTid_ = 0;
    std::atomic_bool threadFinish_ = false;
    std::list<float> speedList_;
    float pktDuration_ = 1024000.f / 48000.0f;
    float cacheSetTime_ = 200;
    float cacheMaxTime_ = 200;
    float cacheTime_ = 200;
    uint reachMinTimeCount_ = 0;
    uint reachNormalTimeCount_ = 0;
    float targetSpeed_ = 1.f;
    float curSpeed_ = 1.f;

    static bool SampleRateValid(int sampleRate);

    static void *AudioThreadProcess(void *arg);
};


#endif //TESTPLAYER_AUDIO_H
