#include "client/entity/EntityPlayerSP.h"
#include "stats/StatFileWriter.h"
#include "entity/SharedMonsterAttributes.h"
#include "entity/player/EntityPlayer.h"
#include "util/CombatTracker.h"
#include "util/MCGameplayCrafting.h"
#include "util/MCGameplayPlayer.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      fprintf(stderr, "SP constructor check %u line %d: %s\n", checks,     \
              __LINE__, #x);                                                   \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
enum {
  PROFILE = 128,
  POSITION = 1,
  BOUNDS,
  DIMENSION,
  INIT,
  APPLY,
  GETMAP,
  GETATTRIBUTE,
  SETHEALTH,
  NOTIFY,
  MATH,
  REMOTE,
  RECIPE,
  SPAWN,
  LOCATION
};
static const unsigned sourceOrder[] = {
    POSITION, BOUNDS,    DIMENSION,    INIT,         APPLY,  GETMAP,
    GETMAP,   GETMAP,    GETMAP,       GETATTRIBUTE, GETMAP, GETATTRIBUTE,
    GETMAP,   SETHEALTH, GETATTRIBUTE, GETMAP,       NOTIFY, MATH,
    POSITION, BOUNDS,    MATH,         MATH,         REMOTE, RECIPE,
    SPAWN,    LOCATION,  POSITION,     BOUNDS};
/* Explicit fixture world/subclass callbacks call the actual translated
   superclass, inventories, attributes and recipe manager. They do not replace
   a production World or pretend that unimplemented dependencies succeeded. */
typedef struct {
  MCObject object;
  MCGameplayPlayer *player;
  EntityPlayerSP *sp;
  NetHandlerPlayClient *queue,*earlyQueue;
  StatFileWriter *stats;
  MCObject *mc;
  NativeGameProfile *profile, *replacementProfile, *capturedProfile;
  DataWatcherBlockPos *spawn;
  CraftingManager *manager;
  NativeJavaUUID *generated;
  NativeJavaRandomRuntime *random;
  NativeEntityIDRuntime *ids;
  NativeJavaRandomState mathState;
  unsigned events[128], count, failAt, mathCount;
  bool remote, mutate, nullSpawn;
  unsigned replaceProfile;
} Witness;
static void trace(MCObject *o, MCObjectVisitor visit, void *context) {
  Witness *w = (Witness *)o;
  w->player = (MCGameplayPlayer *)visit((MCObject *)w->player, context);
  w->sp = (EntityPlayerSP *)visit((MCObject *)w->sp,context);
  w->queue = (NetHandlerPlayClient *)visit((MCObject *)w->queue,context);
  w->earlyQueue = (NetHandlerPlayClient *)visit((MCObject *)w->earlyQueue,context);
  w->stats = (StatFileWriter *)visit((MCObject *)w->stats,context);
  w->mc = visit(w->mc,context);
  w->profile = (NativeGameProfile *)visit((MCObject *)w->profile, context);
  w->replacementProfile = (NativeGameProfile *)visit((MCObject *)w->replacementProfile, context);
  w->capturedProfile = (NativeGameProfile *)visit((MCObject *)w->capturedProfile, context);
  w->spawn = (DataWatcherBlockPos *)visit((MCObject *)w->spawn, context);
  w->manager = (CraftingManager *)visit((MCObject *)w->manager, context);
  w->generated = (NativeJavaUUID *)visit((MCObject *)w->generated, context);
}
static const MCObjectClass witnessClass = {
    "fixture.Player.constructor", MCObjectHeap_plainClone, trace, NULL};
static bool record(Witness *w, unsigned kind) {
  CHECK(w->count < sizeof w->events / sizeof *w->events);
  w->events[w->count++] = kind;
  return !w->failAt || w->count != w->failAt;
}
static bool nano(void *context, int64_t *out) {
  (void)context;
  *out = 123;
  return true;
}
static const NativeJavaRandomRuntimeDependencies randomDependencies = {nano};
static void leaves_null(MCGameplayPlayer *p) {
  CHECK(!p->inventory && !p->theInventoryEnderChest && !p->foodStats &&
        !p->capabilities);
  CHECK(!p->inventoryContainer && !p->openContainer && !p->gameProfile);
}
static bool position(MCObject *context, Entity *e, double x, double y,
                     double z) {
  return record((Witness *)context, POSITION) && Entity_setPosition(e, x, y, z);
}
static bool bounds(MCObject *context, Entity *e, AxisAlignedBB *box) {
  return record((Witness *)context, BOUNDS) &&
         Entity_setEntityBoundingBox(e, box);
}
static bool dimension(MCObject *context, MCObject *world, int32_t *out) {
  CHECK(context == world);
  if (!record((Witness *)context, DIMENSION))
    return false;
  *out = 42;
  return true;
}
static bool watcher(MCObject *context, MCObject *owner, int32_t id) {
  Witness *w = (Witness *)context;
  CHECK(owner == (MCObject *)w->player && id == 6);
  leaves_null(w->player);
  return record(w, NOTIFY);
}
static const DataWatcherDependencies watcherDependencies = {
    .onDataWatcherUpdate = watcher};
