#ifndef C919_C10_PACKET_CREATIVE_INVENTORY_ACTION_H
#define C919_C10_PACKET_CREATIVE_INVENTORY_ACTION_H
#include "network/PacketBuffer.h"
#include "network/play/INetHandlerPlayServer.h"
struct C10PacketCreativeInventoryAction { MCObject object; int32_t slotId; ItemStack *stack; };
C10PacketCreativeInventoryAction *C10PacketCreativeInventoryAction_new_empty(MCObjectHeap *);
C10PacketCreativeInventoryAction *C10PacketCreativeInventoryAction_new(MCObjectHeap *,int32_t slotId,ItemStack *stack);
bool C10PacketCreativeInventoryAction_readPacketData(C10PacketCreativeInventoryAction *,PacketBuffer *);
bool C10PacketCreativeInventoryAction_writePacketData(C10PacketCreativeInventoryAction *,PacketBuffer *);
bool C10PacketCreativeInventoryAction_processPacket(C10PacketCreativeInventoryAction *,INetHandlerPlayServer);
int32_t C10PacketCreativeInventoryAction_getSlotId(const C10PacketCreativeInventoryAction *);
ItemStack *C10PacketCreativeInventoryAction_getStack(const C10PacketCreativeInventoryAction *);
#endif
