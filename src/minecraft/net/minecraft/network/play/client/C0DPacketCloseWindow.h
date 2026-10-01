#ifndef C919_C0D_PACKET_CLOSE_WINDOW_H
#define C919_C0D_PACKET_CLOSE_WINDOW_H
#include "network/PacketBuffer.h"
#include "network/play/INetHandlerPlayServer.h"
struct C0DPacketCloseWindow { MCObject object; int32_t windowId; };
C0DPacketCloseWindow *C0DPacketCloseWindow_new_empty(MCObjectHeap *);
C0DPacketCloseWindow *C0DPacketCloseWindow_new(MCObjectHeap *,int32_t windowId);
bool C0DPacketCloseWindow_readPacketData(C0DPacketCloseWindow *,PacketBuffer *);
bool C0DPacketCloseWindow_writePacketData(C0DPacketCloseWindow *,PacketBuffer *);
bool C0DPacketCloseWindow_processPacket(C0DPacketCloseWindow *,INetHandlerPlayServer);
bool C0DPacketCloseWindow_isInstance(const MCObject *);
#endif
