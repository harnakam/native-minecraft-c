#ifndef C919_SOURCE_ENTITY_PLAYER_MP_H
#define C919_SOURCE_ENTITY_PLAYER_MP_H
#include "entity/player/EntityPlayer.h"
#include "server/management/ItemInWorldManager.h"
#include "stats/StatisticsFile.h"
#include "util/BlockPos.h"

typedef struct NetHandlerPlayServer NetHandlerPlayServer;
typedef struct EntityPlayerMPConstructorDependencies {
    const EntityPlayerDependencies *player;
    MCObject *(*newLinkedList)(MCObject *context);
    bool (*currentTimeMillis)(MCObject *context,int64_t *out);
    BlockPos *(*getSpawnPoint)(MCObject *context,MCObject *world);
    MCObject *(*getProvider)(MCObject *context,MCObject *world);
    bool (*getHasNoSky)(MCObject *context,MCObject *provider,bool *out);
    MCObject *(*getWorldInfo)(MCObject *context,MCObject *world);
    bool (*getWorldGameType)(MCObject *context,MCObject *info,const WorldSettingsGameType **out);
    bool (*getSpawnProtectionSize)(MCObject *context,MCObject *server,int32_t *out);
    MCObject *(*getWorldBorder)(MCObject *context,MCObject *world);
    bool (*getClosestDistance)(MCObject *context,MCObject *border,double x,double z,double *out);
    bool (*nextInt)(MCObject *context,NativeJavaRandom *random,int32_t bound,int32_t *out);
    BlockPos *(*getTopSolidOrLiquidBlock)(MCObject *context,MCObject *world,BlockPos *position);
    MCObject *(*getConfigurationManager)(MCObject *context,MCObject *server);
    StatisticsFile *(*getPlayerStatsFile)(MCObject *context,MCObject *configuration,EntityPlayerMP *);
    bool (*moveToBlockPosAndAngles)(MCObject *context,EntityPlayerMP *,BlockPos *,float,float);
    AxisAlignedBB *(*getEntityBoundingBox)(MCObject *context,EntityPlayerMP *);
    MCObject *(*getCollidingBoundingBoxes)(MCObject *context,MCObject *world,EntityPlayerMP *,AxisAlignedBB *);
    bool (*isCollisionListEmpty)(MCObject *context,MCObject *list,bool *out);
    bool (*setPosition)(MCObject *context,EntityPlayerMP *,double,double,double);
} EntityPlayerMPConstructorDependencies;

/* All original instance fields, following the single first-member Player.
   Untranslated reference classes use managed dependency views. Native dispatch
   fields at the end are not alternate source state. */
struct EntityPlayerMP {
    EntityPlayer player;
    NBTString *translator;
    NetHandlerPlayServer *playerNetServerHandler;
    MCObject *mcServer;
    ItemInWorldManager *theItemInWorldManager;
    double managedPosX,managedPosZ;
    MCObject *loadedChunks,*destroyedItemsNetCache;
    StatisticsFile *statsFile;
    float combinedHealth,lastHealth;
    int32_t lastFoodLevel;
    bool wasHungry;
    int32_t lastExperience,respawnInvulnerabilityTicks;
    MCObject *chatVisibility;
    bool chatColours;
    int64_t playerLastActiveTime;
    Entity *spectatingEntity;
    int32_t currentWindowId;
    bool isChangingQuantityOnly;
    int32_t ping;
    bool playerConqueredTheEnd;
    const EntityPlayerMPConstructorDependencies *constructorDependencies;
    MCObject *constructorContext;
};
bool EntityPlayerMP_isInstance(const MCObject *);
EntityPlayerMP *EntityPlayerMP_nativeAllocate(MCObjectHeap *);
MCGameplayPlayer *EntityPlayerMP_asPlayer(EntityPlayerMP *);
bool EntityPlayerMP_construct(EntityPlayerMP *,MCObject *server,MCObject *world,NativeGameProfile *,
    ItemInWorldManager *,const EntityPlayerMPConstructorDependencies *,const mc_crafting_dispatch *,
    MCObject *context,NativeJavaRandomRuntime *,NativeEntityIDRuntime *);
StatisticsFile *EntityPlayerMP_getStatFile(EntityPlayerMP *);
bool EntityPlayerMP_isSpectator(EntityPlayerMP *);
bool EntityPlayerMP_markPlayerActive(EntityPlayerMP *);
int64_t EntityPlayerMP_getLastActiveTime(EntityPlayerMP *);
#endif
