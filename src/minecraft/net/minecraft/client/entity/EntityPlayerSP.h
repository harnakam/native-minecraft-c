#ifndef C919_SOURCE_ENTITY_PLAYER_SP_H
#define C919_SOURCE_ENTITY_PLAYER_SP_H
#include "client/entity/AbstractClientPlayer.h"
#include "client/network/NetHandlerPlayClient.h"
#include "entity/item/EntityItem.h"
#include "network/play/client/C07PacketPlayerDigging.h"
#include "network/play/client/C0DPacketCloseWindow.h"
#include "stats/StatBase.h"
#include "util/MovementInput.h"

typedef struct EntityPlayerSP EntityPlayerSP;

typedef struct {
    const EntityPlayerDependencies *player;
    /* Original virtual getter. NULL with a clear heap reaches the parent
       constructor's later source profile-dereference failure prefix. */
    NativeGameProfile *(*getGameProfile)(MCObject *context,NetHandlerPlayClient *);
} EntityPlayerSPConstructorDependencies;
typedef struct {
    bool (*addToSendQueue)(MCObject *context, NetHandlerPlayClient *, MCObject *packet);
    DataWatcherBlockPos *(*blockPosOrigin)(MCObject *context);
    /* Actual Minecraft.displayGuiScreen(NULL) dependency. Its current GUI
       retains its old Container, which is closed after openContainer resets. */
    bool (*displayGuiScreenNull)(MCObject *context, MCObject *mc);
} EntityPlayerSPDependencies;
/* Actual source virtual calls. Boolean completion is the native exception
   boundary, separate from the returned boolean. GUI/render-view/full SP tick
   implementations are required native dependencies, not invented defaults. */
typedef struct {
    bool (*isSprinting)(MCObject *context,EntityPlayerSP *,bool *out);
    bool (*isSneaking)(MCObject *context,EntityPlayerSP *,bool *out);
    bool (*isCurrentViewEntity)(MCObject *context,EntityPlayerSP *,bool *out);
    AxisAlignedBB *(*getEntityBoundingBox)(MCObject *context,EntityPlayerSP *);
} EntityPlayerSPWalkingDependencies;
struct EntityPlayerSP {
    AbstractClientPlayer clientPlayer;
    NetHandlerPlayClient *sendQueue;
    StatFileWriter *statWriter;
    double lastReportedPosX,lastReportedPosY,lastReportedPosZ;
    float lastReportedYaw,lastReportedPitch;
    bool serverSneakState,serverSprintState;
    int32_t positionUpdateTicks;
    bool hasValidHealth;
    NBTString *clientBrand;
    MovementInput *movementInput;
    MCObject *mc;
    int32_t sprintToggleTimer,sprintingTicksLeft;
    float renderArmYaw,renderArmPitch,prevRenderArmYaw,prevRenderArmPitch;
    int32_t horseJumpPowerCounter;
    float horseJumpPower,timeInPortal,prevTimeInPortal;
    /* Native bindings for unported Minecraft/GUI/current-view services. */
    MCObject *dependencyContext,*walkingContext;
    const EntityPlayerSPDependencies *dependencies;
    const EntityPlayerSPWalkingDependencies *walkingDependencies;
};
MCGameplayPlayer *EntityPlayerSP_asPlayer(EntityPlayerSP *);
MCObject *EntityPlayerSP_asObject(EntityPlayerSP *);
/* Allocation only. Source construction runs on this zeroed most-derived
   object; every parent virtual call retains that same managed identity. */
EntityPlayerSP *EntityPlayerSP_nativeAllocate(MCObjectHeap *);
bool EntityPlayerSP_isInstance(const MCObject *);
bool EntityPlayerSP_construct(EntityPlayerSP *,MCObject *mc,MCObject *world,
    NetHandlerPlayClient *,StatFileWriter *,const EntityPlayerSPConstructorDependencies *,
    const mc_crafting_dispatch *,MCObject *context,NativeJavaRandomRuntime *,NativeEntityIDRuntime *);
/* Action binding never assigns source mc/sendQueue/statWriter. */
bool EntityPlayerSP_bindActions(EntityPlayerSP *,MCObject *context,const EntityPlayerSPDependencies *);
bool EntityPlayerSP_dropOneItem(EntityPlayerSP *, bool dropAll, EntityItem **out);
bool EntityPlayerSP_closeScreen(EntityPlayerSP *);
bool EntityPlayerSP_closeScreenAndDropStack(EntityPlayerSP *);
/* The supplied original SP join override and EntityPlayer.addStat body are
   empty. These exact empty bodies are not missing production callbacks. The
   inherited triggerAchievement body dispatches to the actual SP addStat. */
bool EntityPlayerSP_joinEntityItemWithWorld(EntityPlayerSP *, EntityItem *);
bool EntityPlayerSP_addStat(EntityPlayerSP *, StatBase *, int32_t amount);
bool EntityPlayerSP_triggerAchievement(EntityPlayerSP *, StatBase *);
bool EntityPlayerSP_bindWalking(EntityPlayerSP *,MCObject *context,const EntityPlayerSPWalkingDependencies *);
bool EntityPlayerSP_onUpdateWalkingPlayer(EntityPlayerSP *);
bool EntityPlayerSP_isSneaking(EntityPlayerSP *);
#endif
