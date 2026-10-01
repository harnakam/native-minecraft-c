#ifndef C919_SOURCE_NET_HANDLER_PLAY_SERVER_H
#define C919_SOURCE_NET_HANDLER_PLAY_SERVER_H
#include "util/MCGameplayPlayer.h"
#include "entity/item/EntityItem.h"
#include "network/play/client/C0DPacketCloseWindow.h"
#include "network/play/client/C0EPacketClickWindow.h"
#include "network/play/client/C0FPacketConfirmTransaction.h"
#include "network/play/client/C10PacketCreativeInventoryAction.h"
#include "network/play/client/C13PacketPlayerAbilities.h"
#include "network/play/client/C0BPacketEntityAction.h"
#include "network/native_packet_thread.h"

typedef struct NetHandlerPlayServer NetHandlerPlayServer;
typedef struct NativeRejectedTransactions NativeRejectedTransactions;
/* Native dispatch to the actual source dependencies. Packet effects must be
   retained in the working graph until its durable transaction commits. They
   must not send a socket response from a snapshot that can still be discarded.
   QUEUED models PacketThreadUtil's enqueue and ThreadQuickExitException. */
typedef struct {
    MCPacketThreadResult (*checkThreadAndEnqueue)(MCObject *context, NetHandlerPlayServer *,
                                                  MCObject *packet);
    bool (*markPlayerActive)(MCObject *context, MCGameplayPlayer *);
    bool (*closeContainer)(MCObject *context, MCGameplayPlayer *);
    bool (*sendConfirmTransaction)(MCObject *context, MCGameplayPlayer *, int32_t window,
                                   int16_t action, bool accepted);
    bool (*updateCraftingInventory)(MCObject *context, MCGameplayPlayer *, Container *,
                                    ContainerList *stacks);
    bool (*updateHeldItem)(MCObject *context, MCGameplayPlayer *);
    MCObject *(*getTileEntity)(MCObject *context, MCGameplayWorld *, int32_t x, int32_t y,
                               int32_t z);
    bool (*tileEntityWriteToNBT)(MCObject *context, MCObject *tile, NBTTagCompound *output);
    EntityItem *(*dropPlayerItemWithRandomChoice)(MCObject *context, MCGameplayPlayer *,
                                                  ItemStack *, bool unused);
} NetHandlerPlayServerDependencies;
/* Required original wake/horse methods remain separately bound dependencies.
   instanceof(NULL) is false without invoking the native classifier. A reached
   non-null horse check must classify the real Entity identity; callbacks never
   substitute empty success for wake, jump or GUI effects. */
typedef struct {
    bool (*wakeUpPlayer)(MCObject *context,MCGameplayPlayer *,bool immediately,bool updateWorld,bool setSpawn);
    bool (*isEntityHorse)(MCObject *context,MCObject *entity,bool *out);
    bool (*setJumpPower)(MCObject *context,MCObject *horse,int32_t power);
    bool (*openHorseGUI)(MCObject *context,MCObject *horse,MCGameplayPlayer *);
} NetHandlerPlayServerEntityActionDependencies;
struct NetHandlerPlayServer {
    MCObject object;
    MCGameplayPlayer *playerEntity;
    int32_t itemDropThreshold;
    NativeRejectedTransactions *field_147372_n;
    MCObject *dependencyContext;
    const NetHandlerPlayServerDependencies *dependencies;
    bool hasMoved;
    MCObject *entityActionContext;
    const NetHandlerPlayServerEntityActionDependencies *entityActionDependencies;
};
/* Explicit native allocation for this translated method subset. MinecraftServer,
   NetworkManager, thread scheduling and the full original constructor are
   separate dependencies. The player retains the same handler reference. */
NetHandlerPlayServer *NetHandlerPlayServer_nativeNew(MCGameplayPlayer *, MCObject *context,
                                                     const NetHandlerPlayServerDependencies *);
bool NetHandlerPlayServer_isInstance(const MCObject *);
bool NetHandlerPlayServer_nativeBindEntityActions(NetHandlerPlayServer *,const NetHandlerPlayServerEntityActionDependencies *,MCObject *context);
bool NetHandlerPlayServer_processEntityAction(NetHandlerPlayServer *,C0BPacketEntityAction *);
bool NetHandlerPlayServer_processPlayerAbilities(NetHandlerPlayServer *,C13PacketPlayerAbilities *);
INetHandlerPlayServer NetHandlerPlayServer_asHandler(NetHandlerPlayServer *);
bool NetHandlerPlayServer_processCloseWindow(NetHandlerPlayServer *, C0DPacketCloseWindow *);
bool NetHandlerPlayServer_processClickWindow(NetHandlerPlayServer *, C0EPacketClickWindow *);
bool NetHandlerPlayServer_processConfirmTransaction(NetHandlerPlayServer *,
                                                    C0FPacketConfirmTransaction *);
bool NetHandlerPlayServer_processCreativeInventoryAction(NetHandlerPlayServer *,
                                                         C10PacketCreativeInventoryAction *);
/* Native read-only inspection of the original int-key/Short-value map. Entries
   are not removed by a successful confirm, as in the source. */
bool NetHandlerPlayServer_rejectedAction(const NetHandlerPlayServer *, int32_t window,
                                         int16_t *action);
#endif
