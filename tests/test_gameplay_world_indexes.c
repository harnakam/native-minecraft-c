#include "entity/item/EntityItem.h"
#include "util/MCGameplayPlayer.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      fprintf(stderr, "world index check %u line %d: %s\n", checks, __LINE__,  \
              #x);                                                             \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
/* Index admission does not invoke gameplay effects. These required fixture
   bindings fail the test if an unrelated Source effect is unexpectedly run. */
static bool log_item(MCObject *c, int32_t id) {
  (void)c;
  (void)id;
  CHECK(0);
  return false;
}
static bool remote(MCObject *c, MCObject *w) {
  (void)c;
  (void)w;
  CHECK(0);
  return false;
}
static InventoryPlayer *inventory(MCObject *c, MCObject *p) {
  (void)c;
  (void)p;
  CHECK(0);
  return NULL;
}
static const NBTString *name(MCObject *c, MCObject *p) {
  (void)c;
  (void)p;
  CHECK(0);
  return NULL;
}
static MCObject *find(MCObject *c, MCObject *w, const NBTString *n) {
  (void)c;
  (void)w;
  (void)n;
  CHECK(0);
  return NULL;
}
static bool achievement(MCObject *c, MCObject *p, EntityItemAchievement a) {
  (void)c;
  (void)p;
  (void)a;
  CHECK(0);
  return false;
}
static bool silent(MCObject *c, const EntityItem *e) {
  (void)c;
  (void)e;
  CHECK(0);
  return false;
}
static float random_float(MCObject *c, EntityItem *e) {
  (void)c;
  (void)e;
  CHECK(0);
  return 0;
}
static bool sound(MCObject *c, MCObject *w, MCObject *p, const char *n, float v,
                  float x) {
  (void)c;
  (void)w;
  (void)p;
  (void)n;
  (void)v;
  (void)x;
  CHECK(0);
  return false;
}
static bool pickup(MCObject *c, MCObject *p, EntityItem *e, int32_t n) {
  (void)c;
  (void)p;
  (void)e;
  (void)n;
  CHECK(0);
  return false;
}
static bool dead(MCObject *c, EntityItem *e) {
  (void)c;
  (void)e;
  CHECK(0);
  return false;
}
static const EntityItemDependencies deps = {
    log_item, remote,       inventory, name,   find, achievement,
    silent,   random_float, sound,     pickup, dead};
