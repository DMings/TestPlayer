//
// Created by odm on 2023/6/1.
//

#ifndef TESTPLAYER_FPACKET_H
#define TESTPLAYER_FPACKET_H

#include <libavcodec/avcodec.h>

struct FPacket {
    AVPacket *avPacket;
    int checkout_time;
};

extern struct FPacket *alloc_packet();

extern void free_packet(struct FPacket *packet);


#endif //TESTPLAYER_FPACKET_H
