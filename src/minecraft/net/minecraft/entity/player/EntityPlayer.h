#ifndef C919_SOURCE_ENTITY_PLAYER_H
#define C919_SOURCE_ENTITY_PLAYER_H
#include "entity/EntityLivingBase.h"
#include "util/NativeGameProfile.h"
#include "inventory/InventoryEnderChest.h"
#include "inventory/ContainerPlayer.h"
#include "util/FoodStats.h"
#include "entity/player/PlayerCapabilities.h"

typedef struct EntityPlayerDependencies {
    const EntityDependencies *entity;
    const EntityLivingBaseDependencies *living;
    const InventoryBasicDependencies *enderBasic;
    const InventoryEnderChestDependencies *enderChest;
    bool (*isRemote)(MCObject *context,MCObject *world,bool *out);
    DataWatcherBlockPos *(*getSpawnPoint)(MCObject *context,MCObject *world);
    bool (*setLocationAndAngles)(MCObject *context,MCGameplayPlayer *,double,double,double,float,float);
} EntityPlayerDependencies;
typedef struct StatFileWriter StatFileWriter;
typedef struct EntityPlayerMPWindowsDependencies EntityPlayerMPWindowsDependencies;
/* Authoritative state of the translated abstract EntityPlayer. Both the native
   concrete Player and source ACP/SP descendants embed this first-member base.
   Native transport/stat/storage fields follow the original Player fields. */
typedef struct MCGameplayPlayer {
    EntityLivingBase living;
    InventoryPlayer *inventory;
    InventoryEnderChest *theInventoryEnderChest;
    Container *inventoryContainer;
    Container *openContainer;
    FoodStats *foodStats;
    int32_t flyToggleTimer;
    float prevCameraYaw,cameraYaw;
    int32_t xpCooldown;
    double prevChasingPosX,prevChasingPosY,prevChasingPosZ,chasingPosX,chasingPosY,chasingPosZ;
    bool sleeping;
    DataWatcherBlockPos *playerLocation;
    int32_t sleepTimer;
    float renderOffsetX,renderOffsetY,renderOffsetZ;
    DataWatcherBlockPos *spawnChunk;
    bool spawnForced;
    DataWatcherBlockPos *startMinecartRidingCoordinate;
    PlayerCapabilities *capabilities;
    int32_t experienceLevel,experienceTotal;
    float experience;
    int32_t xpSeed;
    ItemStack *itemInUse;
    int32_t itemInUseCount;
    float speedOnGround,speedInAir;
    int32_t lastXPSound;
    NativeGameProfile *gameProfile;
    bool hasReducedDebug;
    MCObject *fishEntity;
    const struct EntityPlayerDependencies *playerDependencies;
    MCObject *playerContext;
    /* Explicit native concrete-subclass/environment state. isSpectator is an
       abstract original method; this flag implements the native subtype. */
    StatFileWriter *stats;
    NBTTagCompound *savedFields;
    NBTString *savedRootName;
    bool spectator,isChangingQuantityOnly;
    MCObject *handler,*effects,*pendingPackets;
    const EntityPlayerMPWindowsDependencies *windowDependencies;
} MCGameplayPlayer;
/* Source class name and the native concrete receiver name designate the same
   authoritative object layout. Neither name allocates a second Player. */
typedef MCGameplayPlayer EntityPlayer;
/* Full source constructor/instance initialization on the actual most-derived
   receiver. Its first-member LivingBase owns Entity fields; no inherited state
   or Player mirror is allocated. */
bool EntityPlayer_construct(MCGameplayPlayer *,MCObject *world,NativeGameProfile *,
    const EntityPlayerDependencies *,const mc_crafting_dispatch *,MCObject *context,
    NativeJavaRandomRuntime *,NativeEntityIDRuntime *);
void EntityPlayer_traceFields(MCGameplayPlayer *,MCObjectVisitor,void *context);
bool EntityPlayer_entityInit(MCGameplayPlayer *);
bool EntityPlayer_applyEntityAttributes(MCGameplayPlayer *);
NativeJavaUUID *EntityPlayer_getUUID(NativeGameProfile *);
NativeJavaUUID *EntityPlayer_getOfflineUUID(MCObjectHeap *,NBTString *);
NBTString *EntityPlayer_getName(MCGameplayPlayer *);
#endif
