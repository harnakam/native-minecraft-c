#ifndef C919_S32_PACKET_CONFIRM_TRANSACTION_H
#define C919_S32_PACKET_CONFIRM_TRANSACTION_H
#include "network/PacketBuffer.h"
#include "network/play/INetHandlerPlayClient.h"
struct S32PacketConfirmTransaction {
    MCObject object;
    int32_t windowId;
    int16_t actionNumber;
    bool field_148893_c;
};
S32PacketConfirmTransaction *S32PacketConfirmTransaction_new_empty(MCObjectHeap *);
S32PacketConfirmTransaction *S32PacketConfirmTransaction_new(MCObjectHeap *, int32_t windowId,
                                                             int16_t action, bool accepted);
bool S32PacketConfirmTransaction_readPacketData(S32PacketConfirmTransaction *, PacketBuffer *);
bool S32PacketConfirmTransaction_writePacketData(S32PacketConfirmTransaction *, PacketBuffer *);
bool S32PacketConfirmTransaction_processPacket(S32PacketConfirmTransaction *,
                                               INetHandlerPlayClient);
int32_t S32PacketConfirmTransaction_getWindowId(const S32PacketConfirmTransaction *);
int16_t S32PacketConfirmTransaction_getActionNumber(const S32PacketConfirmTransaction *);
bool S32PacketConfirmTransaction_func_148888_e(const S32PacketConfirmTransaction *);
bool S32PacketConfirmTransaction_isInstance(const MCObject *);
#endif
