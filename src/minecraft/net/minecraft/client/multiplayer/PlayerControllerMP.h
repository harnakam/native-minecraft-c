#ifndef C919_SOURCE_PLAYER_CONTROLLER_MP_H
#define C919_SOURCE_PLAYER_CONTROLLER_MP_H
#include "client/network/NetHandlerPlayClient.h"
#include "network/play/client/C0EPacketClickWindow.h"
typedef struct PlayerControllerMP PlayerControllerMP;
typedef struct {
    bool (*addToSendQueue)(MCObject *context, NetHandlerPlayClient *, C0EPacketClickWindow *);
} PlayerControllerMPDependencies;
struct PlayerControllerMP {
    MCObject object;
    NetHandlerPlayClient *netClientHandler;
    MCObject *dependencyContext;
    const PlayerControllerMPDependencies *dependencies;
};
/* Native allocation for windowClick's source field subset. The complete source
   constructor and other controller methods are not implied by this allocation. */
PlayerControllerMP *PlayerControllerMP_nativeNew(MCObjectHeap *, NetHandlerPlayClient *,
                                                 MCObject *context,
                                                 const PlayerControllerMPDependencies *);
bool PlayerControllerMP_isInstance(const MCObject *);
/* out is the exact source result, including normal NULL. false is a native
   dependency/heap failure: discard the complete working client graph. The source
   does not lock out further clicks or undo prediction on a rejected S32 packet. */
bool PlayerControllerMP_windowClick(PlayerControllerMP *, int32_t windowId, int32_t slotId,
                                    int32_t mouseButtonClicked, int32_t mode,
                                    MCGameplayPlayer *playerIn, ItemStack **out);
#endif
