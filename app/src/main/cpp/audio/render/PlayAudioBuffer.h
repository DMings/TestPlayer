//
// Created by odm on 2023/3/8.
//

#ifndef LDSTREAMS_READ_AUDIO_BUFFER_H
#define LDSTREAMS_READ_AUDIO_BUFFER_H

#include "../../utils/blocking_queue.h"
#include <list>
#include <mutex>
#include <stdint.h>

extern "C" {
#include "libavutil/audio_fifo.h"
}

class PlayAudioBuffer {

public:
    PlayAudioBuffer(int sampleRate, int channels);

    void PutSamples(uint8_t *data, int nbSamples);

    int GetSamples(uint8_t **data, int nbSamples);

    int DataSize();

    void NotifyFinish();

    ~PlayAudioBuffer();

private:
    AVAudioFifo *audioFifo_;
    std::mutex dataLock_;
    int channels_;
    int sampleByte_;
    std::mutex mtx_;
    std::mutex dataMtx_;
    std::condition_variable cv_;
    bool finish_;
    std::list<uint8_t *> memPool_;

    int AvAudioFifoWrite(void **inputData, int nbSamples);

    int AvAudioFifoRead(uint8_t **data, int nbSamples);

    int AvAudioFifoNbSamples();
};


#endif //LDSTREAMS_READ_AUDIO_BUFFER_H
