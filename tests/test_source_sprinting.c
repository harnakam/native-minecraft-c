#include "entity/EntityLivingBase.h"
#include "entity/SharedMonsterAttributes.h"
#include "entity/ai/attributes/ModifiableAttributeInstance.h"
#include "util/MCGameplayPlayer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      fprintf(stderr, "sprinting check %u line %d: %s\n", checks, __LINE__,    \
              #x);                                                             \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
enum { NOTIFY = 1, ATTRIBUTE, LOOKUP, REMOVE, APPLY };
/* This recording fixture operates on real canonical Player, DataWatcher and
   ModifiableAttributeInstance objects. It binds only the source virtual calls
   exercised by the setter; it is not a replacement production actor factory. */
typedef struct {
  MCObject object;
  MCGameplayPlayer *player;
  IAttributeInstance *speed, *replacement, *returned;
  AttributeModifier *removed, *applied;
  NativeJavaUUID *lookup;
  unsigned events[32], count, failAt;
  int32_t flagsAtAttribute, flagsAtNotify;
  bool nullAttribute, swapAttribute, clearLookup, collectDuringAttribute;
  bool removeCompleted, applyCompleted;
  const MCObjectClass *expectedStatics;
  unsigned constructorInit, constructorPosition;
} Witness;
static void trace(MCObject *o, MCObjectVisitor visit, void *context) {
  Witness *w = (Witness *)o;
  w->player = (MCGameplayPlayer *)visit((MCObject *)w->player, context);
  w->speed = (IAttributeInstance *)visit((MCObject *)w->speed, context);
  w->replacement =
      (IAttributeInstance *)visit((MCObject *)w->replacement, context);
  w->returned = (IAttributeInstance *)visit((MCObject *)w->returned, context);
  w->removed = (AttributeModifier *)visit((MCObject *)w->removed, context);
  w->applied = (AttributeModifier *)visit((MCObject *)w->applied, context);
  w->lookup = (NativeJavaUUID *)visit((MCObject *)w->lookup, context);
}
static const MCObjectClass witnessClass = {
    "fixture.SourceSprinting", MCObjectHeap_plainClone, trace, NULL};
static bool any(const MCObject *o, void *context) {
  (void)context;
  return MCObjectHeap_objectSize(o) >= sizeof(Witness);
}
static Witness *witness(IAttributeInstance *i) {
  Witness *w = (Witness *)MCObjectHeap_findObject(i->object.heap, &witnessClass,
                                                  any, NULL);
  CHECK(w);
  return w;
}
static bool event(Witness *w, unsigned kind) {
  CHECK(w->count < 32);
  w->events[w->count++] = kind;
  if (w->failAt && w->count == w->failAt) {
    MCObjectHeap_fail(w->object.heap);
    return false;
  }
  return true;
}
static bool notify(MCObject *context, MCObject *owner, int32_t id) {
  Witness *w = (Witness *)context;
  CHECK(owner == (MCObject *)w->player && id == 0);
  w->flagsAtNotify = DataWatcher_getWatchableObjectByte(
      w->player->living.entity.dataWatcher, 0);
  return event(w, NOTIFY);
}
static const DataWatcherDependencies watcherMethods = {.onDataWatcherUpdate =
                                                           notify};
static IAttributeInstance *
attribute(MCObject *context, EntityLivingBase *living, IAttribute *requested) {
  Witness *w = (Witness *)context;
  CHECK(living == &w->player->living);
  CHECK(requested == SharedMonsterAttributes_get(context->heap)->movementSpeed);
  w->flagsAtAttribute =
      DataWatcher_getWatchableObjectByte(living->entity.dataWatcher, 0);
  if (!event(w, ATTRIBUTE))
    return NULL;
  if (w->collectDuringAttribute)
    CHECK(!MCObjectHeap_collect(context->heap));
  if (w->nullAttribute)
    return NULL;
  w->returned = w->speed;
  if (w->swapAttribute)
    w->speed = w->replacement;
  MCObjectHeap_touch(context->heap);
  return w->returned;
}
static const EntityLivingBaseDependencies livingMethods = {.getEntityAttribute =
                                                               attribute};
