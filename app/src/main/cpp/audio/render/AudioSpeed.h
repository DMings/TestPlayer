//
// Created by 75621 on 2021/10/5.
//

#ifndef ANDROID_LIB_AUDIOSPEED_H
#define ANDROID_LIB_AUDIOSPEED_H

#include <libsoundtouch/SoundTouch.h>
#include <libsoundtouch/FIFOSampleBuffer.h>
#include <functional>

class AudioSpeed {
private:
    soundtouch::SoundTouch *soundTouch_;
    bool isFlush;
public:
    AudioSpeed(uint32_t sampleRate, int nChannels);

    void SetSpeed(float rate);

    uint32_t GetSamples(soundtouch::SAMPLETYPE *inputBuffer, uint32_t inputSamples,
                        soundtouch::SAMPLETYPE *outputBuffer, uint32_t maxOutputSamples);

    double GetInputOutputSampleRatio();

    uint NumUnprocessedSamples() const;

    void Flush();

    ~AudioSpeed();

};

#endif //ANDROID_LIB_AUDIOSPEED_H
