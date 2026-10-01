#include "network/play/client/C0DPacketCloseWindow.h"
#include "network/play/client/native_packet.h"
static const MCObjectClass packet_class={"net.minecraft.network.play.client.C0DPacketCloseWindow",MCObjectHeap_plainClone,NULL,NULL};
C0DPacketCloseWindow *C0DPacketCloseWindow_new_empty(MCObjectHeap *heap) {
    return (C0DPacketCloseWindow *)MCObjectHeap_alloc(heap,sizeof(C0DPacketCloseWindow),&packet_class);
}
C0DPacketCloseWindow *C0DPacketCloseWindow_new(MCObjectHeap *heap,int32_t windowId) {
    C0DPacketCloseWindow *packet=C0DPacketCloseWindow_new_empty(heap);
    if (packet) packet->windowId=windowId;
    return packet;
}
bool C0DPacketCloseWindow_readPacketData(C0DPacketCloseWindow *packet,PacketBuffer *buffer) {
    MCObjectRootScope scope={0}; if (!mc_packet_begin((MCObject *)packet,buffer,&scope)) return false;
    int32_t value=mc_packet_byte(buffer);
    if (!buffer->buffer->failed) packet->windowId=value;
    return mc_packet_finish((MCObject *)packet,buffer,&scope,!buffer->buffer->failed);
}
bool C0DPacketCloseWindow_writePacketData(C0DPacketCloseWindow *packet,PacketBuffer *buffer) {
    MCObjectRootScope scope={0}; if (!mc_packet_begin((MCObject *)packet,buffer,&scope)) return false;
    mc_put_u8(buffer->buffer,(uint8_t)packet->windowId);
    return mc_packet_finish((MCObject *)packet,buffer,&scope,!buffer->buffer->failed);
}
bool C0DPacketCloseWindow_processPacket(C0DPacketCloseWindow *packet,INetHandlerPlayServer handler) {
    MCObjectRootScope scope={0}; if (!mc_packet_handler_begin((MCObject *)packet,handler,&scope)) return false;
    bool ok=handler.methods->processCloseWindow && handler.methods->processCloseWindow(handler.instance,packet);
    return mc_packet_handler_finish((MCObject *)packet,&scope,ok);
}
