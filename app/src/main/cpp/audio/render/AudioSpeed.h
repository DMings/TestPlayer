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
    bool isFlush;
public:
    AudioSpeed(uint32_t sampleRate, int nChannels);

    void setSpeed(float rate);

    uint32_t getSamples(SAMPLETYPE *inputBuffer, uint32_t inputSamples,
                        SAMPLETYPE *outputBuffer, uint32_t maxOutputSamples);

    void flush();

    ~AudioSpeed();

};

#endif //ANDROID_LIB_AUDIOSPEED_H
