#ifndef C919_NATIVE_PACKET_H
#define C919_NATIVE_PACKET_H
#include "network/PacketBuffer.h"
/* Immutable native transport boundary for packet classes not translated yet.
   Source inventory/metadata packets continue to retain their actual objects.
   This object owns only an already encoded message, never gameplay state. */
MCObject *NativePacket_new(MCObjectHeap *, const mc_buf *completeMessage);
bool NativePacket_isInstance(const MCObject *);
bool NativePacket_writePacketData(MCObject *, PacketBuffer *);
#endif
