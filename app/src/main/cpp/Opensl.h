//
// Created by Administrator on 2019/8/20.
//

#ifndef TESTPLAYER_OPENSL_H
#define TESTPLAYER_OPENSL_H

#include <SLES/OpenSLES_Android.h>
#include "log.h"

class Opensl {
public:

    Opensl(int sampleRate, int channels);

    int createPlayer(int sampleRate, int channels);

    static void slBufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context);

    void play();

    void pause();

    void release();

private:
    SLObjectItf engineObject = NULL;
    SLEngineItf engineEngine = NULL;
    SLObjectItf outputMixObject = NULL;
    SLObjectItf bqPlayerObject = NULL;
    SLVolumeItf bqPlayerVolume = NULL;
    SLPlayItf bqPlayerPlay = NULL;
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue = NULL;
};


#endif //TESTPLAYER_OPENSL_H
