//
// Created by odm on 2023/4/23.
//

#ifndef LDSTREAMS_AUDIO_DATA_CALLBACK_H
#define LDSTREAMS_AUDIO_DATA_CALLBACK_H

#include <stdint.h>

class AudioDataCallback {
public:
    virtual void OnAudioAvailable(uint8_t *data, int dataSize) = 0;
};

#endif //LDSTREAMS_AUDIO_DATA_CALLBACK_H
