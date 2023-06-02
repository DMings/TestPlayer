//
// Created by Administrator on 2019/8/20.
//

#ifndef TESTPLAYER_OPENSL_H
#define TESTPLAYER_OPENSL_H

#include <SLES/OpenSLES_Android.h>
#include "../../utils/log.h"

typedef void (*signSlBufferCallback)();
//typedef void (*SlBufferCallback)(uint8_t **buffer, uint32_t *bufferSize);

struct SLConfigure {
    int sampleRate;
    int channels;
    int64_t fmt;
    signSlBufferCallback signSlBufferCallback;
};

class OpenSL {
private:
    SLObjectItf engineObject = NULL;
    SLEngineItf engineEngine = NULL;
    SLObjectItf outputMixObject = NULL;
    SLObjectItf bqPlayerObject = NULL;
    SLVolumeItf bqPlayerVolume = NULL;
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue = NULL;
public:
    SLPlayItf bqPlayerPlay = NULL;
    uint8_t *buffer;
    uint32_t bufferSize;
    SLConfigure *slConfigure;
    int createPlayer(SLConfigure *sLConfigure);
    void setEnqueueBuffer(uint8_t *buffer, uint32_t bufferSize);
    void play();
    void pause();
    void release();
    SLuint32 getSupportSampleRate(int sample_rate);
};


#endif //TESTPLAYER_OPENSL_H
