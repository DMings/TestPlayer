//
// Created by 75621 on 2021/10/5.
//

#include "AudioSpeed.h"

using namespace soundtouch;

AudioSpeed::AudioSpeed(uint32_t sampleRate, int nChannels) {
    isFlush = false;
    soundTouch.setSampleRate(sampleRate);
    soundTouch.setChannels(nChannels);
}

void AudioSpeed::setSpeed(float rate) {
    soundTouch.setTempo(rate);
}

uint32_t
AudioSpeed::getSamples(SAMPLETYPE *inputBuffer, uint32_t inputSamples, SAMPLETYPE *outputBuffer,
                       uint32_t maxOutputSamples) {
    if (inputBuffer && inputSamples > 0) {
        soundTouch.putSamples(inputBuffer, inputSamples);
    }
    return soundTouch.receiveSamples(outputBuffer, maxOutputSamples);
}

void AudioSpeed::flush() {
    isFlush = false;
    soundTouch.clear();
}

AudioSpeed::~AudioSpeed() {
    soundTouch.clear();
}
