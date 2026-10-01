#ifndef C919_SOURCE_NET_HANDLER_PLAY_CLIENT_H
#define C919_SOURCE_NET_HANDLER_PLAY_CLIENT_H
#include "util/MCGameplayPlayer.h"
#include "network/native_packet_thread.h"
#include "network/play/server/S2EPacketCloseWindow.h"
#include "network/play/server/S2FPacketSetSlot.h"
#include "network/play/server/S30PacketWindowItems.h"
#include "network/play/server/S32PacketConfirmTransaction.h"
#include "network/play/server/S1CPacketEntityMetadata.h"
#include "network/play/client/C0FPacketConfirmTransaction.h"
#include "network/play/server/S39PacketPlayerAbilities.h"

typedef struct NetHandlerPlayClient NetHandlerPlayClient;
/* Native bindings for the source Minecraft, GUI, EntityPlayerSP, thread and
   NetworkManager dependencies. Controller/context are managed references;
   immutable methods must outlive the graph. getPlayer reads the controller's
   current player after the thread check, including after player replacement.
   Queue callbacks retain actual packet objects until validated client-frame
   adoption; server queues use their separate durable commitment boundary. */
typedef struct {
    MCPacketThreadResult (*checkThreadAndEnqueue)(MCObject *context, NetHandlerPlayClient *,
                                                  MCObject *packet);
    MCGameplayPlayer *(*getPlayer)(MCObject *context, MCObject *gameController);
    bool (*isCreativeScreen)(MCObject *context, MCObject *gameController);
    int32_t (*selectedCreativeTabIndex)(MCObject *context, MCObject *gameController);
    int32_t (*inventoryCreativeTabIndex)(MCObject *context);
    bool (*closeScreenAndDropStack)(MCObject *context, MCGameplayPlayer *);
    bool (*addToSendQueue)(MCObject *context, NetHandlerPlayClient *,
                           C0FPacketConfirmTransaction *);
    /* WorldClient/Entity dependencies used by the original metadata handler.
       A missing entity is a normal lookup result. A present entity requires
       its actual DataWatcher, with no decoded-slot mirror. */
    MCObject *(*getEntityByID)(MCObject *context, MCGameplayWorld *, int32_t id);
    DataWatcher *(*getDataWatcher)(MCObject *context, MCObject *entity);
} NetHandlerPlayClientDependencies;
struct NetHandlerPlayClient {
    MCObject object;
    MCObject *gameController, *dependencyContext;
    MCGameplayWorld *clientWorldController;
    NativeGameProfile *profile;
    const NetHandlerPlayClientDependencies *dependencies;
};
/* Native partial factory, not the full original constructor. The nullable
   profile is retained before Player/SP construction; no player/world getter is
   invoked here. The caller retains the result before collection/adoption. */
NetHandlerPlayClient *NetHandlerPlayClient_nativeBootstrap(MCObjectHeap *,NativeGameProfile *,
    MCObject *gameController,MCObject *context,const NetHandlerPlayClientDependencies *);
/* Native post-constructor world/owner binding only. It invokes no controller
   callbacks and does not cache a Player across replacements or graph adoption. */
bool NetHandlerPlayClient_nativeBindPlayer(NetHandlerPlayClient *,MCGameplayPlayer *);
/* Original direct getter; NULL profile is returned without an exception. */
NativeGameProfile *NetHandlerPlayClient_getGameProfile(NetHandlerPlayClient *);
/* Existing native fixture factory delegates bootstrap(player.gameProfile)
   then bind. The initial remote actor retains the returned handler. */
NetHandlerPlayClient *NetHandlerPlayClient_nativeNew(MCGameplayPlayer *initialPlayer,
                                                     MCObject *gameController, MCObject *context,
                                                     const NetHandlerPlayClientDependencies *);
bool NetHandlerPlayClient_isInstance(const MCObject *);
INetHandlerPlayClient NetHandlerPlayClient_asHandler(NetHandlerPlayClient *);
bool NetHandlerPlayClient_handleCloseWindow(NetHandlerPlayClient *, S2EPacketCloseWindow *);
bool NetHandlerPlayClient_handleSetSlot(NetHandlerPlayClient *, S2FPacketSetSlot *);
bool NetHandlerPlayClient_handleWindowItems(NetHandlerPlayClient *, S30PacketWindowItems *);
bool NetHandlerPlayClient_handleConfirmTransaction(NetHandlerPlayClient *,
                                                   S32PacketConfirmTransaction *);
bool NetHandlerPlayClient_handlePlayerAbilities(NetHandlerPlayClient *,S39PacketPlayerAbilities *);
bool NetHandlerPlayClient_handleEntityMetadata(NetHandlerPlayClient *, S1CPacketEntityMetadata *);
#endif
