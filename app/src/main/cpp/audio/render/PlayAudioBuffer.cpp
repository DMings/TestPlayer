//
// Created by odm on 2023/3/8.
//

#include "PlayAudioBuffer.h"
#include "../../utils/FLog.h"
#include <malloc.h>

extern "C" {
#include "libavutil/error.h"
}

PlayAudioBuffer::PlayAudioBuffer(int sampleRate, int channels) {
    channels_ = channels;
    sampleByte_ = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    audioFifo_ = av_audio_fifo_alloc(AV_SAMPLE_FMT_S16, channels, sampleRate);
    finish_ = false;
    int nb_buffers = av_sample_fmt_is_planar(AV_SAMPLE_FMT_S16) ? channels : 1;
    LOGI("PlayAudioBuffer nb_buffers: %d", nb_buffers);
}

void PlayAudioBuffer::PutData(uint8_t *data, int dataSize) {
    std::unique_lock<std::mutex> lock(mtx_);
    int ret = AvAudioFifoWrite(reinterpret_cast<void **>(&data), dataSize);
    if (ret < 0) {
        LOGI("av_audio_fifo_write: %s", av_err2str(ret));
    }
    cv_.notify_one();
}


int PlayAudioBuffer::GetData(uint8_t **data, int dataSize) {
    if (dataSize == 0) {
        return 0;
    }
    while (true) {
        int nbSamples = AvAudioFifoNbSamples();
        if (nbSamples >= dataSize) {
            int ret = AvAudioFifoRead(data, dataSize);
            if (ret < 0) {
                LOGE("ERROR: AvAudioFifoRead failed!");
                return -1;
            } else {
                return ret;
            }
        } else {
            std::unique_lock<std::mutex> lock(mtx_);
            nbSamples = AvAudioFifoNbSamples();
            if (nbSamples >= dataSize) {
                continue;
            }
            cv_.wait(lock);
            if (finish_) {
                break;
            }
        }
    }
    return -1;
}

int PlayAudioBuffer::AvAudioFifoWrite(void **inputData, int dataSize) {
    std::lock_guard<std::mutex> lock(dataMtx_);
    return av_audio_fifo_write(audioFifo_, (void **) inputData,
                               dataSize / channels_ / sampleByte_);
}

int PlayAudioBuffer::AvAudioFifoRead(uint8_t **data, int nbSamples) {
    std::lock_guard<std::mutex> lock(dataMtx_);
    return av_audio_fifo_read(audioFifo_, (void **) data, nbSamples);
}


int PlayAudioBuffer::AvAudioFifoNbSamples() {
    std::lock_guard<std::mutex> lock(dataMtx_);
    return av_audio_fifo_size(audioFifo_);
}

void PlayAudioBuffer::NotifyFinish() {
    std::lock_guard<std::mutex> lck(mtx_);
    finish_ = true;
    cv_.notify_all();
}

PlayAudioBuffer::~PlayAudioBuffer() {
    finish_ = true;
    std::lock_guard<std::mutex> lock(dataMtx_);
    av_audio_fifo_free(audioFifo_);
}