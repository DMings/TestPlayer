//
// Created by Administrator on 2019/8/20.
//

#ifndef TESTPLAYER_OPENSL_H
#define TESTPLAYER_OPENSL_H

#include <SLES/OpenSLES_Android.h>
#include "log.h"

typedef void** (*SlBufferCallback)();

typedef struct SLConfigure{
    int sampleRate;
    int channels;
    int64_t fmt;
    SlBufferCallback slBufferCallback;
};

class Opensl {
private:
    SLObjectItf engineObject = NULL;
    SLEngineItf engineEngine = NULL;
    SLObjectItf outputMixObject = NULL;
    SLObjectItf bqPlayerObject = NULL;
    SLVolumeItf bqPlayerVolume = NULL;
    SLPlayItf bqPlayerPlay = NULL;
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue = NULL;
public:
    SLConfigure* slConfigure;

    int createPlayer(SLConfigure *sLConfigure);

    void play();

    void pause();

    void release();

};


#endif //TESTPLAYER_OPENSL_H
