#ifndef C919_SOURCE_PLAYER_CONTROLLER_MP_H
#define C919_SOURCE_PLAYER_CONTROLLER_MP_H
#include "client/network/NetHandlerPlayClient.h"
#include "network/play/client/C0EPacketClickWindow.h"
#include "network/play/client/C09PacketHeldItemChange.h"
#include "network/play/client/C10PacketCreativeInventoryAction.h"
#include "network/play/client/C08PacketPlayerBlockPlacement.h"
#include "item/ItemStackUse.h"
#include "world/WorldSettingsGameType.h"
typedef struct PlayerControllerMP PlayerControllerMP;
typedef struct {
    bool (*addToSendQueue)(MCObject *context, NetHandlerPlayClient *, C0EPacketClickWindow *);
} PlayerControllerMPDependencies;
typedef WorldSettingsGameType PlayerControllerMPGameType;
#define PlayerControllerMP_NOT_SET WorldSettingsGameType_NOT_SET
#define PlayerControllerMP_SURVIVAL WorldSettingsGameType_SURVIVAL
#define PlayerControllerMP_CREATIVE WorldSettingsGameType_CREATIVE
#define PlayerControllerMP_ADVENTURE WorldSettingsGameType_ADVENTURE
#define PlayerControllerMP_SPECTATOR WorldSettingsGameType_SPECTATOR

typedef struct {
    MCGameplayPlayer *(*getPlayer)(MCObject *context, MCObject *mc);
    bool (*addHeldItemToSendQueue)(MCObject *context, NetHandlerPlayClient *,
                                   C09PacketHeldItemChange *);
    bool (*addCreativeItemToSendQueue)(MCObject *context, NetHandlerPlayClient *,
                                       C10PacketCreativeInventoryAction *);
    bool (*addPlacementToSendQueue)(MCObject *context, NetHandlerPlayClient *,
                                    C08PacketPlayerBlockPlacement *);
    const ItemStackUseDependencies *itemUse;
} PlayerControllerMPActionsDependencies;
struct PlayerControllerMP {
    MCObject object;
    NetHandlerPlayClient *netClientHandler;
    MCObject *dependencyContext;
    const PlayerControllerMPDependencies *dependencies;
    MCObject *mc, *actionsContext;
    const PlayerControllerMPActionsDependencies *actionsDependencies;
    const PlayerControllerMPGameType *currentGameType;
    int32_t currentPlayerItem;
};
/* Native allocation for windowClick's source field subset. The complete source
   constructor and other controller methods are not implied by this allocation. */
PlayerControllerMP *PlayerControllerMP_nativeNew(MCObjectHeap *, NetHandlerPlayClient *,
                                                 MCObject *context,
                                                 const PlayerControllerMPDependencies *);
bool PlayerControllerMP_isInstance(const MCObject *);
bool PlayerControllerMP_bindActions(PlayerControllerMP *, MCObject *mc, MCObject *context,
                                    const PlayerControllerMPActionsDependencies *);
bool PlayerControllerMP_setGameType(PlayerControllerMP *,const WorldSettingsGameType *);
bool PlayerControllerMP_setPlayerCapabilities(PlayerControllerMP *,MCGameplayPlayer *);
bool PlayerControllerMP_syncCurrentPlayItem(PlayerControllerMP *);
bool PlayerControllerMP_sendSlotPacket(PlayerControllerMP *, ItemStack *, int32_t slotId);
bool PlayerControllerMP_sendPacketDropItem(PlayerControllerMP *, ItemStack *);
/* out is the original boolean, separate from native failure. The source may
   assign a NULL virtual return before its subsequent dereference fails. */
bool PlayerControllerMP_sendUseItem(PlayerControllerMP *, MCGameplayPlayer *, MCGameplayWorld *,
                                    ItemStack *, bool *out);
/* out is the exact source result, including normal NULL. false is a native
   dependency/heap failure: discard the complete working client graph. The source
   does not lock out further clicks or undo prediction on a rejected S32 packet. */
bool PlayerControllerMP_windowClick(PlayerControllerMP *, int32_t windowId, int32_t slotId,
                                    int32_t mouseButtonClicked, int32_t mode,
                                    MCGameplayPlayer *playerIn, ItemStack **out);
#endif
