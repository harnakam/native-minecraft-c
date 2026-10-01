#ifndef C919_NATIVE_GAMEPLAY_PLAYER_H
#define C919_NATIVE_GAMEPLAY_PLAYER_H
#include "util/MCGameplayWorld.h"
#include "inventory/ContainerPlayer.h"
#include "nbt/NBTTagCompound.h"
#include "util/NativeJavaUUID.h"
#include "entity/player/PlayerCapabilities.h"

typedef struct StatFileWriter StatFileWriter;
typedef struct EntityPlayerMPWindowsDependencies EntityPlayerMPWindowsDependencies;
/* Explicit native EntityPlayer actor owner. Source inventories/containers own
   direct nullable ItemStack references; no authoritative mc_slot mirror exists.
   EntityPlayer/NetHandler/profile/world methods remain separate dependencies. */
typedef struct MCGameplayPlayer {
    MCObject object;
    MCGameplayWorld *worldObj;
    InventoryPlayer *inventory;
    ContainerPlayer *inventoryContainer;
    Container *openContainer;
    NBTString *name;
    StatFileWriter *stats;
    NBTTagCompound *savedFields;
    NBTString *savedRootName;
    NativeJavaRandom *rand;
    NativeJavaUUID *entityUniqueID;
    /* Existing authenticated/offline profile identity. NULL is permitted only
       for generic native fixtures which have no GameProfile dependency. */
    NativeJavaUUID *gameProfileUUID;
    /* Results of the isolated inherited EntityLivingBase RNG segment, not a
       claim that its attributes/health/complete constructor are translated. */
    float randomUnused1,randomUnused2,rotationYawHead;
    double posX,posY,posZ;
    float rotationYaw,rotationPitch;
    int32_t entityId;
    bool spectator,silent,isChangingQuantityOnly;
    /* Native inherited Entity state, shared by the translated map dependencies. */
    bool isDead;
    bool sleeping,sneaking;
    int32_t dimension;
    MCObject *handler,*effects,*pendingPackets;
    const EntityPlayerMPWindowsDependencies *windowDependencies;
    PlayerCapabilities *capabilities;
} MCGameplayPlayer;
/* Native actor allocation invokes the actual InventoryPlayer/ContainerPlayer
   constructors under a RootScope. Callbacks must be immutable and outlive the
   graph. Stats can be attached later; operations needing it must fail if absent.
   The caller retains the returned actor before the next collection/adoption. */
MCGameplayPlayer *MCGameplayPlayer_new(MCGameplayWorld *,NBTString *,StatFileWriter *,const mc_crafting_dispatch *);
bool MCGameplayPlayer_isInstance(const MCObject *);
InventoryPlayer *MCGameplayPlayer_inventory(MCObject *);
MCObject *MCGameplayPlayer_world(MCObject *);
PlayerCapabilities *MCGameplayPlayer_capabilities(const MCObject *);
bool MCGameplayPlayer_isCreativeMode(const MCObject *);
double MCGameplayPlayer_getDistanceSq(const MCObject *,double x,double y,double z);
#endif
