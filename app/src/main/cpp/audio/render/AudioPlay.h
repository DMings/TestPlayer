//
// Created by odm on 2023/3/23.
//

#ifndef LDSTREAMS_RECORD_AUDIO_H
#define LDSTREAMS_RECORD_AUDIO_H

#include "oboe/Oboe.h"
#include "../../utils/time_utils.h"
#include "PlayAudioBuffer.h"

class AudioPlay {
public:
    AudioPlay(int sampleRate, int channels);

    void PutData(uint8_t *data, int dataSize);

    void Start();

    void Stop();

    ~AudioPlay();

private:
    class MyDataCallback : public oboe::AudioStreamDataCallback {

    public:
        MyDataCallback(AudioPlay *parent) : mParent(parent) {}

        oboe::DataCallbackResult onAudioReady(
                oboe::AudioStream *audioStream,
                void *audioData,
                int32_t numFrames) override;

    private:
        AudioPlay *mParent;

    };

    class MyErrorCallback : public oboe::AudioStreamErrorCallback {
    public:
        MyErrorCallback(AudioPlay *parent) : mParent(parent) {}

        virtual ~MyErrorCallback() {
        }

        void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error) override;

    private:
        AudioPlay *mParent;
    };

    std::shared_ptr<oboe::AudioStream> mStream;
    std::shared_ptr<MyDataCallback> mDataCallback;
    std::shared_ptr<MyErrorCallback> mErrorCallback;

    static constexpr int kChannelCount = 2;

    PlayAudioBuffer playAudioBuffer_;

    long long mTestTime;

    void Restart();
};

#endif //LDSTREAMS_RECORD_AUDIO_H
