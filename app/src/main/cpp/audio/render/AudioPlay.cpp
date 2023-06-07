//
// Created by odm on 2023/3/23.
//

#include "AudioPlay.h"
#include "../../utils/FLog.h"
#include "oboe/Oboe.h"

using namespace oboe;

DataCallbackResult AudioPlay::MyDataCallback::onAudioReady(
        AudioStream *audioStream,
        void *audioData,
        int32_t numFrames) {
//    LOGI("numFrames: %d cost time: %lld", numFrames,
//         (GetCurrentTimeMs() - mParent->mTestTime));
    mParent->mTestTime = GetCurrentTimeMs();
    auto *data = static_cast<uint8_t *>(audioData);
    int dataSize = mParent->playAudioBuffer_.GetSamples(&data, numFrames);
    if (dataSize > 0) {
        return DataCallbackResult::Continue;
    } else {
        LOGE("playAudioBuffer_.GetSamples: %d", dataSize);
        return DataCallbackResult::Stop;
    }
}

void AudioPlay::MyErrorCallback::onErrorAfterClose(AudioStream *oboeStream,
                                                   Result error) {
    if (error == Result::ErrorDisconnected) {
        LOGI("Restarting AudioStream");
        mParent->Restart();
    }
    LOGE("Error was %s", convertToText(error));
}

AudioPlay::AudioPlay(int sampleRate, int channels) : playAudioBuffer_(sampleRate, channels) {
    mDataCallback = std::make_shared<MyDataCallback>(this);
    mErrorCallback = std::make_shared<MyErrorCallback>(this);
    AudioStreamBuilder builder;
    builder.setDirection(Direction::Output);
    builder.setPerformanceMode(PerformanceMode::LowLatency);
    builder.setSharingMode(SharingMode::Exclusive);
    builder.setInputPreset(InputPreset::Unprocessed);
//    builder.setAudioApi(AudioApi::OpenSLES);
    builder.setFormat(AudioFormat::I16);
    builder.setChannelCount(channels == 2 ? ChannelCount::Stereo : ChannelCount::Mono);
    builder.setSampleRate(sampleRate);
//    builder.setBufferCapacityInFrames(512 * 2 * 2);
    builder.setDataCallback(mDataCallback);
    builder.setErrorCallback(mErrorCallback);
    Result r = builder.openStream(mStream);
//    mStream->setBufferSizeInFrames(512 * mStream->getBytesPerFrame());
    LOGI("getBufferCapacityInFrames: %d ", mStream->getBufferCapacityInFrames());
    LOGI("getBufferSizeInFrames: %d ", mStream->getBufferSizeInFrames());
    LOGI("getFramesPerBurst: %d ", mStream->getFramesPerBurst());
    LOGI("getBytesPerFrame: %d ", mStream->getBytesPerFrame());
    LOGI("Open stream: %s ", convertToText(r));
    mTestTime = 0;
}

void AudioPlay::PutSamples(uint8_t *data, int nbSamples) {
    if (nbSamples == 0) {
        return;
    }
    playAudioBuffer_.PutSamples(data, nbSamples);
}

int AudioPlay::SampleCount() {
    return playAudioBuffer_.DataSize();
}


void AudioPlay::Start() {
    mStream->requestStart();
    mTestTime = GetCurrentTimeMs();
}

void AudioPlay::Restart() {
    if (mStream && mStream->getState() != StreamState::Closed) {
        mStream->stop();
        mStream->close();
    }
    if (mStream) {
        mStream.reset();
        Result result = mStream->start();
        if (result == Result::OK) {
        } else {
            LOGW("Failed attempt at starting the playback stream. Error: %s",
                 convertToText(result));
        }
    }
}

void AudioPlay::Stop() {
//    const ResultWithValue<double> &delayTime = mStream->calculateLatencyMillis();
//    LOGI("stop delayTime: %f r: %s", delayTime.value(),convertToText(delayTime.error()));
    playAudioBuffer_.NotifyFinish();
    LOGI("requestStop start");
    mStream->requestStop();
    LOGI("requestStop end");
}

AudioPlay::~AudioPlay() {
    LOGI("~RecordAudio");
    StreamState next = StreamState::Unknown;
    constexpr int kTimeoutInNanos = 1000 * kNanosPerMillisecond;
    mStream->waitForStateChange(StreamState::Stopping, &next, kTimeoutInNanos);
    LOGI("~RecordAudio stop");
    if (mStream != nullptr) {
        mStream->close(); // just in case it accidentally opened
    }
    LOGI("~RecordAudio close end");
}