static bool init(MCObject *context, Entity *e) {
  Witness *w = (Witness *)context;
  MCGameplayPlayer *p = (MCGameplayPlayer *)e;
  if (!record(w, INIT))
    return false;
  leaves_null(p);
  CHECK((MCObject *)w->sp==(MCObject *)p);
  CHECK(!w->sp->sendQueue && !w->sp->statWriter && !w->sp->mc);
  CHECK(!w->sp->clientPlayer.playerInfo && !w->sp->movementInput);
  CHECK(w->sp->lastReportedPosX==0 && !w->sp->hasValidHealth);
  CHECK(!p->living.attributeMap && !p->living._combatTracker &&
        !p->living.previousEquipment);
  CHECK(e->rand && e->entityUniqueID && e->cmdResultStats);
  CHECK(WatchableObjectList_size(DataWatcher_getAllWatched(e->dataWatcher)) ==
        5);
  w->generated = e->entityUniqueID;
  CHECK(EntityPlayer_entityInit(p));
  if(w->replaceProfile==2)w->queue->profile=w->replacementProfile;
  CHECK(WatchableObjectList_size(DataWatcher_getAllWatched(e->dataWatcher)) ==
        13);
  CHECK(EntityLivingBase_getHealth(&p->living) == 1);
  if (w->mutate) {
    w->sp->lastReportedPosX=17.5;
    w->sp->hasValidHealth=true;
    w->sp->sprintingTicksLeft=23;
    w->sp->movementInput=MovementInput_new(context->heap);
    CHECK(w->sp->movementInput);
    w->sp->movementInput->sneak=true;
    w->sp->clientPlayer.playerInfo=(MCObject *)w->spawn;
    w->sp->sendQueue=w->earlyQueue;
    w->sp->statWriter=w->stats;
    w->sp->mc=(MCObject *)w->spawn;
    p->living.entity.dimension=77;
    p->sleeping = true;
    p->flyToggleTimer = 17;
    p->xpSeed = 19;
    p->experience = 0.5f;
    p->speedOnGround = 99;
    p->speedInAir = 99;
    p->hasReducedDebug = true;
    MCObjectHeap_touch(context->heap);
  }
  return true;
}
static BaseAttributeMap *get_map(MCObject *context, EntityLivingBase *l) {
  if (!record((Witness *)context, GETMAP))
    return NULL;
  return EntityLivingBase_getAttributeMap(l);
}
static IAttributeInstance *get_attribute(MCObject *context, EntityLivingBase *l,
                                         IAttribute *attribute) {
  if (!record((Witness *)context, GETATTRIBUTE))
    return NULL;
  return EntityLivingBase_getEntityAttribute(l, attribute);
}
static bool apply(MCObject *context, EntityLivingBase *l) {
  Witness *w = (Witness *)context;
  if (!record(w, APPLY))
    return false;
  leaves_null((MCGameplayPlayer *)l);
  CHECK(l->_combatTracker && l->_combatTracker->fighter == l);
  CHECK(l->previousEquipment && l->previousEquipment->length == 5 &&
        l->activePotionsMap);
  return EntityPlayer_applyEntityAttributes((MCGameplayPlayer *)l);
}
static bool health(MCObject *context, EntityLivingBase *l, float amount) {
  return record((Witness *)context, SETHEALTH) &&
         EntityLivingBase_setHealth(l, amount);
}
static bool math_random(MCObject *context, double *out) {
  Witness *w = (Witness *)context;
  if (!record(w, MATH))
    return false;
  leaves_null(w->player);
  ++w->mathCount;
  return NativeJavaRandomState_nextDouble(&w->mathState, out);
}
static const EntityDependencies entityDependencies = {
    .entityInit = init,
    .setPosition = position,
    .setEntityBoundingBox = bounds,
    .getDimensionId = dimension,
    .watcher = &watcherDependencies};
