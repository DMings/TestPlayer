//
// Created by odm on 2023/6/1.
//

#include "FPacket.h"

struct FPacket *alloc_packet() {
    struct FPacket *packet = (struct FPacket *) malloc(sizeof(struct FPacket));
    packet->flush = 0;
//    packet->is_seek = false;
    return packet;
}

void free_packet(struct FPacket *packet) {
    free(packet);
}