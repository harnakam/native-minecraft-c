#ifndef C919_S2E_PACKET_CLOSE_WINDOW_H
#define C919_S2E_PACKET_CLOSE_WINDOW_H
#include "network/PacketBuffer.h"
#include "network/play/INetHandlerPlayClient.h"
struct S2EPacketCloseWindow {
    MCObject object;
    int32_t windowId;
};
S2EPacketCloseWindow *S2EPacketCloseWindow_new_empty(MCObjectHeap *);
S2EPacketCloseWindow *S2EPacketCloseWindow_new(MCObjectHeap *, int32_t windowId);
bool S2EPacketCloseWindow_readPacketData(S2EPacketCloseWindow *, PacketBuffer *);
bool S2EPacketCloseWindow_writePacketData(S2EPacketCloseWindow *, PacketBuffer *);
bool S2EPacketCloseWindow_processPacket(S2EPacketCloseWindow *, INetHandlerPlayClient);
#endif