static AttributeModifier *lookup(IAttributeInstance *i, NativeJavaUUID *id) {
  Witness *w = witness(i);
  CHECK(i == w->returned);
  w->lookup = id;
  if (!event(w, LOOKUP))
    return NULL;
  if (w->clearLookup)
    return NULL;
  return ModifiableAttributeInstance_getModifier(
      (ModifiableAttributeInstance *)i, id);
}
static bool remove_modifier(IAttributeInstance *i, AttributeModifier *m) {
  Witness *w = witness(i);
  CHECK(i == w->returned);
  w->removed = m;
  w->removeCompleted = event(w, REMOVE) &&
                       ModifiableAttributeInstance_removeModifier(
                           (ModifiableAttributeInstance *)i, m);
  return w->removeCompleted;
}
static bool apply_modifier(IAttributeInstance *i, AttributeModifier *m) {
  Witness *w = witness(i);
  CHECK(i == w->returned);
  w->applied = m;
  w->applyCompleted = event(w, APPLY) &&
                      ModifiableAttributeInstance_applyModifier(
                          (ModifiableAttributeInstance *)i, m);
  return w->applyCompleted;
}
#define DELEGATE(ret, name, args, call)                                        \
  static ret delegate_##name args {                                            \
    return ModifiableAttributeInstance_##name call;                            \
  }
DELEGATE(IAttribute *, getAttribute, (IAttributeInstance * i),
         ((ModifiableAttributeInstance *)i))
DELEGATE(double, getBaseValue, (IAttributeInstance * i),
         ((ModifiableAttributeInstance *)i))
DELEGATE(bool, setBaseValue, (IAttributeInstance * i, double value),
         ((ModifiableAttributeInstance *)i, value))
DELEGATE(AttributeCollection *, getModifiersByOperation,
         (IAttributeInstance * i, int32_t op),
         ((ModifiableAttributeInstance *)i, op))
DELEGATE(AttributeCollection *, func_111122_c, (IAttributeInstance * i),
         ((ModifiableAttributeInstance *)i))
DELEGATE(bool, hasModifier, (IAttributeInstance * i, AttributeModifier *m),
         ((ModifiableAttributeInstance *)i, m))
DELEGATE(bool, removeAllModifiers, (IAttributeInstance * i),
         ((ModifiableAttributeInstance *)i))
DELEGATE(double, getAttributeValue, (IAttributeInstance * i),
         ((ModifiableAttributeInstance *)i))
#undef DELEGATE
static const IAttributeInstanceMethods recordingMethods = {
    .getAttribute = delegate_getAttribute,
    .getBaseValue = delegate_getBaseValue,
    .setBaseValue = delegate_setBaseValue,
    .getModifiersByOperation = delegate_getModifiersByOperation,
    .func_111122_c = delegate_func_111122_c,
    .hasModifier = delegate_hasModifier,
    .getModifier = lookup,
    .removeModifier = remove_modifier,
    .applyModifier = apply_modifier,
    .removeAllModifiers = delegate_removeAllModifiers,
    .getAttributeValue = delegate_getAttributeValue};
static const IAttributeInstanceMethods missingLookup = {
    .removeModifier = remove_modifier, .applyModifier = apply_modifier};
static const IAttributeInstanceMethods missingRemove = {
    .getModifier = lookup, .applyModifier = apply_modifier};
static const IAttributeInstanceMethods missingApply = {
    .getModifier = lookup, .removeModifier = remove_modifier};
