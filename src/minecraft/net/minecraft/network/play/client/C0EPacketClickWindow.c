#include "network/play/client/C0EPacketClickWindow.h"
#include "network/play/client/native_packet.h"
static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    C0EPacketClickWindow *packet=(C0EPacketClickWindow *)object;
    packet->clickedItem=(ItemStack *)visitor((MCObject *)packet->clickedItem,context);
}
static const MCObjectClass packet_class={"net.minecraft.network.play.client.C0EPacketClickWindow",MCObjectHeap_plainClone,trace,NULL};
C0EPacketClickWindow *C0EPacketClickWindow_new_empty(MCObjectHeap *heap) {
    return (C0EPacketClickWindow *)MCObjectHeap_alloc(heap,sizeof(C0EPacketClickWindow),&packet_class);
}
C0EPacketClickWindow *C0EPacketClickWindow_new(MCObjectHeap *heap,int32_t windowId,int32_t slotId,
    int32_t usedButton,int32_t mode,ItemStack *clickedItem,int16_t actionNumber) {
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,heap)) return NULL;
    C0EPacketClickWindow *packet=C0EPacketClickWindow_new_empty(heap);
    if (packet) {
        packet->windowId=windowId; packet->slotId=slotId; packet->usedButton=usedButton;
        packet->clickedItem=clickedItem ? ItemStack_copy(heap,clickedItem) : NULL;
        if (clickedItem && !packet->clickedItem) packet=NULL;
        if (packet) { packet->actionNumber=actionNumber; packet->mode=mode; }
    }
    MCObjectRootScope_end(&scope); return packet;
}
bool C0EPacketClickWindow_readPacketData(C0EPacketClickWindow *packet,PacketBuffer *buffer) {
    MCObjectRootScope scope={0}; if (!mc_packet_begin((MCObject *)packet,buffer,&scope)) return false;
    int32_t value=mc_packet_byte(buffer); if (buffer->buffer->failed) goto done; packet->windowId=value;
    value=mc_get_i16(buffer->buffer); if (buffer->buffer->failed) goto done; packet->slotId=value;
    value=mc_packet_byte(buffer); if (buffer->buffer->failed) goto done; packet->usedButton=value;
    value=mc_get_i16(buffer->buffer); if (buffer->buffer->failed) goto done; packet->actionNumber=(int16_t)value;
    value=mc_packet_byte(buffer); if (buffer->buffer->failed) goto done; packet->mode=value;
    if (!PacketBuffer_readItemStackFromBuffer(buffer,&packet->clickedItem)) goto done;
done:
    return mc_packet_finish((MCObject *)packet,buffer,&scope,!buffer->buffer->failed);
}
bool C0EPacketClickWindow_writePacketData(C0EPacketClickWindow *packet,PacketBuffer *buffer) {
    MCObjectRootScope scope={0}; if (!mc_packet_begin((MCObject *)packet,buffer,&scope)) return false;
    mc_put_u8(buffer->buffer,(uint8_t)packet->windowId);
    mc_put_i16(buffer->buffer,mc_packet_short(packet->slotId));
    mc_put_u8(buffer->buffer,(uint8_t)packet->usedButton);
    mc_put_i16(buffer->buffer,packet->actionNumber); mc_put_u8(buffer->buffer,(uint8_t)packet->mode);
    bool ok=PacketBuffer_writeItemStackToBuffer(buffer,packet->clickedItem);
    return mc_packet_finish((MCObject *)packet,buffer,&scope,ok);
}
bool C0EPacketClickWindow_processPacket(C0EPacketClickWindow *packet,INetHandlerPlayServer handler) {
    MCObjectRootScope scope={0}; if (!mc_packet_handler_begin((MCObject *)packet,handler,&scope)) return false;
    bool ok=handler.methods->processClickWindow && handler.methods->processClickWindow(handler.instance,packet);
    return mc_packet_handler_finish((MCObject *)packet,&scope,ok);
}
int32_t C0EPacketClickWindow_getWindowId(const C0EPacketClickWindow *packet) { return packet->windowId; }
int32_t C0EPacketClickWindow_getSlotId(const C0EPacketClickWindow *packet) { return packet->slotId; }
int32_t C0EPacketClickWindow_getUsedButton(const C0EPacketClickWindow *packet) { return packet->usedButton; }
int16_t C0EPacketClickWindow_getActionNumber(const C0EPacketClickWindow *packet) { return packet->actionNumber; }
ItemStack *C0EPacketClickWindow_getClickedItem(const C0EPacketClickWindow *packet) { return packet->clickedItem; }
int32_t C0EPacketClickWindow_getMode(const C0EPacketClickWindow *packet) { return packet->mode; }
