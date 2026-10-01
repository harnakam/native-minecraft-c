#include "network/play/client/C10PacketCreativeInventoryAction.h"
#include "network/play/client/native_packet.h"
static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    C10PacketCreativeInventoryAction *packet=(C10PacketCreativeInventoryAction *)object;
    packet->stack=(ItemStack *)visitor((MCObject *)packet->stack,context);
}
static const MCObjectClass packet_class={"net.minecraft.network.play.client.C10PacketCreativeInventoryAction",MCObjectHeap_plainClone,trace,NULL};
C10PacketCreativeInventoryAction *C10PacketCreativeInventoryAction_new_empty(MCObjectHeap *heap) {
    return (C10PacketCreativeInventoryAction *)MCObjectHeap_alloc(heap,sizeof(C10PacketCreativeInventoryAction),&packet_class);
}
C10PacketCreativeInventoryAction *C10PacketCreativeInventoryAction_new(MCObjectHeap *heap,int32_t slotId,ItemStack *stack) {
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,heap)) return NULL;
    C10PacketCreativeInventoryAction *packet=C10PacketCreativeInventoryAction_new_empty(heap);
    if (packet) {
        packet->slotId=slotId; packet->stack=stack ? ItemStack_copy(heap,stack) : NULL;
        if (stack && !packet->stack) packet=NULL;
    }
    MCObjectRootScope_end(&scope); return packet;
}
bool C10PacketCreativeInventoryAction_readPacketData(C10PacketCreativeInventoryAction *packet,PacketBuffer *buffer) {
    MCObjectRootScope scope={0}; if (!mc_packet_begin((MCObject *)packet,buffer,&scope)) return false;
    int32_t value=mc_get_i16(buffer->buffer); if (buffer->buffer->failed) goto done; packet->slotId=value;
    if (!PacketBuffer_readItemStackFromBuffer(buffer,&packet->stack)) goto done;
done:
    return mc_packet_finish((MCObject *)packet,buffer,&scope,!buffer->buffer->failed);
}
bool C10PacketCreativeInventoryAction_writePacketData(C10PacketCreativeInventoryAction *packet,PacketBuffer *buffer) {
    MCObjectRootScope scope={0}; if (!mc_packet_begin((MCObject *)packet,buffer,&scope)) return false;
    mc_put_i16(buffer->buffer,mc_packet_short(packet->slotId));
    bool ok=PacketBuffer_writeItemStackToBuffer(buffer,packet->stack);
    return mc_packet_finish((MCObject *)packet,buffer,&scope,ok);
}
bool C10PacketCreativeInventoryAction_processPacket(C10PacketCreativeInventoryAction *packet,INetHandlerPlayServer handler) {
    MCObjectRootScope scope={0}; if (!mc_packet_handler_begin((MCObject *)packet,handler,&scope)) return false;
    bool ok=handler.methods->processCreativeInventoryAction && handler.methods->processCreativeInventoryAction(handler.instance,packet);
    return mc_packet_handler_finish((MCObject *)packet,&scope,ok);
}
int32_t C10PacketCreativeInventoryAction_getSlotId(const C10PacketCreativeInventoryAction *packet) { return packet->slotId; }
ItemStack *C10PacketCreativeInventoryAction_getStack(const C10PacketCreativeInventoryAction *packet) { return packet->stack; }

bool C10PacketCreativeInventoryAction_isInstance(const MCObject *object) {
    return object && object->klass == &packet_class;
}
