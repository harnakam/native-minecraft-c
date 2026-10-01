#ifndef C919_S2F_PACKET_SET_SLOT_H
#define C919_S2F_PACKET_SET_SLOT_H
#include "network/PacketBuffer.h"
#include "network/play/INetHandlerPlayClient.h"
struct S2FPacketSetSlot {
    MCObject object;
    int32_t windowId, slot;
    ItemStack *item;
};
S2FPacketSetSlot *S2FPacketSetSlot_new_empty(MCObjectHeap *);
S2FPacketSetSlot *S2FPacketSetSlot_new(MCObjectHeap *, int32_t windowId, int32_t slot, ItemStack *);
bool S2FPacketSetSlot_readPacketData(S2FPacketSetSlot *, PacketBuffer *);
bool S2FPacketSetSlot_writePacketData(S2FPacketSetSlot *, PacketBuffer *);
bool S2FPacketSetSlot_processPacket(S2FPacketSetSlot *, INetHandlerPlayClient);
int32_t S2FPacketSetSlot_func_149175_c(const S2FPacketSetSlot *);
int32_t S2FPacketSetSlot_func_149173_d(const S2FPacketSetSlot *);
ItemStack *S2FPacketSetSlot_func_149174_e(const S2FPacketSetSlot *);
#endif
