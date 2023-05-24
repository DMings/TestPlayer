//
// Created by 75621 on 2021/10/5.
//

#include "AudioSpeed.h"

using namespace soundtouch;

AudioSpeed::AudioSpeed(uint32_t sampleRate, uint32_t nChannels, uint32_t nSamples) {
    isFlush = false;
    isReceive = false;
    numSamples = nSamples;
    soundTouch.setSampleRate(sampleRate);
    soundTouch.setChannels(nChannels);
    outputBuffer.setChannels(nChannels);
    tempBuffer = new SAMPLETYPE[numSamples * soundTouch.numChannels() * 4];
}

void AudioSpeed::setDataSource(std::function<uint32_t(SAMPLETYPE *)> request) {
    requestData = std::move(request);
}

void AudioSpeed::setSpeed(float rate) {
    soundTouch.setTempo(rate);
}

uint32_t AudioSpeed::getWantSamples(SAMPLETYPE *sampleBuffer) {
    do {
        uint32_t nSamples = getSamples(sampleBuffer);
        if (nSamples > 0) {
            outputBuffer.putSamples(sampleBuffer, nSamples);
            if (outputBuffer.numSamples() >= numSamples) {
                nSamples = outputBuffer.receiveSamples(sampleBuffer, numSamples);
                return nSamples;
            }
        } else {
            if (outputBuffer.numSamples() > 0) {
                nSamples = outputBuffer.receiveSamples(sampleBuffer, outputBuffer.numSamples());
                return nSamples;
            }
            break;
        }
    } while (true);
    return 0;
}

uint32_t AudioSpeed::getSamples(SAMPLETYPE *output) {
    uint32_t inputSamples;
    uint32_t outputSamples;
    // 如果未接收完成，继续接收
    if (isReceive) {
        do {
            outputSamples = soundTouch.receiveSamples(output, numSamples);
            if (outputSamples > 0) {
                return outputSamples;
            }
        } while (outputSamples != 0);
        isReceive = false;
    }
    // 先判断是否结束,如果结束了，就不会继续读了
    if (!isFlush) {
        do {
            inputSamples = requestData(tempBuffer);
            if (inputSamples <= 0) {
                break;
            }
            soundTouch.putSamples(tempBuffer, inputSamples);
            do {
                isReceive = true;
                outputSamples = soundTouch.receiveSamples(output, numSamples);
                if (outputSamples > 0) {
                    return outputSamples;
                }
            } while (outputSamples != 0);
            isReceive = false;
        } while (inputSamples > 0);
        soundTouch.flush();
        isFlush = true;
    }
    do {
        outputSamples = soundTouch.receiveSamples(output, numSamples);
        if (outputSamples > 0) {
            return outputSamples;
        }
    } while (outputSamples != 0);
    return 0;
}

void AudioSpeed::flush() {
    isFlush = false;
    soundTouch.clear();
    outputBuffer.clear();
}

AudioSpeed::~AudioSpeed() {
    soundTouch.clear();
    outputBuffer.clear();
    delete[] tempBuffer;
}
