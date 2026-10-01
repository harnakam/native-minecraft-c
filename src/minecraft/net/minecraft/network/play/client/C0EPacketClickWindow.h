#ifndef C919_C0E_PACKET_CLICK_WINDOW_H
#define C919_C0E_PACKET_CLICK_WINDOW_H
#include "network/PacketBuffer.h"
#include "network/play/INetHandlerPlayServer.h"
struct C0EPacketClickWindow {
    MCObject object;
    int32_t windowId,slotId,usedButton;
    int16_t actionNumber;
    ItemStack *clickedItem;
    int32_t mode;
};
C0EPacketClickWindow *C0EPacketClickWindow_new_empty(MCObjectHeap *);
C0EPacketClickWindow *C0EPacketClickWindow_new(MCObjectHeap *,int32_t windowId,int32_t slotId,
    int32_t usedButton,int32_t mode,ItemStack *clickedItem,int16_t actionNumber);
bool C0EPacketClickWindow_readPacketData(C0EPacketClickWindow *,PacketBuffer *);
bool C0EPacketClickWindow_writePacketData(C0EPacketClickWindow *,PacketBuffer *);
bool C0EPacketClickWindow_processPacket(C0EPacketClickWindow *,INetHandlerPlayServer);
int32_t C0EPacketClickWindow_getWindowId(const C0EPacketClickWindow *);
int32_t C0EPacketClickWindow_getSlotId(const C0EPacketClickWindow *);
int32_t C0EPacketClickWindow_getUsedButton(const C0EPacketClickWindow *);
int16_t C0EPacketClickWindow_getActionNumber(const C0EPacketClickWindow *);
ItemStack *C0EPacketClickWindow_getClickedItem(const C0EPacketClickWindow *);
int32_t C0EPacketClickWindow_getMode(const C0EPacketClickWindow *);
#endif
