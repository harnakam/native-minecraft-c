#ifndef C919_C0F_PACKET_CONFIRM_TRANSACTION_H
#define C919_C0F_PACKET_CONFIRM_TRANSACTION_H
#include "network/PacketBuffer.h"
#include "network/play/INetHandlerPlayServer.h"
struct C0FPacketConfirmTransaction { MCObject object; int32_t windowId; int16_t uid; bool accepted; };
C0FPacketConfirmTransaction *C0FPacketConfirmTransaction_new_empty(MCObjectHeap *);
C0FPacketConfirmTransaction *C0FPacketConfirmTransaction_new(MCObjectHeap *,int32_t windowId,int16_t uid,bool accepted);
bool C0FPacketConfirmTransaction_readPacketData(C0FPacketConfirmTransaction *,PacketBuffer *);
bool C0FPacketConfirmTransaction_writePacketData(C0FPacketConfirmTransaction *,PacketBuffer *);
bool C0FPacketConfirmTransaction_processPacket(C0FPacketConfirmTransaction *,INetHandlerPlayServer);
int32_t C0FPacketConfirmTransaction_getWindowId(const C0FPacketConfirmTransaction *);
int16_t C0FPacketConfirmTransaction_getUid(const C0FPacketConfirmTransaction *);
#endif
