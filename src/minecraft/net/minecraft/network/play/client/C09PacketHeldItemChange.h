#ifndef C919_SOURCE_C09_PACKET_HELD_ITEM_CHANGE_H
#define C919_SOURCE_C09_PACKET_HELD_ITEM_CHANGE_H
#include "network/PacketBuffer.h"
#include "network/play/INetHandlerPlayServer.h"
struct C09PacketHeldItemChange {
    MCObject object;
    int32_t slotId;
};
C09PacketHeldItemChange *C09PacketHeldItemChange_new_empty(MCObjectHeap *);
C09PacketHeldItemChange *C09PacketHeldItemChange_new(MCObjectHeap *, int32_t slotId);
bool C09PacketHeldItemChange_isInstance(const MCObject *);
bool C09PacketHeldItemChange_readPacketData(C09PacketHeldItemChange *, PacketBuffer *);
bool C09PacketHeldItemChange_writePacketData(C09PacketHeldItemChange *, PacketBuffer *);
bool C09PacketHeldItemChange_processPacket(C09PacketHeldItemChange *, INetHandlerPlayServer);
int32_t C09PacketHeldItemChange_getSlotId(const C09PacketHeldItemChange *);
#endif