static const EntityLivingBaseDependencies livingDependencies = {
    apply, get_map, get_attribute, health, math_random};
static bool remote(MCObject *context, MCObject *world, bool *out) {
  Witness *w = (Witness *)context;
  CHECK(context == world);
  if (!record(w, REMOTE))
    return false;
  CHECK(w->player->inventory && w->player->capabilities &&
        w->player->gameProfile == w->capturedProfile);
  CHECK(!w->player->inventoryContainer && !w->player->openContainer);
  *out = w->remote;
  return true;
}
static DataWatcherBlockPos *spawn(MCObject *context, MCObject *world) {
  Witness *w = (Witness *)context;
  CHECK(context == world);
  if (!record(w, SPAWN) || w->nullSpawn)
    return NULL;
  CHECK(w->player->inventoryContainer &&
        w->player->openContainer == w->player->inventoryContainer);
  return w->spawn;
}
static bool location(MCObject *context, MCGameplayPlayer *p, double x, double y,
                     double z, float yaw, float pitch) {
  Witness *w = (Witness *)context;
  if (!record(w, LOCATION))
    return false;
  CHECK(p->gameProfile == w->capturedProfile &&
        p->inventoryContainer == p->openContainer);
  CHECK(p->living.unused180 == 0 && p->living.entity.fireResistance == 1);
  return Entity_setLocationAndAngles(&p->living.entity, x, y, z, yaw, pitch);
}
static const EntityPlayerDependencies playerDependencies = {
    .entity = &entityDependencies,
    .living = &livingDependencies,
    .isRemote = remote,
    .getSpawnPoint = spawn,
    .setLocationAndAngles = location};
static InventoryPlayer *inventory(MCObject *p) {
  return ((MCGameplayPlayer *)p)->inventory;
}
static MCObject *world(MCObject *p) {
  return ((MCGameplayPlayer *)p)->living.entity.worldObj;
}
static NBTString *unexpected_display(MCObject *c, const ItemStack *s) {
  (void)c;
  (void)s;
  CHECK(false);
  return NULL;
}
static ItemStack *recipe(InventoryCrafting *grid, MCObject *o) {
  Witness *w = (Witness *)o;
  if (!record(w, RECIPE)) {
    MCObjectHeap_fail(o->heap);
    return NULL;
  }
  CHECK(w->player->inventory && w->player->theInventoryEnderChest &&
        w->player->foodStats && w->player->capabilities);
  CHECK(w->player->gameProfile == w->capturedProfile &&
        !w->player->inventoryContainer && !w->player->openContainer);
  return CraftingManager_findMatchingRecipe(w->manager, grid, o,
                                            unexpected_display, NULL);
}
static int32_t unexpected_armor(const Item *item) {
  (void)item;
  CHECK(false);
  return -1;
}
static bool unexpected_drop(MCObject *p, ItemStack *s, bool scattered) {
  (void)p;
  (void)s;
  (void)scattered;
  CHECK(false);
  return false;
}
static ItemStackArray *unexpected_remaining(InventoryCrafting *g, MCObject *w) {
  (void)g;
  (void)w;
  CHECK(false);
  return NULL;
}
static bool unexpected_craft(ItemStack *s, MCObject *w, MCObject *p,
                             int32_t n) {
  (void)s;
  (void)w;
  (void)p;
  (void)n;
  CHECK(false);
  return false;
}
static bool unexpected_achievement(MCObject *p, mc_crafting_achievement a) {
  (void)p;
  (void)a;
  CHECK(false);
  return false;
}
static bool unexpected_kind(const Item *i) {
  (void)i;
  CHECK(false);
  return false;
}
static const mc_crafting_dispatch craftingDependencies = {
    .inventory = inventory,
    .world = world,
    .findMatchingRecipe = recipe,
    .armorType = unexpected_armor,
    .drop = unexpected_drop,
    .getRemainingItems = unexpected_remaining,
    .onCrafting = unexpected_craft,
    .triggerAchievement = unexpected_achievement,
    .isPickaxe = unexpected_kind,
    .isHoe = unexpected_kind,
    .isSword = unexpected_kind,
    .isWoodPickaxe = unexpected_kind};

