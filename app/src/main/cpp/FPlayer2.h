//
// Created by Administrator on 2019/9/12.
//

#ifndef TESTPLAYER_FFUTILS_H
#define TESTPLAYER_FFUTILS_H

#include "Audio2.h"
#include "Video2.h"

extern void decode_packet(AVPacket *pkt, bool clear_cache);

#endif //TESTPLAYER_FFUTILS_H