static Witness *setup(MCObjectHeap *heap) {
  Witness *w = (Witness *)MCObjectHeap_alloc(heap, sizeof *w, &witnessClass);
  CHECK(w);
  w->player = MCGameplayPlayer_nativeAllocate(heap);
  CHECK(w->player);
  EntityLivingBase *l = &w->player->living;
  l->livingDependencies = &livingMethods;
  l->livingContext = (MCObject *)w;
  l->entity.dataWatcher = DataWatcher_new(heap, (MCObject *)w->player,
                                          &watcherMethods, (MCObject *)w);
  CHECK(l->entity.dataWatcher);
  CHECK(DataWatcher_addObject(l->entity.dataWatcher, 0,
                              DataWatcher_boxByte(heap, 0)));
  BaseAttributeMap *map = EntityLivingBase_getAttributeMap(l);
  CHECK(map);
  SharedMonsterAttributes *s = SharedMonsterAttributes_get(heap);
  CHECK(s);
  w->speed = BaseAttributeMap_registerAttribute(map, s->movementSpeed);
  CHECK(w->speed);
  CHECK(IAttributeInstance_setBaseValue(w->speed, 0.10000000149011612));
  w->speed->methods = &recordingMethods;
  w->replacement = (IAttributeInstance *)ModifiableAttributeInstance_new(
      heap, map, s->movementSpeed);
  CHECK(w->replacement);
  w->replacement->methods = &recordingMethods;
  return w;
}
static void reset(Witness *w) {
  w->count = 0;
  w->failAt = 0;
  w->removed = NULL;
  w->applied = NULL;
  w->lookup = NULL;
  w->returned = NULL;
  w->flagsAtAttribute = 0;
  w->flagsAtNotify = 0;
  w->removeCompleted = false;
  w->applyCompleted = false;
}
static void order(Witness *w, const unsigned *events, unsigned count) {
  CHECK(w->count == count);
  for (unsigned i = 0; i < count; i++)
    CHECK(w->events[i] == events[i]);
}
static uint64_t bits(double d) {
  uint64_t value;
  memcpy(&value, &d, sizeof value);
  return value;
}
static void statics_and_lifetime(void) {
  MCObjectHeap *heap = MCObjectHeap_new(16 * 1024 * 1024);
  CHECK(heap);
  const EntityLivingBaseStaticFields *s =
      EntityLivingBase_getStaticFields(heap);
  CHECK(s);
  CHECK(EntityLivingBase_getStaticFields(heap) == s);
  CHECK(s->sprintingSpeedBoostModifierUUID->mostSignificantBits ==
        INT64_C(0x662a6b8dda3e4c1c));
  CHECK((uint64_t)s->sprintingSpeedBoostModifierUUID->leastSignificantBits ==
        UINT64_C(0x881396ea6097278d));
  AttributeModifier *m = s->sprintingSpeedBoostModifier;
  CHECK(AttributeModifier_getID(m) == s->sprintingSpeedBoostModifierUUID);
  CHECK(
      NBTString_equals(AttributeModifier_getName(m),
                       NBTString_literalASCII(heap, "Sprinting speed boost")));
  CHECK(bits(AttributeModifier_getAmount(m)) == UINT64_C(0x3fd3333340000000));
  CHECK(AttributeModifier_getOperation(m) == 2 &&
        !AttributeModifier_isSaved(m));
  Witness *w = setup(heap);
  MCObjectRoot root = {0};
  CHECK(MCObjectRoot_init(&root, heap, (MCObject *)w));
  CHECK(EntityLivingBase_setSprinting(&w->player->living, true));
  CHECK(ModifiableAttributeInstance_getModifier(
            (ModifiableAttributeInstance *)w->speed,
            s->sprintingSpeedBoostModifierUUID) == m);
  CHECK(MCObjectHeap_collect(heap) &&
        EntityLivingBase_getStaticFields(heap) == s);
  MCObjectHeap *copy = MCObjectHeap_clone(heap);
  CHECK(copy);
  MCObjectRoot copied = {0};
  CHECK(MCObjectRoot_rebind(&copied, copy, &root));
  Witness *other = (Witness *)MCObjectRoot_get(&copied);
  CHECK(other && other != w);
  const EntityLivingBaseStaticFields *t =
      EntityLivingBase_getStaticFields(copy);
  CHECK(t && t != s);
  CHECK(t->sprintingSpeedBoostModifier != m &&
        t->sprintingSpeedBoostModifierUUID !=
            s->sprintingSpeedBoostModifierUUID);
  CHECK(AttributeModifier_getID(t->sprintingSpeedBoostModifier) ==
        t->sprintingSpeedBoostModifierUUID);
  CHECK(ModifiableAttributeInstance_getModifier(
            (ModifiableAttributeInstance *)other->speed,
            t->sprintingSpeedBoostModifierUUID) ==
        t->sprintingSpeedBoostModifier);
  reset(other);
  CHECK(EntityLivingBase_setSprinting(&other->player->living, false));
  CHECK(Entity_isSprinting(&w->player->living.entity) &&
        !Entity_isSprinting(&other->player->living.entity));
  CHECK(MCObjectHeap_collect(copy) &&
        EntityLivingBase_getStaticFields(copy) == t);
  CHECK(MCObjectHeap_adopt(heap, copy));
  CHECK(MCObjectRoot_rebind(&copied, heap, &copied));
  MCObjectHeap_free(copy);
  w = (Witness *)MCObjectRoot_get(&root);
  CHECK(w);
  s = EntityLivingBase_getStaticFields(heap);
  CHECK(s && s == t);
  CHECK(!Entity_isSprinting(&w->player->living.entity));
  CHECK(AttributeModifier_getID(s->sprintingSpeedBoostModifier) ==
        s->sprintingSpeedBoostModifierUUID);
  CHECK(AttributeModifier_setSaved(s->sprintingSpeedBoostModifier, true) ==
        s->sprintingSpeedBoostModifier);
  CHECK(EntityLivingBase_getStaticFields(heap)->sprintingSpeedBoostModifier ==
        s->sprintingSpeedBoostModifier);
  CHECK(AttributeModifier_isSaved(
      EntityLivingBase_getStaticFields(heap)->sprintingSpeedBoostModifier));
  reset(w);
  CHECK(EntityLivingBase_setSprinting(&w->player->living, true));
  CHECK(w->applied == s->sprintingSpeedBoostModifier &&
        AttributeModifier_isSaved(w->applied));
  MCObjectRoot_drop(&copied);
  MCObjectRoot_drop(&root);
  CHECK(MCObjectHeap_collect(heap));
  CHECK(EntityLivingBase_getStaticFields(heap) == s);
  MCObjectHeap_free(heap);
  MCObjectHeap *second = MCObjectHeap_new(1024 * 1024);
  CHECK(second);
  CHECK(EntityLivingBase_getStaticFields(second));
  MCObjectHeap_free(second);
}
static void source_order_and_repeated(void) {
  MCObjectHeap *heap = MCObjectHeap_new(16 * 1024 * 1024);
  CHECK(heap);
  Witness *w = setup(heap);
  const EntityLivingBaseStaticFields *s =
      EntityLivingBase_getStaticFields(heap);
  CHECK(s);
  CHECK(Entity_setSneaking(&w->player->living.entity, true));
  reset(w);
  CHECK(EntityLivingBase_setSprinting(&w->player->living, true));
  const unsigned first[] = {NOTIFY, ATTRIBUTE, LOOKUP, APPLY};
  order(w, first, 4);
  CHECK(w->flagsAtNotify == 10 && w->flagsAtAttribute == 10 &&
        w->lookup == s->sprintingSpeedBoostModifierUUID);
  CHECK(w->applied == s->sprintingSpeedBoostModifier &&
        Entity_isSneaking(&w->player->living.entity));
  CHECK(bits(ModifiableAttributeInstance_getAttributeValue(
            (ModifiableAttributeInstance *)w->speed)) ==
        UINT64_C(0x3fc0a3d710f5c290));
  reset(w);
  CHECK(EntityLivingBase_setSprinting(&w->player->living, true));
  const unsigned repeat[] = {ATTRIBUTE, LOOKUP, REMOVE, APPLY};
  order(w, repeat, 4);
  CHECK(w->removed == s->sprintingSpeedBoostModifier &&
        w->applied == w->removed);
  CHECK(((ModifiableAttributeInstance *)w->speed)->needsUpdate);
  reset(w);
  CHECK(EntityLivingBase_setSprinting(&w->player->living, false));
  const unsigned stop[] = {NOTIFY, ATTRIBUTE, LOOKUP, REMOVE};
  order(w, stop, 4);
  CHECK(w->flagsAtNotify == 2 && w->flagsAtAttribute == 2 && !w->applied);
  CHECK(!Entity_isSprinting(&w->player->living.entity));
  CHECK(ModifiableAttributeInstance_getAttributeValue(
            (ModifiableAttributeInstance *)w->speed) == 0.10000000149011612);
  reset(w);
  CHECK(EntityLivingBase_setSprinting(&w->player->living, false));
  const unsigned absent[] = {ATTRIBUTE, LOOKUP};
  order(w, absent, 2);
  CHECK(!w->removed && !w->applied);
  CHECK(Entity_setFlag(&w->player->living.entity, 7, true));
  reset(w);
  CHECK(EntityLivingBase_setSprinting(&w->player->living, true));
  CHECK(DataWatcher_getWatchableObjectByte(w->player->living.entity.dataWatcher,
                                           0) == -118);
  MCObjectHeap_free(heap);
}
static void different_lookup_and_captured_receiver(void) {
  MCObjectHeap *heap = MCObjectHeap_new(16 * 1024 * 1024);
  CHECK(heap);
  Witness *w = setup(heap);
  const EntityLivingBaseStaticFields *s =
      EntityLivingBase_getStaticFields(heap);
  CHECK(s);
  NativeJavaUUID *equal = NativeJavaUUID_new(
      heap, s->sprintingSpeedBoostModifierUUID->mostSignificantBits,
      s->sprintingSpeedBoostModifierUUID->leastSignificantBits);
  CHECK(equal);
  AttributeModifier *foreign = AttributeModifier_new(
      heap, equal, NBTString_literalASCII(heap, "Different name"), 7, 0);
  CHECK(foreign);
  CHECK(ModifiableAttributeInstance_applyModifier(
      (ModifiableAttributeInstance *)w->speed, foreign));
  CHECK(ModifiableAttributeInstance_getAttributeValue(
            (ModifiableAttributeInstance *)w->speed) > 7);
  w->swapAttribute = true;
  IAttributeInstance *original = w->speed;
  CHECK(EntityLivingBase_setSprinting(&w->player->living, true));
  CHECK(w->returned == original && w->speed == w->replacement);
  CHECK(w->removed == s->sprintingSpeedBoostModifier && w->removed != foreign);
  CHECK(ModifiableAttributeInstance_getModifier(
            (ModifiableAttributeInstance *)original, equal) ==
        s->sprintingSpeedBoostModifier);
  CHECK(ModifiableAttributeInstance_getModifier(
            (ModifiableAttributeInstance *)w->replacement, equal) == NULL);
  /* Original removal uses the static name: UUID/op membership is removed,
     but a different-name set retains the equal-UUID original reference. */
  AttributeCollection *byName = (AttributeCollection *)AttributeNativeMap_get(
      ((ModifiableAttributeInstance *)original)->mapByName,
      (MCObject *)foreign->name);
  CHECK(byName && AttributeCollection_size(byName) == 1 &&
        AttributeCollection_getAt(byName, 0) == (MCObject *)foreign);
  CHECK(AttributeCollection_size(
            ((ModifiableAttributeInstance *)original)->mapByOperation[0]) == 0);
  CHECK(AttributeCollection_size(
            ((ModifiableAttributeInstance *)original)->mapByOperation[2]) == 1);
  MCObjectHeap_free(heap);
}
static bool ctor_position(MCObject *context, Entity *e, double x, double y,
                          double z) {
  Witness *w = (Witness *)context;
  ++w->constructorPosition;
  return Entity_setPosition(e, x, y, z);
}
static bool ctor_box(MCObject *context, Entity *e, AxisAlignedBB *box) {
  (void)context;
  return Entity_setEntityBoundingBox(e, box);
}
static bool static_fields_size(const MCObject *o, void *context) {
  (void)context;
  return MCObjectHeap_objectSize(o) >= sizeof(EntityLivingBaseStaticFields);
}
static bool ctor_init(MCObject *context, Entity *e) {
  Witness *w = (Witness *)context;
  ++w->constructorInit;
  const EntityLivingBaseStaticFields *s =
      (const EntityLivingBaseStaticFields *)MCObjectHeap_findObject(
          context->heap, w->expectedStatics, static_fields_size, NULL);
  CHECK(s && s->sprintingSpeedBoostModifier &&
        s->sprintingSpeedBoostModifierUUID);
  CHECK(AttributeModifier_getID(s->sprintingSpeedBoostModifier) ==
        s->sprintingSpeedBoostModifierUUID);
  CHECK(!AttributeModifier_isSaved(s->sprintingSpeedBoostModifier));
  return EntityLivingBase_entityInit((EntityLivingBase *)e);
}
static bool ctor_attributes(MCObject *context, EntityLivingBase *e) {
  (void)context;
  return EntityLivingBase_applyEntityAttributes(e);
}
static BaseAttributeMap *ctor_map(MCObject *context, EntityLivingBase *e) {
  (void)context;
  return EntityLivingBase_getAttributeMap(e);
}
static IAttributeInstance *ctor_attribute(MCObject *context,
                                          EntityLivingBase *e, IAttribute *a) {
  (void)context;
  return EntityLivingBase_getEntityAttribute(e, a);
}
static bool ctor_health(MCObject *context, EntityLivingBase *e, float f) {
  (void)context;
  return EntityLivingBase_setHealth(e, f);
}
static bool ctor_math(MCObject *context, double *out) {
  (void)context;
  *out = 0.5;
  return true;
}
static bool ctor_nano(void *context, int64_t *out) {
  (void)context;
  *out = 123;
  return true;
}
static const NativeJavaRandomRuntimeDependencies ctor_clocks = {ctor_nano};
static const EntityDependencies ctor_entity = {.entityInit = ctor_init,
                                               .setPosition = ctor_position,
                                               .setEntityBoundingBox =
                                                   ctor_box};
