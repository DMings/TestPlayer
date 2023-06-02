//
// Created by odm on 2023/3/23.
//

#include "record_audio.h"
#include "../../utils/FLog.h"
#include "oboe/Oboe.h"

using namespace oboe;

DataCallbackResult RecordAudio::MyDataCallback::onAudioReady(
        AudioStream *audioStream,
        void *audioData,
        int32_t numFrames) {
//    LOGI("numFrames: %d cost time: %lld", numFrames,
//         (GetCurrentTime() - mParent->mTestTime));
    mParent->mTestTime = GetCurrentTimeMs();
    if (numFrames > 0) {
        auto data = static_cast<uint8_t *>(audioData);
//        fwrite(data, 1, numFrames * audioStream->getBytesPerFrame(), mParent->outFile_);
        mParent->audioDataCallback_->OnAudioAvailable(data,
                                                      numFrames * audioStream->getBytesPerFrame());
    }
    return DataCallbackResult::Continue;
//    const int64_t timeoutNanos = 0; // for a non-blocking read
//    auto result = audioStream->read(audioData, numFrames, timeoutNanos);
//    // result has type ResultWithValue<int32_t>, which for convenience is coerced
//    // to a Result type when compared with another Result.
//    if (result == Result::OK) {
//        if (result.value() < numFrames) {
//            // replace the missing data with silence
////            memset(static_cast<sample_type*>(audioData) + result.value() * samplesPerFrame, 0,
////                   (numFrames - result.value()) * audioStream->getBytesPerFrame());
//        }
//        LOGI("numFrames: %d cost time: %lld", result.value(), (GetCurrentTime() - mParent->mTestTime));
//        mParent->mTestTime = GetCurrentTime();
//        return DataCallbackResult::Continue;
//    }
//
//    return DataCallbackResult::Stop;
}

void RecordAudio::MyErrorCallback::onErrorAfterClose(AudioStream *oboeStream,
                                                     Result error) {
    if (error == Result::ErrorDisconnected) {
        LOGI("Restarting AudioStream");
        mParent->Restart();
    }
    LOGE("Error was %s", convertToText(error));
}

RecordAudio::RecordAudio(int sampleRate, int channels) {
    audioDataCallback_ = nullptr;
    mDataCallback = std::make_shared<MyDataCallback>(this);
    mErrorCallback = std::make_shared<MyErrorCallback>(this);
    AudioStreamBuilder builder;
    builder.setDirection(Direction::Input);
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

void RecordAudio::SetCallback(AudioDataCallback *audioDataCallback) {
    audioDataCallback_ = audioDataCallback;
}

void RecordAudio::Start() {
    mStream->requestStart();
    mTestTime = GetCurrentTimeMs();
}

void RecordAudio::Restart() {
    if (mStream && mStream->getState() != StreamState::Closed) {
        mStream->stop();
        mStream->close();
    }
    mStream.reset();
    Result result = mStream->start();
    if (result == Result::OK) {
    } else {
        LOGW("Failed attempt at starting the playback stream. Error: %s", convertToText(result));
    }
}

void RecordAudio::Stop() {
//    const ResultWithValue<double> &delayTime = mStream->calculateLatencyMillis();
//    LOGI("stop delayTime: %f r: %s", delayTime.value(),convertToText(delayTime.error()));
    LOGI("requestStop start");
    mStream->requestStop();
    LOGI("requestStop end");
}

RecordAudio::~RecordAudio() {
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
