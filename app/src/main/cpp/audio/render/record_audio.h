//
// Created by odm on 2023/3/23.
//

#ifndef LDSTREAMS_RECORD_AUDIO_H
#define LDSTREAMS_RECORD_AUDIO_H

#include "oboe/Oboe.h"
#include "../../utils/time_utils.h"
#include "audio_data_callback.h"

class RecordAudio {
public:
    RecordAudio(int sampleRate, int channels);

    void SetCallback(AudioDataCallback *audioDataCallback);

    void Start();

    void Stop();

    ~RecordAudio();

private:
    class MyDataCallback : public oboe::AudioStreamDataCallback {

    public:
        MyDataCallback(RecordAudio *parent) : mParent(parent) {}

        oboe::DataCallbackResult onAudioReady(
                oboe::AudioStream *audioStream,
                void *audioData,
                int32_t numFrames) override;

    private:
        RecordAudio *mParent;

    };

    class MyErrorCallback : public oboe::AudioStreamErrorCallback {
    public:
        MyErrorCallback(RecordAudio *parent) : mParent(parent) {}

        virtual ~MyErrorCallback() {
        }

        void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error) override;

    private:
        RecordAudio *mParent;
    };

    std::shared_ptr<oboe::AudioStream> mStream;
    std::shared_ptr<MyDataCallback> mDataCallback;
    std::shared_ptr<MyErrorCallback> mErrorCallback;

    static constexpr int kChannelCount = 2;

    AudioDataCallback *audioDataCallback_;

    long long mTestTime;

    void Restart();
};

#endif //LDSTREAMS_RECORD_AUDIO_H