static const EntityLivingBaseDependencies ctor_living = {
    .applyEntityAttributes = ctor_attributes,
    .getAttributeMap = ctor_map,
    .getEntityAttribute = ctor_attribute,
    .setHealth = ctor_health,
    .mathRandom = ctor_math};
static void constructor_class_initialization(void) {
  /* Class-static storage initialization is native and per-heap. Its fields
     are already initialized at Entity's first virtual entityInit call. This
     fixture translates Living on an allocated concrete Player receiver;
     it does not claim that the Player leaf constructor ran. */
  MCObjectHeap *prior = MCObjectHeap_new(1024 * 1024);
  CHECK(prior);
  const EntityLivingBaseStaticFields *existing =
      EntityLivingBase_getStaticFields(prior);
  CHECK(existing);
  const MCObjectClass *expected = existing->object.klass;
  MCObjectHeap_free(prior);
  for (unsigned fail = 0; fail < 2; fail++) {
    MCObjectHeap *heap = MCObjectHeap_new(16 * 1024 * 1024);
    CHECK(heap);
    Witness *w = (Witness *)MCObjectHeap_alloc(heap, sizeof *w, &witnessClass);
    CHECK(w);
    w->expectedStatics = expected;
    w->player = MCGameplayPlayer_nativeAllocate(heap);
    CHECK(w->player);
    CHECK(!MCObjectHeap_findObject(heap, expected, static_fields_size, NULL));
    NativeJavaRandomRuntime *random =
        NativeJavaRandomRuntime_new(&ctor_clocks, NULL);
    CHECK(random);
    NativeEntityIDRuntime *ids = NativeEntityIDRuntime_new(123);
    CHECK(ids);
    MCObjectClass exhausted = {"fixture.exhaust.constructor",
                               MCObjectHeap_plainClone, NULL, NULL};
    if (fail) {
      size_t remaining = 16 * 1024 * 1024 - MCObjectHeap_liveBytes(heap);
      CHECK(MCObjectHeap_alloc(heap, remaining, &exhausted));
    }
    CHECK(EntityLivingBase_construct(&w->player->living, NULL, &ctor_entity,
                                     (MCObject *)w, &ctor_living, (MCObject *)w,
                                     random, ids) == !fail);
    if (fail) {
      CHECK(MCObjectHeap_failed(heap) && w->constructorInit == 0 &&
            w->constructorPosition == 0);
      CHECK(!w->player->living.entity.rand &&
            !w->player->living.entity.dataWatcher);
    } else {
      CHECK(w->constructorInit == 1 && w->constructorPosition == 2);
      const EntityLivingBaseStaticFields *s =
          EntityLivingBase_getStaticFields(heap);
      CHECK(s && s->object.klass == expected);
      CHECK(w->player->living.entity.entityId == 123 &&
            EntityLivingBase_getHealth(&w->player->living) == 20);
      IAttributeInstance *speed = EntityLivingBase_getEntityAttribute(
          &w->player->living, SharedMonsterAttributes_get(heap)->movementSpeed);
      CHECK(speed);
      CHECK(EntityLivingBase_setSprinting(&w->player->living, true));
      CHECK(IAttributeInstance_getModifier(
                speed, s->sprintingSpeedBoostModifierUUID) ==
            s->sprintingSpeedBoostModifier);
    }
    CHECK(NativeJavaRandomRuntime_free(random) &&
          NativeEntityIDRuntime_free(ids));
    MCObjectHeap_free(heap);
  }
}
static void malformed_native_boundaries(void) {
  for (unsigned mode = 0; mode < 3; mode++) {
    MCObjectHeap *heap = MCObjectHeap_new(16 * 1024 * 1024);
    CHECK(heap);
    Witness *w = setup(heap);
    CHECK(EntityLivingBase_getStaticFields(heap));
    MCObjectHeap *foreign = MCObjectHeap_new(1024 * 1024);
    CHECK(foreign);
    if (mode == 0)
      w->player->living.entity.dataWatcher = NULL;
    if (mode == 1) {
      CHECK(WatchableObject_setObject(
          DataWatcher_nativeGetWatchedObject(
              w->player->living.entity.dataWatcher, 0),
          DataWatcher_boxInt(heap, 7)));
      reset(w);
    }
    if (mode == 2) {
      SharedMonsterAttributes *other = SharedMonsterAttributes_get(foreign);
      CHECK(other);
      w->speed = (IAttributeInstance *)ModifiableAttributeInstance_new(
          foreign, NULL, other->movementSpeed);
      CHECK(w->speed);
    }
    CHECK(!EntityLivingBase_setSprinting(&w->player->living, true) &&
          MCObjectHeap_failed(heap));
    CHECK(w->count == (mode == 2 ? 2 : 0));
    MCObjectHeap_free(heap);
    MCObjectHeap_free(foreign);
  }
  MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024);
  CHECK(heap);
  const EntityLivingBaseStaticFields *s =
      EntityLivingBase_getStaticFields(heap);
  CHECK(s);
  MCObjectHeap *other = MCObjectHeap_new(1024 * 1024);
  CHECK(other);
  CHECK(MCObjectHeap_alloc(other, sizeof(MCObject), s->object.klass));
  const EntityLivingBaseStaticFields *proper =
      EntityLivingBase_getStaticFields(other);
  CHECK(proper);
  CHECK(MCObjectHeap_objectSize((MCObject *)proper) >= sizeof *proper &&
        proper->sprintingSpeedBoostModifier);
  MCObjectHeap_free(other);
  MCObjectHeap_free(heap);
  heap = MCObjectHeap_new(1024 * 1024);
  CHECK(heap);
  MCGameplayPlayer *native = MCGameplayPlayer_nativeAllocate(heap);
  CHECK(native);
  EntityLivingBase *undersized = (EntityLivingBase *)MCObjectHeap_alloc(
      heap, sizeof(MCObject), native->living.entity.object.klass);
  CHECK(undersized);
  CHECK(!EntityLivingBase_setSprinting(undersized, true) &&
        MCObjectHeap_failed(heap));
  MCObjectHeap_free(heap);
}
static void failure_prefixes(void) {
  for (unsigned point = 1; point <= 5; point++) {
    MCObjectHeap *heap = MCObjectHeap_new(16 * 1024 * 1024);
    CHECK(heap);
    Witness *w = setup(heap);
    const EntityLivingBaseStaticFields *s =
        EntityLivingBase_getStaticFields(heap);
    CHECK(s);
    CHECK(ModifiableAttributeInstance_applyModifier(
        (ModifiableAttributeInstance *)w->speed,
        s->sprintingSpeedBoostModifier));
    w->failAt = point;
    CHECK(!EntityLivingBase_setSprinting(&w->player->living, true) &&
          MCObjectHeap_failed(heap));
    CHECK(w->count == point);
    /* Source changed flag is stored before the watcher callback fails. */
    WatchableObject *entry = DataWatcher_nativeGetWatchedObject(
        w->player->living.entity.dataWatcher, 0);
    CHECK(entry);
    MCObject *boxed = WatchableObject_getObject(entry);
    CHECK(boxed);
    /* Source getters fail once the native heap is fatal; the recorded
       observer sees the actual pre-exception flag instead. */
    CHECK(w->flagsAtNotify == 8);
    if (point >= 2)
      CHECK(w->flagsAtAttribute == 8);
    if (point >= 4)
      CHECK(w->removed == s->sprintingSpeedBoostModifier);
    if (point >= 5)
      CHECK(w->applied == s->sprintingSpeedBoostModifier);
    CHECK(w->removeCompleted == (point == 5));
    CHECK(!w->applyCompleted);
    MCObjectHeap_free(heap);
  }
  for (unsigned mode = 0; mode < 5; mode++) {
    MCObjectHeap *heap = MCObjectHeap_new(16 * 1024 * 1024);
    CHECK(heap);
    Witness *w = setup(heap);
    const EntityLivingBaseStaticFields *s =
        EntityLivingBase_getStaticFields(heap);
    CHECK(s);
    if (mode == 0)
      w->nullAttribute = true;
    if (mode == 1)
      w->player->living.livingDependencies = NULL;
    if (mode == 2)
      w->speed->methods = &missingLookup;
    if (mode == 3) {
      CHECK(ModifiableAttributeInstance_applyModifier(
          (ModifiableAttributeInstance *)w->speed,
          s->sprintingSpeedBoostModifier));
      w->speed->methods = &missingRemove;
    }
    if (mode == 4)
      w->speed->methods = &missingApply;
    CHECK(!EntityLivingBase_setSprinting(&w->player->living, true) &&
          MCObjectHeap_failed(heap));
    CHECK(w->flagsAtNotify == 8);
    MCObjectHeap_free(heap);
  }
  MCObjectHeap *heap = MCObjectHeap_new(16 * 1024 * 1024);
  CHECK(heap);
  Witness *w = setup(heap);
  CHECK(EntityLivingBase_getStaticFields(heap));
  w->collectDuringAttribute = true;
  CHECK(EntityLivingBase_setSprinting(&w->player->living, true) &&
        !MCObjectHeap_failed(heap));
  CHECK(w->flagsAtNotify == 8 && w->count == 4);
  MCObjectHeap_free(heap);
  CHECK(!EntityLivingBase_getStaticFields(NULL));
  CHECK(!EntityLivingBase_setSprinting(NULL, true) &&
        !Entity_isSprinting(NULL));
  heap = MCObjectHeap_new(1);
  CHECK(heap);
  CHECK(!EntityLivingBase_getStaticFields(heap) && MCObjectHeap_failed(heap));
  MCObjectHeap_free(heap);
  heap = MCObjectHeap_new(16 * 1024 * 1024);
  CHECK(heap);
  w = setup(heap);
  CHECK(EntityLivingBase_getStaticFields(heap));
  size_t remaining = 16 * 1024 * 1024 - MCObjectHeap_liveBytes(heap);
  MCObjectClass exhaustedClass = {"fixture.exhaustion", MCObjectHeap_plainClone,
                                  NULL, NULL};
  CHECK(remaining > sizeof(MCObject));
  CHECK(MCObjectHeap_alloc(heap, remaining, &exhaustedClass));
  CHECK(!EntityLivingBase_setSprinting(&w->player->living, true) &&
        MCObjectHeap_failed(heap));
  CHECK(w->count == 0);
  MCObjectHeap_free(heap);
}
int main(void) {
  statics_and_lifetime();
  source_order_and_repeated();
  different_lookup_and_captured_receiver();
  constructor_class_initialization();
  malformed_native_boundaries();
  failure_prefixes();
  printf("Source sprinting: %u checks passed\n", checks);
  return 0;
}