static MCPacketThreadResult unused_thread(MCObject *c,NetHandlerPlayClient *h,MCObject *p) {
  (void)c;(void)h;(void)p;CHECK(false);return MC_PACKET_THREAD_FAILED;
}
static MCGameplayPlayer *unused_player(MCObject *c,MCObject *mc) {
  (void)c;(void)mc;CHECK(false);return NULL;
}
static bool unused_screen(MCObject *c,MCObject *mc) {
  (void)c;(void)mc;CHECK(false);return false;
}
static int32_t unused_tab(MCObject *c,MCObject *mc) {
  (void)c;(void)mc;CHECK(false);return 0;
}
static int32_t unused_inventory_tab(MCObject *c) {(void)c;CHECK(false);return 0;}
static bool unused_close(MCObject *c,MCGameplayPlayer *p) {(void)c;(void)p;CHECK(false);return false;}
static bool unused_confirm(MCObject *c,NetHandlerPlayClient *h,C0FPacketConfirmTransaction *p) {
  (void)c;(void)h;(void)p;CHECK(false);return false;
}
static const NetHandlerPlayClientDependencies handlerDependencies={
  .checkThreadAndEnqueue=unused_thread,.getPlayer=unused_player,.isCreativeScreen=unused_screen,
  .selectedCreativeTabIndex=unused_tab,.inventoryCreativeTabIndex=unused_inventory_tab,
  .closeScreenAndDropStack=unused_close,.addToSendQueue=unused_confirm};
static NativeGameProfile *get_profile(MCObject *context,NetHandlerPlayClient *handler) {
  Witness *w=(Witness *)context;
  CHECK(handler==w->queue && !w->player->living.entity.rand && !w->player->inventory);
  if(!record(w,PROFILE)){MCObjectHeap_fail(context->heap);return NULL;}
  if(w->replaceProfile==1)handler->profile=w->replacementProfile;
  w->capturedProfile=NetHandlerPlayClient_getGameProfile(handler);
  return w->capturedProfile;
}
static const EntityPlayerSPConstructorDependencies constructorDependencies={&playerDependencies,get_profile};
static Witness *setup(MCObjectHeap *heap) {
  Witness *w=(Witness *)MCObjectHeap_alloc(heap,sizeof *w,&witnessClass);CHECK(w);
  w->random=NativeJavaRandomRuntime_new(&randomDependencies,NULL);w->ids=NativeEntityIDRuntime_new(100);
  CHECK(w->random && w->ids && NativeJavaRandomState_setSeed(&w->mathState,0));
  w->sp=EntityPlayerSP_nativeAllocate(heap);CHECK(w->sp);
  w->player=EntityPlayerSP_asPlayer(w->sp);CHECK((MCObject *)w->player==EntityPlayerSP_asObject(w->sp));
  w->spawn=DataWatcher_blockPos(heap,-3,63,8);CHECK(w->spawn);
  NativeJavaUUID *id=NativeJavaUUID_new(heap,INT64_C(0x123456789abcdef),-7);CHECK(id);
  NBTString *name=NBTString_fromUTF8(heap,"SPTest");CHECK(name);
  w->profile=NativeGameProfile_new(heap,id,name);CHECK(w->profile);
  w->replacementProfile=NativeGameProfile_new(heap,NativeJavaUUID_new(heap,7,9),NBTString_fromUTF8(heap,"ChangedSP"));CHECK(w->replacementProfile);
  w->manager=MCGameplayCrafting_newManager(heap);CHECK(w->manager);
  w->stats=StatFileWriter_new(heap);CHECK(w->stats);w->mc=(MCObject *)w->spawn;w->remote=true;
  w->queue=NetHandlerPlayClient_nativeBootstrap(heap,w->profile,(MCObject *)w,(MCObject *)w,&handlerDependencies);
  w->earlyQueue=NetHandlerPlayClient_nativeBootstrap(heap,w->profile,(MCObject *)w,(MCObject *)w,&handlerDependencies);
  CHECK(w->queue && w->earlyQueue);return w;
}
static bool construct(Witness *w,MCObject *worldObject,NetHandlerPlayClient *queueObject) {
  return EntityPlayerSP_construct(w->sp,w->mc,worldObject,queueObject,w->stats,
     &constructorDependencies,&craftingDependencies,(MCObject *)w,w->random,w->ids);
}
static void release(Witness *w) {
  CHECK(NativeJavaRandomRuntime_free(w->random) &&
        NativeEntityIDRuntime_free(w->ids));
}

