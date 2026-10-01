#ifndef C919_S30_PACKET_WINDOW_ITEMS_H
#define C919_S30_PACKET_WINDOW_ITEMS_H
#include "network/PacketBuffer.h"
#include "network/play/INetHandlerPlayClient.h"
#include "inventory/Container.h"
struct S30PacketWindowItems {
    MCObject object;
    int32_t windowId;
    ItemStackArray *itemStacks;
};
S30PacketWindowItems *S30PacketWindowItems_new_empty(MCObjectHeap *);
S30PacketWindowItems *S30PacketWindowItems_new(MCObjectHeap *, int32_t windowId, ContainerList *);
bool S30PacketWindowItems_readPacketData(S30PacketWindowItems *, PacketBuffer *);
bool S30PacketWindowItems_writePacketData(S30PacketWindowItems *, PacketBuffer *);
bool S30PacketWindowItems_processPacket(S30PacketWindowItems *, INetHandlerPlayClient);
int32_t S30PacketWindowItems_func_148911_c(const S30PacketWindowItems *);
ItemStackArray *S30PacketWindowItems_getItemStacks(const S30PacketWindowItems *);
bool S30PacketWindowItems_isInstance(const MCObject *);
#endif
