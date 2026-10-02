#include "entity/DataWatcher.h"
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
      fprintf(stderr, "player constructor check %u line %d: %s\n", checks,     \
              __LINE__, #x);                                                   \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
enum {
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
  NativeGameProfile *profile;
  DataWatcherBlockPos *spawn;
  CraftingManager *manager;
  NativeJavaUUID *generated;
  NativeJavaRandomRuntime *random;
  NativeEntityIDRuntime *ids;
  NativeJavaRandomState mathState;
  unsigned events[128], count, failAt, mathCount;
  bool remote, mutate, nullSpawn;
} Witness;
static void trace(MCObject *o, MCObjectVisitor visit, void *context) {
  Witness *w = (Witness *)o;
  w->player = (MCGameplayPlayer *)visit((MCObject *)w->player, context);
  w->profile = (NativeGameProfile *)visit((MCObject *)w->profile, context);
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
  CHECK(!p->living.attributeMap && !p->living._combatTracker &&
        !p->living.previousEquipment);
  CHECK(e->rand && e->entityUniqueID && e->cmdResultStats);
  CHECK(WatchableObjectList_size(DataWatcher_getAllWatched(e->dataWatcher)) ==
        5);
  w->generated = e->entityUniqueID;
  CHECK(EntityPlayer_entityInit(p));
  CHECK(WatchableObjectList_size(DataWatcher_getAllWatched(e->dataWatcher)) ==
        13);
  CHECK(EntityLivingBase_getHealth(&p->living) == 1);
  if (w->mutate) {
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
        w->player->gameProfile == w->profile);
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
  CHECK(p->gameProfile == w->profile &&
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
  CHECK(w->player->gameProfile == w->profile &&
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
static Witness *setup(MCObjectHeap *heap) {
  Witness *w = (Witness *)MCObjectHeap_alloc(heap, sizeof *w, &witnessClass);
  CHECK(w);
  w->random = NativeJavaRandomRuntime_new(&randomDependencies, NULL);
  w->ids = NativeEntityIDRuntime_new(100);
  CHECK(w->random && w->ids && NativeJavaRandomState_setSeed(&w->mathState, 0));
  w->player = MCGameplayPlayer_nativeAllocate(heap);
  CHECK(w->player);
  w->spawn = DataWatcher_blockPos(heap, -3, 63, 8);
  CHECK(w->spawn);
  NativeJavaUUID *id = NativeJavaUUID_new(heap, INT64_C(0x123456789abcdef), -7);
  CHECK(id);
  NBTString *name = NBTString_fromUTF8(heap, "PlayerTest");
  CHECK(name);
  w->profile = NativeGameProfile_new(heap, id, name);
  CHECK(w->profile);
  w->manager = MCGameplayCrafting_newManager(heap);
  CHECK(w->manager);
  return w;
}
static bool construct(Witness *w, MCObject *worldObject,
                      NativeGameProfile *profile) {
  return EntityPlayer_construct(w->player, worldObject, profile,
                                &playerDependencies, &craftingDependencies,
                                (MCObject *)w, w->random, w->ids);
}
static void release(Witness *w) {
  CHECK(NativeJavaRandomRuntime_free(w->random) &&
        NativeEntityIDRuntime_free(w->ids));
}
static uint32_t bits(float f) {
  uint32_t value;
  memcpy(&value, &f, sizeof value);
  return value;
}
static void final_state(Witness *w) {
  MCGameplayPlayer *p = w->player;
  Entity *e = &p->living.entity;
  CHECK(e->entityId == 100 && e->dimension == 42 &&
        e->entityUniqueID == w->profile->id &&
        e->entityUniqueID != w->generated);
  CHECK(e->worldObj == (MCObject *)w && p->gameProfile == w->profile &&
        EntityPlayer_getName(p) == w->profile->name);
  CHECK(e->posX == -2.5 && e->posY == 64 && e->posZ == 8.5);
  CHECK(e->prevPosX == e->posX && e->prevPosY == e->posY &&
        e->prevPosZ == e->posZ);
  CHECK(e->rotationYaw == 0 && e->rotationPitch == 0 &&
        p->living.unused180 == 180 && e->fireResistance == 20);
  CHECK(bits(p->living.randomUnused1) == UINT32_C(0x3c8dcd06) &&
        bits(p->living.randomUnused2) == UINT32_C(0x453a62bb));
  CHECK(bits(p->living.rotationYawHead) == UINT32_C(0x4080290f) &&
        w->mathCount == 3);
  CHECK(e->preventEntitySpawning && e->stepHeight == 0.6f && e->width == 0.6f &&
        e->height == 1.8f);
  CHECK(e->boundingBox->minX == e->posX - (double)(e->width / 2) &&
        e->boundingBox->minY == 64);
  CHECK(e->boundingBox->maxZ == e->posZ + (double)(e->width / 2) &&
        e->boundingBox->maxY == 64 + (double)e->height);
  CHECK(p->inventory->player == (MCObject *)p &&
        p->inventory->mainInventory->length == 36 &&
        p->inventory->armorInventory->length == 4);
  for (int i = 0; i < 36; i++)
    CHECK(p->inventory->mainInventory->items[i] == NULL);
  for (int i = 0; i < 4; i++)
    CHECK(p->inventory->armorInventory->items[i] == NULL);
  CHECK(p->inventory->currentItem == 0 && !p->inventory->itemStack &&
        !p->inventory->inventoryChanged);
  CHECK(p->theInventoryEnderChest->basic.slotsCount == 27 &&
        p->theInventoryEnderChest->basic.inventoryContents->length == 27);
  CHECK(!p->theInventoryEnderChest->associatedChest &&
        !p->theInventoryEnderChest->basic.hasCustomName);
  for (int i = 0; i < 27; i++)
    CHECK(p->theInventoryEnderChest->basic.inventoryContents->items[i] == NULL);
  CHECK(p->foodStats->foodLevel == 20 &&
        p->foodStats->foodSaturationLevel == 5 &&
        p->foodStats->foodExhaustionLevel == 0);
  CHECK(p->foodStats->foodTimer == 0 && p->foodStats->prevFoodLevel == 20);
  CHECK(!p->capabilities->disableDamage && !p->capabilities->isFlying &&
        !p->capabilities->allowFlying && !p->capabilities->isCreativeMode &&
        p->capabilities->allowEdit);
  CHECK(p->capabilities->flySpeed == 0.05f &&
        p->capabilities->walkSpeed == 0.1f);
  CHECK(p->speedOnGround == 0.1f && p->speedInAir == 0.02f &&
        !p->hasReducedDebug);
  CHECK(!p->playerLocation && !p->spawnChunk &&
        !p->startMinecartRidingCoordinate && !p->itemInUse && !p->fishEntity);
  CHECK(!p->sleepTimer && !p->spawnForced && !p->itemInUseCount &&
        !p->lastXPSound && !p->experienceLevel && !p->experienceTotal);
  CHECK(p->prevCameraYaw == 0 && p->cameraYaw == 0 && p->xpCooldown == 0 &&
        p->prevChasingPosX == 0 && p->prevChasingPosY == 0 &&
        p->prevChasingPosZ == 0);
  CHECK(p->chasingPosX == 0 && p->chasingPosY == 0 && p->chasingPosZ == 0 &&
        p->renderOffsetX == 0 && p->renderOffsetY == 0 &&
        p->renderOffsetZ == 0);
  CHECK(p->sleeping == w->mutate && p->flyToggleTimer == (w->mutate ? 17 : 0) &&
        p->xpSeed == (w->mutate ? 19 : 0) &&
        p->experience == (w->mutate ? 0.5f : 0));
  CHECK(p->inventoryContainer == p->openContainer &&
        ContainerList_size(p->inventoryContainer->inventorySlots) == 45);
  ContainerPlayer *container = (ContainerPlayer *)p->inventoryContainer;
  CHECK(container->thePlayer == (MCObject *)p &&
        container->isLocalWorld == !w->remote);
  CHECK(container->craftMatrix->inventoryWidth == 2 &&
        container->craftMatrix->inventoryHeight == 2 &&
        container->craftMatrix->stackList->length == 4);
  CHECK(container->craftResult->stackResult->length == 1 &&
        !container->craftResult->stackResult->items[0]);
  SharedMonsterAttributes *s = SharedMonsterAttributes_get(e->object.heap);
  CHECK(s);
  CHECK(AttributeCollection_size(
            BaseAttributeMap_getAllAttributes(p->living.attributeMap)) == 4);
  CHECK(IAttributeInstance_getAttributeValue(
            BaseAttributeMap_getAttributeInstance(p->living.attributeMap,
                                                  s->maxHealth)) == 20);
  CHECK(IAttributeInstance_getAttributeValue(
            BaseAttributeMap_getAttributeInstance(
                p->living.attributeMap, s->knockbackResistance)) == 0);
  CHECK(IAttributeInstance_getAttributeValue(
            BaseAttributeMap_getAttributeInstance(p->living.attributeMap,
                                                  s->movementSpeed)) ==
        0.10000000149011612);
  CHECK(IAttributeInstance_getAttributeValue(
            BaseAttributeMap_getAttributeInstance(p->living.attributeMap,
                                                  s->attackDamage)) == 1);
  CHECK(!BaseAttributeMap_getAttributeInstance(p->living.attributeMap,
                                               s->followRange));
  CHECK(EntityLivingBase_getHealth(&p->living) == 20 &&
        WatchableObjectList_size(DataWatcher_getAllWatched(e->dataWatcher)) ==
            13);
  CHECK(DataWatcher_getWatchableObjectByte(e->dataWatcher, 16) == 0 &&
        DataWatcher_getWatchableObjectFloat(e->dataWatcher, 17) == 0);
  CHECK(DataWatcher_getWatchableObjectInt(e->dataWatcher, 18) == 0 &&
        DataWatcher_getWatchableObjectByte(e->dataWatcher, 10) == 0);
}
static void success_and_lifetime(void) {
  for (unsigned mode = 0; mode < 4; mode++) {
    MCObjectHeap *heap = MCObjectHeap_new(32 * 1024 * 1024);
    CHECK(heap);
    Witness *w = setup(heap);
    w->remote = (mode & 1) != 0;
    w->mutate = (mode & 2) != 0;
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, heap, (MCObject *)w));
    CHECK(construct(w, (MCObject *)w, w->profile));
    CHECK(w->count == sizeof sourceOrder / sizeof *sourceOrder &&
          memcmp(w->events, sourceOrder, sizeof sourceOrder) == 0);
    final_state(w);
    CHECK(MCObjectHeap_collect(heap));
    final_state(w);
    MCObjectHeap *copy = MCObjectHeap_clone(heap);
    CHECK(copy);
    MCObjectRoot otherRoot = {0};
    CHECK(MCObjectRoot_rebind(&otherRoot, copy, &root));
    Witness *other = (Witness *)MCObjectRoot_get(&otherRoot);
    CHECK(other && other != w && other->player != w->player);
    CHECK(other->player->inventory->player == (MCObject *)other->player &&
          other->player->living._combatTracker->fighter ==
              &other->player->living);
    CHECK(other->player->playerContext == (MCObject *)other &&
          other->player->living.entity.entityUniqueID == other->profile->id);
    CHECK(MCObjectHeap_collect(copy));
    final_state(other);
    CHECK(MCObjectHeap_adopt(heap, copy));
    w = (Witness *)MCObjectRoot_get(&root);
    CHECK(w);
    final_state(w);
    CHECK(MCObjectHeap_collect(heap));
    MCObjectRoot_drop(&otherRoot);
    MCObjectHeap_free(copy);
    release(w);
    MCObjectRoot_drop(&root);
    MCObjectHeap_free(heap);
  }
}
static void failure_prefixes(void) {
  unsigned baseline[128], count;
  MCObjectHeap *heap = MCObjectHeap_new(32 * 1024 * 1024);
  CHECK(heap);
  Witness *w = setup(heap);
  CHECK(construct(w, (MCObject *)w, w->profile));
  count = w->count;
  CHECK(count < 128);
  memcpy(baseline, w->events, count * sizeof *baseline);
  release(w);
  MCObjectHeap_free(heap);
  for (unsigned fail = 1; fail <= count; fail++) {
    heap = MCObjectHeap_new(32 * 1024 * 1024);
    CHECK(heap);
    w = setup(heap);
    w->failAt = fail;
    CHECK(!construct(w, (MCObject *)w, w->profile) &&
          MCObjectHeap_failed(heap) && w->count == fail);
    CHECK(memcmp(baseline, w->events, fail * sizeof *baseline) == 0);
    int32_t id;
    CHECK(NativeEntityIDRuntime_next(w->ids, &id) && id == 101);
    CHECK(w->player->living.unused180 == 0 &&
          w->player->living.entity.fireResistance == 1);
    if (baseline[fail - 1] >= REMOTE)
      CHECK(w->player->inventory && w->player->capabilities &&
            w->player->gameProfile == w->profile);
    release(w);
    MCObjectHeap_free(heap);
  }
  for (unsigned mode = 0; mode < 3; mode++) {
    heap = MCObjectHeap_new(32 * 1024 * 1024);
    CHECK(heap);
    w = setup(heap);
    w->nullSpawn = mode == 2;
    CHECK(!construct(w, mode == 1 ? NULL : (MCObject *)w,
                     mode == 0 ? NULL : w->profile) &&
          MCObjectHeap_failed(heap));
    CHECK(w->mathCount == 3 && w->player->inventory && w->player->foodStats &&
          w->player->capabilities);
    if (mode == 0)
      CHECK(!w->player->gameProfile && !w->player->inventoryContainer &&
            w->player->living.entity.entityUniqueID == w->generated);
    if (mode == 1)
      CHECK(w->player->gameProfile == w->profile &&
            !w->player->inventoryContainer);
    if (mode == 2)
      CHECK(w->player->inventoryContainer == w->player->openContainer &&
            w->player->inventoryContainer);
    release(w);
    MCObjectHeap_free(heap);
  }
}
static void spawn_overflow(void) {
  MCObjectHeap *heap = MCObjectHeap_new(32 * 1024 * 1024);
  CHECK(heap);
  Witness *w = setup(heap);
  w->spawn = DataWatcher_blockPos(heap, INT32_MIN, INT32_MAX, INT32_MAX);
  CHECK(w->spawn);
  CHECK(construct(w, (MCObject *)w, w->profile));
  CHECK(w->player->living.entity.posX == (double)INT32_MIN + 0.5 &&
        w->player->living.entity.posY == (double)INT32_MIN);
  CHECK(w->player->living.entity.posZ == (double)INT32_MAX + 0.5);
  release(w);
  MCObjectHeap_free(heap);
}
/* Break caught: the generated superclass UUID must be replaced only after
   all leaf initializers. Offline profiles use Java UTF-8 replacement for a
   lone UTF-16 surrogate, and keep the exact supplied profile/name objects.
   These immutable numeric facts were observed in the unchanged original
   EntityPlayer constructor, not recomputed by the implementation under test. */
static void offline_profile_constructors(void) {
  static const uint64_t most[] = {UINT64_C(0x29248963278b3c58),
                                  UINT64_C(0x7f091a332d523318),
                                  UINT64_C(0xa2ae45d26f783946)};
  static const uint64_t least[] = {UINT64_C(0xb053f6f1cd87dee2),
                                   UINT64_C(0xbc64918c7a63a34e),
                                   UINT64_C(0xa75a8f02034a96c8)};
  for (unsigned mode = 0; mode < 3; mode++) {
    MCObjectHeap *heap = MCObjectHeap_new(32 * 1024 * 1024);
    CHECK(heap);
    Witness *w = setup(heap);
    const uint16_t lone[] = {'A', 0xd800, 'B'};
    NBTString *name =
        mode == 2
            ? NBTString_fromUTF16(heap, lone, 3)
            : NBTString_fromUTF8(heap, mode == 1 ? "本の作者" : "PlayerTest");
    CHECK(name);
    w->profile = NativeGameProfile_new(heap, NULL, name);
    CHECK(w->profile);
    CHECK(construct(w, (MCObject *)w, w->profile));
    NativeJavaUUID *id = w->player->living.entity.entityUniqueID;
    CHECK(id && id != w->generated && w->profile->id == NULL &&
          w->mathCount == 3);
    CHECK((uint64_t)id->mostSignificantBits == most[mode] &&
          (uint64_t)id->leastSignificantBits == least[mode]);
    CHECK(w->player->gameProfile == w->profile &&
          EntityPlayer_getName(w->player) == name);
    CHECK(w->player->inventoryContainer == w->player->openContainer);
    release(w);
    MCObjectHeap_free(heap);
  }
}
int main(void) {
  success_and_lifetime();
  failure_prefixes();
  spawn_overflow();
  offline_profile_constructors();
  printf("source Player constructor: %u checks GREEN\n", checks);
  return 0;
}
