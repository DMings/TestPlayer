//
// Created by Administrator on 2019/8/20.
//

//#include <unistd.h>
#include <pthread.h>
#include "OpenSL.h"


//第一次主动调用在调用线程
//之后在新线程中回调
//static void slBufferCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
////    LOGI("slBufferQueueCallback1 %ld====%d", pthread_self(), gettid());
//    OpenSL *OpenSL = (OpenSL *) context;
//    uint8_t *buffer;
//    SLuint32 bufferSize;
//
//    OpenSL->slConfigure->slBufferCallback(&buffer, &bufferSize);
//    if (bufferSize > 0) {
//        SLresult result = (*bq)->Enqueue(bq, buffer, bufferSize);
////        LOGI("  bqPlayerCallback :%d size:%d", result, bufferSize);
//    } else {
//        uint8_t b[] = {0};
//        SLresult result = (*bq)->Enqueue(bq, b, 1);
////        LOGE("  decodeAudio error bqPlayerCallback :%d", result);
//    }
//}

static void slBufferCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
//    LOGE("***slBufferCallback *** %X <====> %d", pthread_self(), gettid());
//    LOGI("OpenSL slBufferCallback>>>>>");
    SLuint32 pState;
    OpenSL *openSL = (OpenSL *) context;
    (*openSL->bqPlayerPlay)->GetPlayState(openSL->bqPlayerPlay, &pState);
    if (pState == SL_PLAYSTATE_PLAYING) {
        openSL->slConfigure->signSlBufferCallback();
        if (openSL->bufferSize) {
            SLresult result = (*bq)->Enqueue(bq, openSL->buffer, openSL->bufferSize);
        } else {
            uint8_t b[] = {0};
            SLresult result = (*bq)->Enqueue(bq, b, 1);
            LOGI("OpenSL get empty buffer!!!");
        }
        openSL->bufferSize = 0;
        openSL->buffer = NULL;
    }
}

void OpenSL::setEnqueueBuffer(uint8_t *buffer, uint32_t bufferSize) {
    SLuint32 pState;
    (*bqPlayerPlay)->GetPlayState(bqPlayerPlay, &pState);
    if (pState == SL_PLAYSTATE_PLAYING) {
        this->buffer = buffer;
        this->bufferSize = bufferSize;
    }
//    if (bqPlayerPlay != NULL && bqPlayerBufferQueue != NULL) {
//        SLuint32 pState;
//        (*bqPlayerPlay)->GetPlayState(bqPlayerPlay, &pState);
//        if(pState == SL_PLAYSTATE_PLAYING){
////            (*sbq)->Enqueue(bqPlayerBufferQueue, buffer, bufferSize);
//            (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, buffer, bufferSize);
//        }
//    }
}

SLuint32 OpenSL::getSupportSampleRate(int sample_rate) {
    SLuint32 sr;
    switch (sample_rate) {
        case 8000:
            sr = SL_SAMPLINGRATE_8;
            break;
        case 11025:
            sr = SL_SAMPLINGRATE_11_025;
            break;
        case 16000:
            sr = SL_SAMPLINGRATE_16;
            break;
        case 22050:
            sr = SL_SAMPLINGRATE_22_05;
            break;
        case 24000:
            sr = SL_SAMPLINGRATE_24;
            break;
        case 32000:
            sr = SL_SAMPLINGRATE_32;
            break;
        case 44100:
            sr = SL_SAMPLINGRATE_44_1;
            break;
        case 48000:
            sr = SL_SAMPLINGRATE_48;
            break;
        case 64000:
            sr = SL_SAMPLINGRATE_64;
            break;
        case 88200:
            sr = SL_SAMPLINGRATE_88_2;
            break;
        case 96000:
            sr = SL_SAMPLINGRATE_96;
            break;
        case 192000:
            sr = SL_SAMPLINGRATE_192;
            break;
        default:
            sr = 0;
            break;
    }
    return sr;
}