static void successful_single_receiver(void) {
  for(unsigned mode=0;mode<3;mode++) {
    MCObjectHeap *h=MCObjectHeap_new(32*1024*1024);CHECK(h);MCObjectRootScope scope={0};
    CHECK(MCObjectRootScope_begin(&scope,h));Witness *w=setup(h);CHECK(MCObjectRootScope_pin(&scope,(MCObject *)w));
    w->mutate=mode!=0;if(mode==2){w->stats=NULL;w->mc=NULL;}
    CHECK(construct(w,(MCObject *)w,w->queue));
    MCGameplayPlayer *p=w->player;Entity *e=&p->living.entity;
    CHECK(EntityPlayerSP_isInstance((MCObject *)p) && AbstractClientPlayer_isInstance((MCObject *)p));
    CHECK(MCGameplayPlayer_isInstance((MCObject *)p) && EntityLivingBase_isInstance((MCObject *)p) && Entity_isInstance((MCObject *)p));
    CHECK(w->count==1+sizeof sourceOrder/sizeof *sourceOrder && w->events[0]==PROFILE);
    for(unsigned i=0;i<sizeof sourceOrder/sizeof *sourceOrder;i++)CHECK(w->events[i+1]==sourceOrder[i]);
    CHECK(e->entityId==100 && e->dimension==0 && e->worldObj==(MCObject *)w);
    CHECK(w->sp->sendQueue==w->queue && w->sp->statWriter==w->stats && w->sp->mc==w->mc);
    CHECK(p->gameProfile==w->profile && e->entityUniqueID==w->profile->id);
    CHECK(DataWatcher_isInstance((MCObject *)e->dataWatcher) && p->inventory->player==(MCObject *)w->sp);
    CHECK(p->living._combatTracker->fighter==&p->living);
    CHECK(((ContainerPlayer *)p->inventoryContainer)->thePlayer==(MCObject *)w->sp && p->openContainer==p->inventoryContainer);
    CHECK(e->posX==-2.5 && e->posY==64 && e->posZ==8.5 && p->living.unused180==180);
    if(w->mutate)CHECK(w->sp->lastReportedPosX==17.5 && w->sp->hasValidHealth && w->sp->sprintingTicksLeft==23 && w->sp->movementInput->sneak && w->sp->clientPlayer.playerInfo==(MCObject *)w->spawn);
    release(w);MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
  }
}
static void failure_prefixes(void) {
  for(unsigned failure=1;failure<=1+sizeof sourceOrder/sizeof *sourceOrder;failure++) {
    MCObjectHeap *h=MCObjectHeap_new(32*1024*1024);CHECK(h);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));
    Witness *w=setup(h);w->failAt=failure;CHECK(!construct(w,(MCObject *)w,w->queue));
    CHECK(MCObjectHeap_failed(h) && w->count==failure);
    CHECK(!w->sp->sendQueue && !w->sp->statWriter && !w->sp->mc);
    if(failure==1)CHECK(!w->player->living.entity.rand && !w->player->inventory);
    release(w);MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
  }
  for(unsigned kind=0;kind<2;kind++) {
    MCObjectHeap *h=MCObjectHeap_new(32*1024*1024);CHECK(h);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));
    Witness *w=setup(h);if(kind==1)w->queue->profile=NULL;
    CHECK(!construct(w,(MCObject *)w,kind==0?NULL:w->queue));
    CHECK(MCObjectHeap_failed(h));CHECK(w->count==(kind==0?0:23));
    CHECK(!w->sp->sendQueue && !w->sp->statWriter && !w->sp->mc);
    CHECK((w->player->living.entity.rand==NULL)==(kind==0));
    if(kind==1)CHECK(w->player->inventory && w->player->capabilities && !w->player->gameProfile);
    release(w);MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
  }
}
static void captured_profile_and_lifetime(void) {
  for(unsigned change=1;change<=2;change++) {
    MCObjectHeap *h=MCObjectHeap_new(32*1024*1024);CHECK(h);
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));
    Witness *w=setup(h);w->replaceProfile=change;w->mutate=true;
    CHECK(construct(w,(MCObject *)w,w->queue));
    CHECK(w->queue->profile==w->replacementProfile);
    CHECK(w->player->gameProfile==(change==1?w->replacementProfile:w->profile));
    CHECK(w->player->living.entity.entityUniqueID==w->player->gameProfile->id);
    w->sp->clientBrand=NBTString_fromUTF8(h,"source-client-brand");CHECK(w->sp->clientBrand);
    w->sp->dependencyContext=(MCObject *)w->spawn;
    w->sp->walkingContext=(MCObject *)w->stats;
    CHECK(MCGameplayPlayer_nativeAttachEnvironment(w->player,w->stats));
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)w));
    MCObjectRootScope_end(&scope);CHECK(MCObjectHeap_collect(h));
    w=(Witness *)MCObjectRoot_get(&root);CHECK(w->sp->sendQueue==w->queue);
    MCObjectHeap *working=MCObjectHeap_clone(h);CHECK(working);
    MCObjectRoot workRoot={0};CHECK(MCObjectRoot_rebind(&workRoot,working,&root));
    Witness *copy=(Witness *)MCObjectRoot_get(&workRoot);CHECK(copy && copy!=w);
    EntityPlayerSP *sp=copy->sp;MCGameplayPlayer *player=copy->player;
    CHECK((MCObject *)sp==(MCObject *)player && EntityPlayerSP_asPlayer(sp)==player);
    CHECK(sp!=w->sp && sp->sendQueue==copy->queue && sp->statWriter==copy->stats);
    CHECK(sp->mc==copy->mc && sp->clientPlayer.playerInfo==(MCObject *)copy->spawn);
    CHECK(sp->dependencyContext==(MCObject *)copy->spawn && sp->walkingContext==(MCObject *)copy->stats);
    CHECK(sp->clientBrand!=w->sp->clientBrand && NBTString_isInstance((MCObject *)sp->clientBrand));
    CHECK(sp->movementInput!=w->sp->movementInput && sp->movementInput->sneak);
    CHECK(player->inventory->player==(MCObject *)sp && player->living._combatTracker->fighter==&player->living);
    CHECK(((ContainerPlayer *)player->inventoryContainer)->thePlayer==(MCObject *)sp);
    CHECK(player->openContainer==player->inventoryContainer && player->stats==copy->stats && player->savedFields);
    CHECK(player->gameProfile==(change==1?copy->replacementProfile:copy->profile));
    CHECK(MCObjectHeap_collect(working));CHECK(MCObjectHeap_adopt(h,working));
    CHECK(MCObjectRoot_rebind(&workRoot,h,&workRoot));MCObjectHeap_free(working);
    copy=(Witness *)MCObjectRoot_get(&root);CHECK(copy && copy->sp==sp);
    CHECK(MCObjectHeap_collect(h));CHECK(copy->sp->clientPlayer.playerInfo==(MCObject *)copy->spawn);
    release(copy);MCObjectRoot_drop(&workRoot);MCObjectRoot_drop(&root);MCObjectHeap_free(h);
  }
}
static void native_argument_boundaries(void) {
  for(unsigned kind=0;kind<6;kind++) {
    MCObjectHeap *h=MCObjectHeap_new(32*1024*1024);CHECK(h);
    MCObjectHeap *foreign=MCObjectHeap_new(1024*1024);CHECK(foreign);
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));Witness *w=setup(h);
    EntityPlayerSPConstructorDependencies deps=constructorDependencies;
    MCObject *ctx=(MCObject *)w,*mc=w->mc,*worldObject=(MCObject *)w;
    StatFileWriter *stats=w->stats;EntityPlayerSP *sp=w->sp;
    if(kind==0)deps.getGameProfile=NULL;
    if(kind==1)ctx=(MCObject *)DataWatcher_blockPos(foreign,0,0,0);
    if(kind==2)mc=(MCObject *)DataWatcher_blockPos(foreign,0,0,0);
    if(kind==3)stats=StatFileWriter_new(foreign);
    if(kind==4)worldObject=(MCObject *)DataWatcher_blockPos(foreign,0,0,0);
    if(kind==5)sp=(EntityPlayerSP *)MCObjectHeap_alloc(h,sizeof(MCObject),((MCObject *)w->sp)->klass);
    CHECK(!EntityPlayerSP_construct(sp,mc,worldObject,w->queue,stats,&deps,
                                  &craftingDependencies,ctx,w->random,w->ids));
    CHECK(MCObjectHeap_failed(h) && w->count==0 && !w->player->living.entity.rand);
    CHECK(!MCObjectHeap_failed(foreign));release(w);MCObjectRootScope_end(&scope);
    CHECK(!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);MCObjectHeap_free(foreign);
  }
  MCObjectHeap *tiny=MCObjectHeap_new(sizeof(MCObject));CHECK(tiny);
  CHECK(!EntityPlayerSP_nativeAllocate(tiny) && MCObjectHeap_failed(tiny));MCObjectHeap_free(tiny);
}
int main(void) {
  successful_single_receiver();failure_prefixes();captured_profile_and_lifetime();native_argument_boundaries();
  printf("Source SP constructor: %u checks passed\n",checks);return 0;
}
