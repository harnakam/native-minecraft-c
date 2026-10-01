#ifndef C919_SOURCE_ENTITY_H
#define C919_SOURCE_ENTITY_H
#include "util/MCObjectHeap.h"
#include "util/AxisAlignedBB.h"
#include "command/CommandResultStats.h"
#include "entity/DataWatcher.h"
#include "util/NativeJavaRandomRuntime.h"
#include "util/NativeJavaUUID.h"
#include "util/NativeEntityIDRuntime.h"

typedef struct Entity Entity;
typedef struct EntityDependencies {
    bool (*entityInit)(MCObject *context,Entity *);
    bool (*setPosition)(MCObject *context,Entity *,double,double,double);
    bool (*setEntityBoundingBox)(MCObject *context,Entity *,AxisAlignedBB *);
    bool (*getDimensionId)(MCObject *context,MCObject *world,int32_t *out);
    bool (*isRemote)(MCObject *context,MCObject *world,bool *out);
    bool (*moveEntity)(MCObject *context,Entity *,double,double,double);
    const DataWatcherDependencies *watcher;
    bool (*setLocationAndAngles)(MCObject *context,Entity *,double,double,double,float,float);
} EntityDependencies;
/* Complete original Entity instance state. Java references whose classes are
   not yet translated use managed native views. There is no second scalar or
   watcher/Random/UUID owner in subclasses. Native dispatch/context are explicit
   class/runtime boundaries; complete physics and Living/Player remain pending. */
struct Entity {
    MCObject object;
    int32_t entityId;
    double renderDistanceWeight;
    bool preventEntitySpawning;
    Entity *riddenByEntity,*ridingEntity;
    bool forceSpawn;
    MCObject *worldObj;
    double prevPosX,prevPosY,prevPosZ,posX,posY,posZ,motionX,motionY,motionZ;
    float rotationYaw,rotationPitch,prevRotationYaw,prevRotationPitch;
    AxisAlignedBB *boundingBox;
    bool onGround,isCollidedHorizontally,isCollidedVertically,isCollided,velocityChanged,isInWeb,isOutsideBorder,isDead;
    float width,height,prevDistanceWalkedModified,distanceWalkedModified,distanceWalkedOnStepModified,fallDistance;
    int32_t nextStepDistance;
    double lastTickPosX,lastTickPosY,lastTickPosZ;
    float stepHeight;
    bool noClip;
    float entityCollisionReduction;
    NativeJavaRandom *rand;
    int32_t ticksExisted,fireResistance,fire;
    bool inWater;
    int32_t hurtResistantTime;
    bool firstUpdate,isImmuneToFire;
    DataWatcher *dataWatcher;
    double entityRiderPitchDelta,entityRiderYawDelta;
    bool addedToChunk;
    int32_t chunkCoordX,chunkCoordY,chunkCoordZ,serverPosX,serverPosY,serverPosZ;
    bool ignoreFrustumCheck,isAirBorne;
    int32_t timeUntilPortal;
    bool inPortal;
    int32_t portalCounter,dimension;
    DataWatcherBlockPos *lastPortalPos;
    MCObject *lastPortalVec,*teleportDirection;
    bool invulnerable;
    NativeJavaUUID *entityUniqueID;
    CommandResultStats *cmdResultStats;
    const EntityDependencies *entityDependencies;
    MCObject *entityContext;
};
/* Original abstract class constructor, on an already allocated zeroed actual
   subclass. No fabricated base Entity instance or partial constructor fallback.
   Invoked callbacks are required; NULL watcher selects the original inherited
   Entity.onDataWatcherUpdate body. RootScope retains receiver/world/context.
   Counter/seed/Math service effects cannot be undone by graph abort. */
bool Entity_construct(Entity *,MCObject *world,const EntityDependencies *,MCObject *context,
                      NativeJavaRandomRuntime *,NativeEntityIDRuntime *);
bool Entity_isInstance(const MCObject *);
void Entity_traceFields(Entity *,MCObjectVisitor,void *context);
bool Entity_setPosition(Entity *,double,double,double);
bool Entity_setSize(Entity *,float,float);
bool Entity_setLocationAndAngles(Entity *,double,double,double,float,float);
bool Entity_moveToBlockPosAndAngles(Entity *,DataWatcherBlockPos *,float,float);
AxisAlignedBB *Entity_getEntityBoundingBox(Entity *);
bool Entity_setEntityBoundingBox(Entity *,AxisAlignedBB *);
int32_t Entity_getEntityId(const Entity *);
void Entity_setEntityId(Entity *,int32_t);
DataWatcher *Entity_getDataWatcher(Entity *);
NativeJavaUUID *Entity_getUniqueID(Entity *);
CommandResultStats *Entity_getCommandStats(Entity *);
bool Entity_isSilent(Entity *);
bool Entity_setSilent(Entity *,bool);
bool Entity_getFlag(Entity *,int32_t);
bool Entity_setFlag(Entity *,int32_t,bool);
bool Entity_isSneaking(Entity *);
bool Entity_setSneaking(Entity *,bool);

/* Original inherited Entity method. EntityItem has no override. This body is
   intentionally empty in Entity.java; it supplies no native success hook.
   Physics and other virtual methods remain separate dependencies. The receiver
   is the actual managed entity identity. */
void Entity_onDataWatcherUpdate(MCObject *entity, int32_t dataID);
#endif