int OpenSL::createPlayer(SLConfigure *sLConfigure) {
//    this->slConfigure = sLConfigure;
//    LOGI("sampleRate: %d, channels: %d", sLConfigure->sampleRate, sLConfigure->channels);
//    int speakers;
//    if (sLConfigure->channels > 1)
//        speakers = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
//    else speakers = SL_SPEAKER_FRONT_CENTER;
//    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, (SLuint32) sLConfigure->channels,
//                                   (SLuint32) getSupportSampleRate(sLConfigure->sampleRate),
//                                   SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
//                                   (SLuint32) speakers, SL_BYTEORDER_LITTLEENDIAN};
//    //--------------------------------------------------------------------------------
//    SLresult result;
//    // 创建引擎engineObject
//    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
//    if (SL_RESULT_SUCCESS != result) {
//        return -1;
//    }
//    // 实现引擎engineObject
//    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
//    if (SL_RESULT_SUCCESS != result) {
//        return -1;
//    }
//    // 获取引擎接口engineEngine
//    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
//    if (SL_RESULT_SUCCESS != result) {
//        return -1;
//    }
//    //-------------------------------------------------------------------------------
//    // 创建混音器outputMixObject
//    const SLInterfaceID mIds[1] = {SL_IID_ENVIRONMENTALREVERB};
//    const SLboolean mReq[1] = {SL_BOOLEAN_FALSE};
//    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 1, mIds, mReq);
//    if (SL_RESULT_SUCCESS != result) {
//        return -1;
//    }
//    // 实现混音器outputMixObject
//    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
//    if (SL_RESULT_SUCCESS != result) {
//        return -1;
//    }
//    //不启用混响可以不用获取接口
////    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
////                                              &outputMixEnvironmentalReverb);
////    const SLEnvironmentalReverbSettings settings = SL_I3DL2_ENVIRONMENT_PRESET_DEFAULT;
////    if (SL_RESULT_SUCCESS == result) {
////        (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
////                outputMixEnvironmentalReverb, &settings);
////    }
//    //======================
//    SLDataLocator_AndroidSimpleBufferQueue android_queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 1};
//    //   新建一个数据源 将上述配置信息放到这个数据源中
//    SLDataSource slDataSource = {&android_queue, &format_pcm};
//    //   设置混音器
//    SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
//    SLDataSink audioSnk = {&outputMix, NULL};
//
//    const SLInterfaceID pIds[] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
//    const SLboolean pReq[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
//    LOGI("CreateAudioPlayer ");
//    //创建播放器
//    (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &slDataSource,
//                                       &audioSnk, 2,
//                                       pIds, pReq);
//    LOGI("CreateAudioPlayer Realize");
//    //初始化播放器
//    (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
//    //得到接口后调用  获取Player接口
//    (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
//    LOGI("CreateAudioPlayer bqPlayerObject");
//    //-------------------------------------------
//    //注册回调缓冲区 //获取缓冲队列接口
//    (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE, &bqPlayerBufferQueue);
//    //缓冲接口回调
//    (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, slBufferCallback, this);
//    //获取音量接口
//    (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &bqPlayerVolume);
//    SLmillibel pMaxLevel;
//    (*bqPlayerVolume)->GetMaxVolumeLevel(bqPlayerVolume, &pMaxLevel);
//    LOGI("pMaxLevel: %d", pMaxLevel);
//    (*bqPlayerVolume)->SetVolumeLevel(bqPlayerVolume, pMaxLevel);
    return 0;
}

void OpenSL::play() {
//    LOGI("OpenSL play->>>>>");
//    if (bqPlayerPlay != NULL && bqPlayerBufferQueue != NULL) {
//        // 设置播放状态
//        (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
////        this->slConfigure->signSlBufferCallback();
//        uint8_t b[] = {0};
//        (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, b, 1);
////        slBufferCallback(bqPlayerBufferQueue, this);
//    }
}

void OpenSL::pause() {
//    LOGI("OpenSL pause>>>>>");
//    if (bqPlayerPlay != NULL) {
//        (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PAUSED);
//    }
}

void OpenSL::release() {
//    LOGI("OpenSL release>>>>>");
//    //设置停止状态
//    if (bqPlayerPlay) {
//        (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_STOPPED);
//        (*bqPlayerBufferQueue)->Clear(bqPlayerBufferQueue);
//        bqPlayerPlay = NULL;
//    }
//    LOGI("OpenSL release>>>>>SetPlayState");
//    //销毁播放器
//    if (bqPlayerObject) {
//        (*bqPlayerObject)->Destroy(bqPlayerObject);
//        bqPlayerObject = NULL;
//        bqPlayerBufferQueue = NULL;
//    }
//    LOGI("OpenSL release>>>>>bqPlayerObject");
//    //销毁混音器
//    if (outputMixObject) {
//        (*outputMixObject)->Destroy(outputMixObject);
//        outputMixObject = NULL;
//    }
//    LOGI("OpenSL release>>>>>outputMixObject");
//    //销毁引擎
//    if (engineObject) {
//        (*engineObject)->Destroy(engineObject);
//        engineObject = NULL;
//        engineEngine = NULL;
//    }
//    LOGI("OpenSL release>>>>>engineObject");
}

