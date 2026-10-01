#include "network/play/client/C0FPacketConfirmTransaction.h"
#include "network/play/client/native_packet.h"
static const MCObjectClass packet_class={"net.minecraft.network.play.client.C0FPacketConfirmTransaction",MCObjectHeap_plainClone,NULL,NULL};
C0FPacketConfirmTransaction *C0FPacketConfirmTransaction_new_empty(MCObjectHeap *heap) {
    return (C0FPacketConfirmTransaction *)MCObjectHeap_alloc(heap,sizeof(C0FPacketConfirmTransaction),&packet_class);
}
C0FPacketConfirmTransaction *C0FPacketConfirmTransaction_new(MCObjectHeap *heap,int32_t windowId,int16_t uid,bool accepted) {
    C0FPacketConfirmTransaction *packet=C0FPacketConfirmTransaction_new_empty(heap);
    if (packet) { packet->windowId=windowId; packet->uid=uid; packet->accepted=accepted; }
    return packet;
}
bool C0FPacketConfirmTransaction_readPacketData(C0FPacketConfirmTransaction *packet,PacketBuffer *buffer) {
    MCObjectRootScope scope={0}; if (!mc_packet_begin((MCObject *)packet,buffer,&scope)) return false;
    int32_t value=mc_packet_byte(buffer); if (buffer->buffer->failed) goto done; packet->windowId=value;
    value=mc_get_i16(buffer->buffer); if (buffer->buffer->failed) goto done; packet->uid=(int16_t)value;
    value=mc_packet_byte(buffer); if (buffer->buffer->failed) goto done; packet->accepted=value!=0;
done:
    return mc_packet_finish((MCObject *)packet,buffer,&scope,!buffer->buffer->failed);
}
bool C0FPacketConfirmTransaction_writePacketData(C0FPacketConfirmTransaction *packet,PacketBuffer *buffer) {
    MCObjectRootScope scope={0}; if (!mc_packet_begin((MCObject *)packet,buffer,&scope)) return false;
    mc_put_u8(buffer->buffer,(uint8_t)packet->windowId); mc_put_i16(buffer->buffer,packet->uid);
    mc_put_u8(buffer->buffer,packet->accepted ? 1 : 0);
    return mc_packet_finish((MCObject *)packet,buffer,&scope,!buffer->buffer->failed);
}
bool C0FPacketConfirmTransaction_processPacket(C0FPacketConfirmTransaction *packet,INetHandlerPlayServer handler) {
    MCObjectRootScope scope={0}; if (!mc_packet_handler_begin((MCObject *)packet,handler,&scope)) return false;
    bool ok=handler.methods->processConfirmTransaction && handler.methods->processConfirmTransaction(handler.instance,packet);
    return mc_packet_handler_finish((MCObject *)packet,&scope,ok);
}
int32_t C0FPacketConfirmTransaction_getWindowId(const C0FPacketConfirmTransaction *packet) { return packet->windowId; }
int16_t C0FPacketConfirmTransaction_getUid(const C0FPacketConfirmTransaction *packet) { return packet->uid; }