static MCGameplayWorld *setup(MCGameplay *g) {
  CHECK(MCGameplay_init(g, 32u << 20));
  MCGameplayWorld *w =
      MCGameplayWorld_new(g->heap, MCGameplay_get(g), NULL, NULL);
  CHECK(w);
  CHECK(MCGameplay_setWorld(g, (MCObject *)w));
  return w;
}
static EntityItem *item(MCGameplay *g, MCGameplayWorld *w, int32_t id) {
  EntityItem *e = EntityItem_nativeNew(g->heap, (MCObject *)w, NULL, &deps);
  CHECK(e);
  Entity_setEntityId(&e->entity, id);
  return e;
}
static MCGameplayPlayer *player(MCGameplay *g, MCGameplayWorld *w, int32_t id) {
  /* Native allocation-only fixture; no Source constructor is claimed. */
  MCGameplayPlayer *p = MCGameplayPlayer_nativeAllocate(g->heap);
  CHECK(p);
  p->living.entity.worldObj = (MCObject *)w;
  Entity_setEntityId(&p->living.entity, id);
  return p;
}
static void initial_admission(void) {
  MCGameplay g = {0};
  CHECK(MCGameplay_init(&g, 32u << 20));
  MCGameplayObjects *o = MCGameplay_get(&g);
  MCGameplayWorld *w = MCGameplayWorld_new(g.heap, o, NULL, NULL);
  CHECK(w);
  CHECK(MCGameplay_setWorld(&g, (MCObject *)w));
  EntityItem *e = item(&g, w, 17);
  CHECK(MCGameplay_addItem(&g, (MCObject *)e));
  CHECK(NativeReferenceList_size(w->loadedEntityList) == 1);
  CHECK(IntHashMap_lookup(w->entitiesById, 17) == (MCObject *)e);
  CHECK(MCGameplay_validateWorldIndexes(o));
  CHECK(MCGameplay_free(&g));
}
static void incremental_order_rekey_remove_and_world_switch(void) {
  MCGameplay g = {0};
  MCGameplayWorld *w = setup(&g);
  MCGameplayObjects *o = MCGameplay_get(&g);
  MCObjectRootScope scope = {0};
  CHECK(MCObjectRootScope_begin(&scope, g.heap));
  EntityItem *unrelated = item(&g, w, -1),
             *indexedOnly = item(&g, w, INT32_MIN);
  CHECK(NativeReferenceList_add(w->loadedEntityList, (MCObject *)unrelated));
  CHECK(IntHashMap_addKey(w->entitiesById, -1, (MCObject *)unrelated) &&
        IntHashMap_addKey(w->entitiesById, INT32_MIN, (MCObject *)indexedOnly));
  MCGameplayPlayer *p = player(&g, w, 71);
  CHECK(MCGameplay_setPlayer(&g, 0, "11111111-1111-1111-1111-111111111111",
                             (MCObject *)p));
  EntityItem *a = item(&g, w, 17), *b = item(&g, w, 0);
  CHECK(MCGameplay_addItem(&g, (MCObject *)a) &&
        MCGameplay_addItem(&g, (MCObject *)b));
  CHECK(NativeReferenceList_size(w->loadedEntityList) == 4 &&
        NativeReferenceList_size(w->playerEntities) == 1);
  CHECK(NativeReferenceList_get(w->loadedEntityList, 0) ==
            (MCObject *)unrelated &&
        NativeReferenceList_get(w->loadedEntityList, 1) == (MCObject *)p &&
        NativeReferenceList_get(w->loadedEntityList, 2) == (MCObject *)a &&
        NativeReferenceList_get(w->loadedEntityList, 3) == (MCObject *)b);
  uint32_t loadedMod = w->loadedEntityList->modCount,
           playersMod = w->playerEntities->modCount;
  IntHashMapEntry *aEntry = IntHashMap_lookupEntry(w->entitiesById, 17),
                  *pEntry = IntHashMap_lookupEntry(w->entitiesById, 71);
  size_t live = MCObjectHeap_liveObjects(g.heap),
         bytes = MCObjectHeap_liveBytes(g.heap);
  for (unsigned i = 0; i < 20; i++)
    CHECK(MCGameplay_reindexWorld(o) && MCGameplay_validateWorldIndexes(o));
  CHECK(w->loadedEntityList->modCount == loadedMod &&
        w->playerEntities->modCount == playersMod &&
        IntHashMap_lookupEntry(w->entitiesById, 17) == aEntry &&
        IntHashMap_lookupEntry(w->entitiesById, 71) == pEntry);
  CHECK(MCObjectHeap_liveObjects(g.heap) == live &&
        MCObjectHeap_liveBytes(g.heap) == bytes);
  Entity_setEntityId(&b->entity, 17);
  Entity_setEntityId(&a->entity, 0);
  Entity_setEntityId(&unrelated->entity, 30);
  Entity_setEntityId(&indexedOnly->entity, 41);
  CHECK(MCGameplay_reindexWorld(o));
  CHECK(IntHashMap_lookup(w->entitiesById, 17) == (MCObject *)b &&
        IntHashMap_lookup(w->entitiesById, 0) == (MCObject *)a);
  CHECK(!IntHashMap_lookup(w->entitiesById, -1) &&
        !IntHashMap_lookup(w->entitiesById, INT32_MIN) &&
        IntHashMap_lookup(w->entitiesById, 30) == (MCObject *)unrelated &&
        IntHashMap_lookup(w->entitiesById, 41) == (MCObject *)indexedOnly);
  CHECK(w->loadedEntityList->modCount == loadedMod &&
        w->playerEntities->modCount == playersMod &&
        IntHashMap_lookupEntry(w->entitiesById, 71) == pEntry);
  CHECK(MCGameplay_removeItem(&g, 0) && o->items[0] == (MCObject *)b &&
        o->itemCount == 1);
  CHECK(!IntHashMap_lookup(w->entitiesById, 0) &&
        NativeReferenceList_size(w->loadedEntityList) == 3 &&
        NativeReferenceList_get(w->loadedEntityList, 2) == (MCObject *)b);
  MCGameplayPlayer *replacement = player(&g, w, 88);
  CHECK(MCGameplay_setPlayer(&g, 0, "22222222-2222-2222-2222-222222222222",
                             (MCObject *)replacement));
  CHECK(!IntHashMap_lookup(w->entitiesById, 71) &&
        NativeReferenceList_get(w->playerEntities, 0) ==
            (MCObject *)replacement);
  CHECK(NativeReferenceList_get(w->loadedEntityList, 0) ==
            (MCObject *)unrelated &&
        NativeReferenceList_get(w->loadedEntityList, 1) == (MCObject *)b &&
        NativeReferenceList_get(w->loadedEntityList, 2) ==
            (MCObject *)replacement);
  World *next = MCGameplayWorld_new(g.heap, o, NULL, NULL);
  CHECK(next);
  b->entity.worldObj = (MCObject *)next;
  replacement->living.entity.worldObj = (MCObject *)next;
  MCObjectHeap_touch(g.heap);
  CHECK(MCGameplay_setWorld(&g, (MCObject *)next));
  CHECK(NativeReferenceList_size(w->loadedEntityList) == 1 &&
        NativeReferenceList_get(w->loadedEntityList, 0) ==
            (MCObject *)unrelated &&
        NativeReferenceList_size(w->playerEntities) == 0 &&
        w->entitiesById->count == 2 &&
        IntHashMap_lookup(w->entitiesById, 30) == (MCObject *)unrelated &&
        IntHashMap_lookup(w->entitiesById, 41) == (MCObject *)indexedOnly);
  CHECK(next->entitiesById->count == 2 &&
        NativeReferenceList_get(next->loadedEntityList, 0) ==
            (MCObject *)replacement &&
        NativeReferenceList_get(next->loadedEntityList, 1) == (MCObject *)b &&
        MCGameplay_validateWorldIndexes(o));
  CHECK(MCGameplay_setPlayer(&g, 0, NULL, NULL) &&
        NativeReferenceList_size(next->playerEntities) == 0 &&
        !IntHashMap_lookup(next->entitiesById, 88));
  MCObjectRootScope_end(&scope);
  CHECK(MCObjectHeap_collect(g.heap));
  CHECK(IntHashMap_lookup(next->entitiesById, 17) == o->items[0] &&
        MCGameplay_validateWorldIndexes(o));
  CHECK(MCGameplay_free(&g));
}
static bool frame_validate(MCGameplayObjects *o, void *context) {
  unsigned *calls = context;
  ++*calls;
  return MCGameplay_validateWorldIndexes(o);
}
static bool unused_player(const MCGameplayObjects *o, size_t i, mc_nbt *out,
                          void *c) {
  (void)o;
  (void)i;
  (void)out;
  (void)c;
  CHECK(0);
  return false;
}
static bool unused_items(const MCGameplayObjects *o, mc_nbt *out, void *c) {
  (void)o;
  (void)out;
  (void)c;
  CHECK(0);
  return false;
}
static void snapshot_gc_adopt_and_rejection(void) {
  MCGameplay g = {0};
  MCGameplayWorld *w = setup(&g);
  w->isRemote = true;
  EntityItem *a = item(&g, w, INT32_MIN), *b = item(&g, w, INT32_MAX);
  CHECK(MCGameplay_addItem(&g, (MCObject *)a) &&
        MCGameplay_addItem(&g, (MCObject *)b));
  IntHashMapEntry *entry = IntHashMap_lookupEntry(w->entitiesById, INT32_MIN);
  int32_t hash = MCObjectHeap_identityHashCode((MCObject *)entry);
  uint32_t mod = w->loadedEntityList->modCount;
  CHECK(MCObjectHeap_collect(g.heap) &&
        MCGameplay_validateWorldIndexes(MCGameplay_get(&g)));
  MCGameplayTransaction tx = {0};
  CHECK(MCGameplay_begin(&g, &tx));
  MCGameplayObjects *copy = MCGameplay_get(&tx.working);
  World *cw = (World *)copy->world;
  CHECK(copy->nativeEntityIndex != MCGameplay_get(&g)->nativeEntityIndex &&
        cw != w &&
        NativeReferenceList_get(cw->loadedEntityList, 0) == copy->items[0] &&
        copy->items[0] != (MCObject *)a);
  CHECK(IntHashMap_lookup(cw->entitiesById, INT32_MIN) == copy->items[0] &&
        MCObjectHeap_identityHashCode((MCObject *)IntHashMap_lookupEntry(
            cw->entitiesById, INT32_MIN)) == hash &&
        cw->loadedEntityList->modCount == mod &&
        MCGameplay_validateWorldIndexes(copy));
  Entity_setEntityId((Entity *)copy->items[0], 0);
  unsigned calls = 0;
  char error[160];
  CHECK(MCGameplay_acceptClientFrame(&tx, frame_validate, &calls, error,
                                     sizeof(error)) &&
        calls == 1);
  MCGameplayObjects *o = MCGameplay_get(&g);
  w = (World *)o->world;
  CHECK(!IntHashMap_lookup(w->entitiesById, INT32_MIN) &&
        IntHashMap_lookup(w->entitiesById, 0) == o->items[0] &&
        NativeReferenceList_get(w->loadedEntityList, 0) == o->items[0] &&
        MCGameplay_validateWorldIndexes(o));
  MCObject *parentA = o->items[0], *parentB = o->items[1];
  uint64_t serial = o->commitSerial;
  CHECK(MCGameplay_begin(&g, &tx));
  copy = MCGameplay_get(&tx.working);
  Entity_setEntityId((Entity *)copy->items[1], 0);
  calls = 0;
  CHECK(!MCGameplay_acceptClientFrame(&tx, frame_validate, &calls, error,
                                      sizeof(error)) &&
        calls == 0);
  CHECK(MCGameplay_get(&g) == o && o->items[0] == parentA &&
        o->items[1] == parentB && o->commitSerial == serial &&
        !MCObjectHeap_failed(g.heap) && MCGameplay_validateWorldIndexes(o));
  CHECK(MCGameplay_begin(&g, &tx));
  copy = MCGameplay_get(&tx.working);
  Entity_setEntityId((Entity *)copy->items[1], 0);
  const MCGameplayEncoders encoders = {unused_player, unused_items, NULL};
  CHECK(MCGameplay_commit(&tx, "invalid-index-never-written", &encoders, NULL,
                          error, sizeof(error)) == MC_GAMEPLAY_NOT_COMMITTED);
  CHECK(MCGameplay_get(&g) == o && o->commitSerial == serial &&
        !MCObjectHeap_failed(g.heap));
  CHECK(MCGameplay_free(&g));
}
static void invalid_objects_and_index_shape(void) {
  for (unsigned which = 0; which < 7; which++) {
    MCGameplay g = {0};
    MCGameplayWorld *w = setup(&g);
    MCGameplayObjects *o = MCGameplay_get(&g);
    EntityItem *e = item(&g, w, 9);
    CHECK(MCGameplay_addItem(&g, (MCObject *)e));
    if (which == 0) {
      ItemStack *wrong = ItemStack_new(g.heap, ItemStack_registryItem(1), 0, 0);
      CHECK(wrong);
      CHECK(!MCGameplay_addItem(&g, (MCObject *)wrong));
    } else if (which == 1) {
      EntityItem *wrong = item(&g, w, 9);
      CHECK(!MCGameplay_addItem(&g, (MCObject *)wrong));
    } else if (which == 2) {
      e->entity.worldObj = NULL;
      CHECK(!MCGameplay_reindexWorld(o));
    } else if (which == 3) {
      w->entitiesById->count = INT32_MAX;
      CHECK(!MCGameplay_reindexWorld(o));
    } else if (which == 4) {
      IntHashMapEntry *entry = IntHashMap_lookupEntry(w->entitiesById, 9);
      entry->nextEntry = entry;
      CHECK(!MCGameplay_reindexWorld(o));
    } else if (which == 5) {
      CHECK(IntHashMap_addKey(w->entitiesById, 8, (MCObject *)e));
      CHECK(!MCGameplay_validateWorldIndexes(o));
    } else {
      IntHashMapEntry *entry = IntHashMap_lookupEntry(w->entitiesById, 9);
      IntHashMapEntry *duplicate =
          (IntHashMapEntry *)MCObjectHeap_plainClone(g.heap, (MCObject *)entry);
      CHECK(duplicate);
      duplicate->nextEntry = entry;
      w->entitiesById->slots->values[IntHashMap_getSlotIndex(
          entry->slotHash, w->entitiesById->slots->length)] = duplicate;
      ++w->entitiesById->count;
      CHECK(!MCGameplay_reindexWorld(o));
    }
    CHECK(MCObjectHeap_failed(g.heap));
    CHECK(MCGameplay_free(&g));
  }
  /* Generic native graph fixtures remain valid without a Source World. */
  MCGameplay g = {0};
  CHECK(MCGameplay_init(&g, 1u << 20));
  NBTTagCompound *generic = NBTTagCompound_new(g.heap);
  CHECK(generic);
  CHECK(MCGameplay_setWorld(&g, (MCObject *)generic));
  ItemStack *s = ItemStack_new(g.heap, ItemStack_registryItem(1), 0, 0);
  CHECK(s);
  CHECK(MCGameplay_addItem(&g, (MCObject *)s) &&
        MCGameplay_validateWorldIndexes(MCGameplay_get(&g)));
  CHECK(MCGameplay_free(&g));
}
int main(void) {
  initial_admission();
  incremental_order_rekey_remove_and_world_switch();
  snapshot_gc_adopt_and_rejection();
  invalid_objects_and_index_shape();
  printf("world indexes: %u checks\n", checks);
  return 0;
}
