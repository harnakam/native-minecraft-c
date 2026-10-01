#ifndef C919_SOURCE_ENTITY_PLAYER_SP_H
#define C919_SOURCE_ENTITY_PLAYER_SP_H
#include "client/network/NetHandlerPlayClient.h"
#include "entity/item/EntityItem.h"
#include "network/play/client/C07PacketPlayerDigging.h"
#include "network/play/client/C0DPacketCloseWindow.h"
#include "stats/StatBase.h"

typedef struct EntityPlayerSP EntityPlayerSP;
typedef struct {
    bool (*addToSendQueue)(MCObject *context, NetHandlerPlayClient *, MCObject *packet);
    DataWatcherBlockPos *(*blockPosOrigin)(MCObject *context);
    /* Actual Minecraft.displayGuiScreen(NULL) dependency. Its current GUI
       retains its old Container, which is closed after openContainer resets. */
    bool (*displayGuiScreenNull)(MCObject *context, MCObject *mc);
} EntityPlayerSPDependencies;
struct EntityPlayerSP {
    MCObject object;
    MCGameplayPlayer *nativeActor;
    NetHandlerPlayClient *sendQueue;
    MCObject *mc, *dependencyContext;
    const EntityPlayerSPDependencies *dependencies;
};
/* Native storage for this source-method subset; not the complete SP/base Entity
   constructor. Context/controller/actor refs are traced and never value mirrors. */
EntityPlayerSP *EntityPlayerSP_nativeNew(MCGameplayPlayer *, NetHandlerPlayClient *, MCObject *mc,
                                         MCObject *context, const EntityPlayerSPDependencies *);
bool EntityPlayerSP_isInstance(const MCObject *);
bool EntityPlayerSP_dropOneItem(EntityPlayerSP *, bool dropAll, EntityItem **out);
bool EntityPlayerSP_closeScreen(EntityPlayerSP *);
bool EntityPlayerSP_closeScreenAndDropStack(EntityPlayerSP *);
/* The supplied original SP join override and EntityPlayer.addStat body are
   empty. These exact empty bodies are not missing production callbacks. The
   inherited triggerAchievement body dispatches to the actual SP addStat. */
bool EntityPlayerSP_joinEntityItemWithWorld(EntityPlayerSP *, EntityItem *);
bool EntityPlayerSP_addStat(EntityPlayerSP *, StatBase *, int32_t amount);
bool EntityPlayerSP_triggerAchievement(EntityPlayerSP *, StatBase *);
#endif
