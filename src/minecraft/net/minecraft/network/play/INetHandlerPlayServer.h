#ifndef C919_SOURCE_I_NET_HANDLER_PLAY_SERVER_H
#define C919_SOURCE_I_NET_HANDLER_PLAY_SERVER_H
#include "util/MCObjectHeap.h"
typedef struct C0DPacketCloseWindow C0DPacketCloseWindow;
typedef struct C0EPacketClickWindow C0EPacketClickWindow;
typedef struct C0FPacketConfirmTransaction C0FPacketConfirmTransaction;
typedef struct C10PacketCreativeInventoryAction C10PacketCreativeInventoryAction;
/* Native virtual dispatch for these four original interface methods. Other
   packet handlers await their source ports. Required callback failure represents
   an exception and marks the current heap; it is never an empty success. */
typedef struct {
    bool (*processCloseWindow)(MCObject *,C0DPacketCloseWindow *);
    bool (*processClickWindow)(MCObject *,C0EPacketClickWindow *);
    bool (*processConfirmTransaction)(MCObject *,C0FPacketConfirmTransaction *);
    bool (*processCreativeInventoryAction)(MCObject *,C10PacketCreativeInventoryAction *);
} INetHandlerPlayServerMethods;
typedef struct { MCObject *instance; const INetHandlerPlayServerMethods *methods; } INetHandlerPlayServer;
#endif
