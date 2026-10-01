#ifndef C919_SOURCE_ENTITY_ITEM_H
#define C919_SOURCE_ENTITY_ITEM_H
#include "item/ItemStack.h"
#include "entity/DataWatcher.h"
#include "entity/player/InventoryPlayer.h"
#include "nbt/NBTString.h"

typedef struct EntityItem EntityItem;
typedef enum {
    ENTITYITEM_ACH_MINE_WOOD, ENTITYITEM_ACH_KILL_COW, ENTITYITEM_ACH_DIAMONDS,
    ENTITYITEM_ACH_BLAZE_ROD, ENTITYITEM_ACH_DIAMONDS_TO_YOU
} EntityItemAchievement;
/* Required native dispatch to source dependencies which are not yet classes.
   False from an effect callback means failure, not an empty successful hook.
   Semantic predicate false and a missing named player are ordinary results. */
typedef struct {
    bool (*logMissingItem)(MCObject *context,int32_t entityId);
    bool (*isRemote)(MCObject *context,MCObject *world);
    InventoryPlayer *(*inventory)(MCObject *context,MCObject *player);
    const NBTString *(*name)(MCObject *context,MCObject *player);
    MCObject *(*findPlayer)(MCObject *context,MCObject *world,const NBTString *name);
    bool (*triggerAchievement)(MCObject *context,MCObject *player,EntityItemAchievement achievement);
    bool (*isSilent)(MCObject *context,const EntityItem *entity);
    float (*nextFloat)(MCObject *context,EntityItem *entity);
    bool (*playSoundAtEntity)(MCObject *context,MCObject *world,MCObject *player,
                             const char *sound,float volume,float pitch);
    bool (*onItemPickup)(MCObject *context,MCObject *player,EntityItem *entity,int32_t originalCount);
    bool (*setDead)(MCObject *context,EntityItem *entity);
} EntityItemDependencies;
struct EntityItem {
    MCObject object;
    int32_t age,delayBeforeCanPickup,health;
    NBTString *thrower,*owner;
    /* Native storage for inherited Entity state. The actual DataWatcher owns
       slot10; no separate ItemStack owner or metadata value mirror exists.
       Complete inherited Entity/World implementations remain dependencies. */
    MCObject *worldObj,*dependencyContext;
    int32_t entityId;
    double posX,posY,posZ,motionX,motionY,motionZ;
    float hoverStart,rotationYaw,width,height;
    int32_t ticksExisted;
    bool isDead,onGround,noClip;
    DataWatcher *dataWatcher;
    /* Native persistence envelope metadata, distinct from source class fields. */
    NBTTagCompound *savedFields;
    const EntityItemDependencies *dependencies;
};
/* Native allocation only. This is not an original EntityItem constructor:
   base Entity construction, size/position/random/hover and physics are pending.
   The watcher is initially NULL. Explicitly initialize the native inherited
   watcher segment before invoking original watcher-dependent methods. */
EntityItem *EntityItem_nativeNew(MCObjectHeap *,MCObject *world,MCObject *context,const EntityItemDependencies *);
bool EntityItem_isInstance(const MCObject *);
DataWatcher *EntityItem_getDataWatcher(EntityItem *);
/* Original virtual entityInit body: adds the null ItemStack entry10/type5.
   An explicit native adapter for the inherited Entity constructor segment
   creates the watcher, adds source base entries0/1/3/2/4, then dispatches this
   body. It does not implement the complete Entity constructor. NULL methods
   select Entity's actual inherited onDataWatcherUpdate empty body; an explicit
   override/context is retained and traced by DataWatcher. Initialization must
   occur at the inherited constructor's virtual-call point, not afterward. */
bool EntityItem_entityInit(EntityItem *);
bool EntityItem_nativeInitializeDataWatcher(EntityItem *,const DataWatcherDependencies *,MCObject *context);
/* Actual three constructor bodies, with explicit unported inherited Entity
   and Math.random dependencies. baseConstructor invokes its real virtual
   entityInit/DataWatcher work (using the explicit segment adapter above).
   A missing watcher after base construction is failure, never repaired by
   EntityItem afterward. setSize/setPosition implement inherited state.
   Callbacks use this retained context, and false is a native failure. */
typedef struct {
    bool (*baseConstructor)(MCObject *context,EntityItem *,MCObject *world);
    double (*mathRandom)(MCObject *context);
    bool (*setSize)(MCObject *context,EntityItem *,float width,float height);
    bool (*setPosition)(MCObject *context,EntityItem *,double x,double y,double z);
} EntityItemConstructorDependencies;
EntityItem *EntityItem_new_world(MCObjectHeap *,MCObject *world,MCObject *context,
    const EntityItemDependencies *,const EntityItemConstructorDependencies *);
EntityItem *EntityItem_new_position(MCObjectHeap *,MCObject *world,MCObject *context,
    const EntityItemDependencies *,const EntityItemConstructorDependencies *,double x,double y,double z);
EntityItem *EntityItem_new_stack(MCObjectHeap *,MCObject *world,MCObject *context,
    const EntityItemDependencies *,const EntityItemConstructorDependencies *,double x,double y,double z,ItemStack *);
ItemStack *EntityItem_getEntityItem(EntityItem *);
bool EntityItem_setEntityItemStack(EntityItem *,ItemStack *);
bool EntityItem_writeEntityToNBT(EntityItem *,NBTTagCompound *);
ItemStackNBTResult EntityItem_readEntityFromNBT(EntityItem *,NBTTagCompound *);
bool EntityItem_combineItems(EntityItem *,EntityItem *other);
/* Original void method with a completion/failure adapter result. A partial
   insertion can mutate shared references even when no pickup effect occurs. */
bool EntityItem_onCollideWithPlayer(EntityItem *,MCObject *player);
NBTString *EntityItem_getOwner(const EntityItem *);
bool EntityItem_setOwner(EntityItem *,NBTString *);
NBTString *EntityItem_getThrower(const EntityItem *);
bool EntityItem_setThrower(EntityItem *,NBTString *);
int32_t EntityItem_getAge(const EntityItem *);
void EntityItem_setAgeToCreativeDespawnTime(EntityItem *);
void EntityItem_setDefaultPickupDelay(EntityItem *);
void EntityItem_setNoPickupDelay(EntityItem *);
void EntityItem_setInfinitePickupDelay(EntityItem *);
void EntityItem_setPickupDelay(EntityItem *,int32_t ticks);
bool EntityItem_cannotPickup(const EntityItem *);
void EntityItem_setNoDespawn(EntityItem *);
void EntityItem_func_174870_v(EntityItem *);
/* Borrow calls/results under caller RootScope. Trace preserves exact stack,
   world, string and managed callback-context edges across snapshot/adoption.
   No packet/save identity IDs or owned mc_slot mirror exist in this class. */
#endif
