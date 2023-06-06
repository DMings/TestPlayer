//
// Created by 75621 on 2021/10/5.
//

#include "AudioSpeed.h"
#include "../../utils/FLog.h"

using namespace soundtouch;

AudioSpeed::AudioSpeed(uint32_t sampleRate, int nChannels) {
    isFlush = false;
    soundTouch_ = new SoundTouch();
    soundTouch_->setSampleRate(sampleRate);
    soundTouch_->setChannels(nChannels);
}

void AudioSpeed::SetSpeed(float rate) {
    soundTouch_->setTempo(rate);
}

uint32_t
AudioSpeed::GetSamples(SAMPLETYPE *inputBuffer, uint32_t inputSamples, SAMPLETYPE *outputBuffer,
                       uint32_t maxOutputSamples) {
    if (inputBuffer && inputSamples > 0) {
        soundTouch_->putSamples(inputBuffer, inputSamples);
    }
    return soundTouch_->receiveSamples(outputBuffer, maxOutputSamples);
}

double AudioSpeed::GetInputOutputSampleRatio() {
    return soundTouch_->getInputOutputSampleRatio();
}

uint AudioSpeed::NumUnprocessedSamples() const {
    return soundTouch_->numUnprocessedSamples();
}

void AudioSpeed::Flush() {
    isFlush = false;
    soundTouch_->clear();
}

AudioSpeed::~AudioSpeed() {
    soundTouch_->clear();
    delete soundTouch_;
}
