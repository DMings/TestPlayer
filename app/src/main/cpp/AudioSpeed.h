//
// Created by 75621 on 2021/10/5.
//

#ifndef ANDROID_LIB_AUDIOSPEED_H
#define ANDROID_LIB_AUDIOSPEED_H

#include <libsoundtouch/SoundTouch.h>
#include <libsoundtouch/FIFOSampleBuffer.h>
#include <functional>

using namespace soundtouch;

class AudioSpeed {
private:
    SoundTouch soundTouch;
    FIFOSampleBuffer outputBuffer;
    std::function<uint32_t(SAMPLETYPE * sampleBuffer)> requestData;
    uint32_t numSamples;
    SAMPLETYPE *tempBuffer;
    bool isReceive;
    bool isFlush;
public:
    AudioSpeed(uint32_t sampleRate, uint32_t nChannels, uint32_t nSamples);

    void setDataSource(std::function<uint32_t(SAMPLETYPE * sampleBuffer)> request);

    // n = -95.0 .. +5000.0 %
    void setSpeed(float rate);

    uint32_t getSamples(SAMPLETYPE *output);

    uint32_t getWantSamples(SAMPLETYPE *output);

    void flush();

    ~AudioSpeed();

};

#endif //ANDROID_LIB_AUDIOSPEED_H
