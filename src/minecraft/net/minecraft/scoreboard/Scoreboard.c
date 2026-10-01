#include "scoreboard/Scoreboard.h"
static bool fail(MCObjectHeap *h) {
  MCObjectHeap_fail(h);
  return false;
}
static void array_trace(MCObject *o, MCObjectVisitor v, void *c) {
  if (MCObjectHeap_objectSize(o) < sizeof(ScoreObjectiveArray)) {
    fail(o->heap);
    return;
  }
  ScoreObjectiveArray *a = (ScoreObjectiveArray *)o;
  for (int i = 0; i < 19; i++)
    a->items[i] = (ScoreObjective *)v((MCObject *)a->items[i], c);
}
static const MCObjectClass arrayClass = {
    "native.ScoreObjective[19]", MCObjectHeap_plainClone, array_trace, NULL};
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
  if (MCObjectHeap_objectSize(o) < sizeof(Scoreboard)) {
    fail(o->heap);
    return;
  }
  Scoreboard *b = (Scoreboard *)o;
  b->scoreObjectives = (NativeHashMap *)v((MCObject *)b->scoreObjectives, c);
  b->scoreObjectiveCriterias =
      (NativeHashMap *)v((MCObject *)b->scoreObjectiveCriterias, c);
  b->entitiesScoreObjectives =
      (NativeHashMap *)v((MCObject *)b->entitiesScoreObjectives, c);
  b->objectiveDisplaySlots =
      (ScoreObjectiveArray *)v((MCObject *)b->objectiveDisplaySlots, c);
  b->teams = (NativeHashMap *)v((MCObject *)b->teams, c);
  b->teamMemberships = (NativeHashMap *)v((MCObject *)b->teamMemberships, c);
  b->context = v(b->context, c);
}
static const MCObjectClass klass = {"net.minecraft.scoreboard.Scoreboard",
                                    MCObjectHeap_plainClone, trace, NULL};
bool Scoreboard_isInstance(const MCObject *o) {
  return o && o->klass == &klass &&
         MCObjectHeap_objectSize(o) >= sizeof(Scoreboard);
}
static bool valid(Scoreboard *b) {
  if (!Scoreboard_isInstance((MCObject *)b) ||
      MCObjectHeap_failed(b ? b->object.heap : NULL))
    return fail(b ? b->object.heap : NULL);
  MCObjectHeap *h = b->object.heap;
  NativeHashMap *maps[] = {b->scoreObjectives, b->scoreObjectiveCriterias,
                           b->entitiesScoreObjectives, b->teams,
                           b->teamMemberships};
  for (int i = 0; i < 5; i++)
    if (!NativeHashMap_isInstance((MCObject *)maps[i]) ||
        ((MCObject *)maps[i])->heap != h)
      return fail(h);
  if (b->context && b->context->heap != h)
    return fail(h);
  return true;
}
static bool begin(Scoreboard *b, MCObjectRootScope *s, MCObject *a,
                  MCObject *c) {
  if (!valid(b) || !MCObjectRootScope_begin(s, b->object.heap))
    return false;
  if (MCObjectRootScope_pin(s, (MCObject *)b) && MCObjectRootScope_pin(s, a) &&
      MCObjectRootScope_pin(s, c))
    return true;
  MCObjectRootScope_end(s);
  return false;
}
Scoreboard *Scoreboard_new(MCObjectHeap *h) {
  MCObjectRootScope scope = {0};
  if (!MCObjectRootScope_begin(&scope, h))
    return NULL;
  Scoreboard *b = (Scoreboard *)MCObjectHeap_alloc(h, sizeof(*b), &klass);
  if (!b)
    goto done;
  b->scoreObjectives = NativeHashMap_new(h, NATIVE_HASH_KEY_STRING);
  if (!b->scoreObjectives)
    goto failed;
  b->scoreObjectiveCriterias = NativeHashMap_new(h, NATIVE_HASH_KEY_IDENTITY);
  if (!b->scoreObjectiveCriterias)
    goto failed;
  b->entitiesScoreObjectives = NativeHashMap_new(h, NATIVE_HASH_KEY_STRING);
  if (!b->entitiesScoreObjectives)
    goto failed;
  b->objectiveDisplaySlots = (ScoreObjectiveArray *)MCObjectHeap_alloc(
      h, sizeof(*b->objectiveDisplaySlots), &arrayClass);
  if (!b->objectiveDisplaySlots)
    goto failed;
  b->teams = NativeHashMap_new(h, NATIVE_HASH_KEY_STRING);
  if (!b->teams)
    goto failed;
  b->teamMemberships = NativeHashMap_new(h, NATIVE_HASH_KEY_STRING);
  if (!b->teamMemberships)
    goto failed;
  goto done;
failed:
  b = NULL;
done:
  MCObjectRootScope_end(&scope);
  return b;
}
bool Scoreboard_nativeBindOverrides(Scoreboard *b, const ScoreboardOverrides *o,
                                    MCObject *ctx) {
  if (!valid(b) || (ctx && ctx->heap != b->object.heap))
    return fail(b ? b->object.heap : NULL);
  b->overrides = o;
  b->context = ctx;
  MCObjectHeap_touch(b->object.heap);
  return true;
}
/* These base methods really have empty original bodies. Installed native
   overrides are required effects and cannot silently fall back to the base. */
#define HOOK1(function, member, T)                                             \
  bool function(Scoreboard *b, T *value) {                                     \
    MCObjectRootScope s = {0};                                                 \
    if (!begin(b, &s, (MCObject *)value, NULL))                                \
      return false;                                                            \
    bool ok = true;                                                            \
    const ScoreboardOverrides *o = b->overrides;                               \
    MCObject *ctx = b->context;                                                \
    if (o)                                                                     \
      ok = MCObjectRootScope_pin(&s, ctx) && o->member &&                      \
           o->member(ctx, b, value);                                           \
    if (!ok)                                                                   \
      fail(b->object.heap);                                                    \
    MCObjectRootScope_end(&s);                                                 \
    return ok && !MCObjectHeap_failed(b->object.heap);                         \
  }
HOOK1(Scoreboard_onScoreObjectiveAdded, onScoreObjectiveAdded, ScoreObjective)
HOOK1(Scoreboard_onObjectiveDisplayNameChanged, onObjectiveDisplayNameChanged,
      ScoreObjective)
HOOK1(Scoreboard_onScoreObjectiveRemoved, onScoreObjectiveRemoved,
      ScoreObjective)
HOOK1(Scoreboard_func_96536_a, onScoreChanged, Score)
HOOK1(Scoreboard_func_96516_a, onEntityScoresRemoved, NBTString)
#undef HOOK1
bool Scoreboard_func_178820_a(Scoreboard *b, NBTString *name,
                              ScoreObjective *objective) {
  MCObjectRootScope s = {0};
  if (!begin(b, &s, (MCObject *)name, (MCObject *)objective))
    return false;
  bool ok = true;
  const ScoreboardOverrides *o = b->overrides;
  MCObject *ctx = b->context;
  if (o)
    ok = MCObjectRootScope_pin(&s, ctx) && o->onEntityObjectiveRemoved &&
         o->onEntityObjectiveRemoved(ctx, b, name, objective);
  if (!ok)
    fail(b->object.heap);
  MCObjectRootScope_end(&s);
  return ok && !MCObjectHeap_failed(b->object.heap);
}
ScoreObjective *Scoreboard_getObjective(Scoreboard *b, NBTString *name) {
  return valid(b) ? (ScoreObjective *)NativeHashMap_get(b->scoreObjectives,
                                                        (MCObject *)name)
                  : NULL;
}
static bool length_valid(Scoreboard *b, NBTString *name, size_t max) {
  return name && NBTString_isInstance((MCObject *)name) &&
                 ((MCObject *)name)->heap == b->object.heap &&
                 NBTString_length(name) <= max
             ? true
             : fail(b->object.heap);
}
ScoreObjective *
Scoreboard_addScoreObjective(Scoreboard *b, NBTString *name,
                             IScoreObjectiveCriteria *criteria) {
  MCObjectRootScope s = {0};
  if (!begin(b, &s, (MCObject *)name, (MCObject *)criteria))
    return NULL;
  ScoreObjective *out = NULL;
  if (!length_valid(b, name, 16))
    goto done;
  if (Scoreboard_getObjective(b, name)) {
    fail(b->object.heap);
    goto done;
  }
  if (MCObjectHeap_failed(b->object.heap))
    goto done;
  out = ScoreObjective_new(b->object.heap, b, name, criteria);
  if (!out)
    goto done;
  NativeReferenceList *list = (NativeReferenceList *)NativeHashMap_get(
      b->scoreObjectiveCriterias, (MCObject *)criteria);
  if (MCObjectHeap_failed(b->object.heap))
    goto failed;
  if (!list) {
    list = NativeReferenceList_new(b->object.heap);
    if (!list || !NativeHashMap_put(b->scoreObjectiveCriterias,
                                    (MCObject *)criteria, (MCObject *)list))
      goto failed;
  }
  if (!NativeReferenceList_add(list, (MCObject *)out) ||
      !NativeHashMap_put(b->scoreObjectives, (MCObject *)name,
                         (MCObject *)out) ||
      !Scoreboard_onScoreObjectiveAdded(b, out))
    goto failed;
  goto done;
failed:
  out = NULL;
done:
  MCObjectRootScope_end(&s);
  return out;
}
NativeReferenceList *
Scoreboard_getObjectivesFromCriteria(Scoreboard *b,
                                     IScoreObjectiveCriteria *c) {
  MCObjectRootScope s = {0};
  if (!begin(b, &s, (MCObject *)c, NULL))
    return NULL;
  NativeReferenceList *list = (NativeReferenceList *)NativeHashMap_get(
                          b->scoreObjectiveCriterias, (MCObject *)c),
                      *out = NULL;
  if (!MCObjectHeap_failed(b->object.heap))
    out = list ? NativeReferenceList_copy(list)
               : NativeReferenceList_new(b->object.heap);
  MCObjectRootScope_end(&s);
  return out;
}
bool Scoreboard_entityHasObjective(Scoreboard *b, NBTString *name,
                                   ScoreObjective *o) {
  if (!valid(b))
    return false;
  NativeHashMap *map = (NativeHashMap *)NativeHashMap_get(
      b->entitiesScoreObjectives, (MCObject *)name);
  return map && NativeHashMap_get(map, (MCObject *)o) != NULL;
}
Score *Scoreboard_getValueFromObjective(Scoreboard *b, NBTString *name,
                                        ScoreObjective *o) {
  MCObjectRootScope s = {0};
  if (!begin(b, &s, (MCObject *)name, (MCObject *)o))
    return NULL;
  Score *out = NULL;
  if (!length_valid(b, name, 40))
    goto done;
  NativeHashMap *map = (NativeHashMap *)NativeHashMap_get(
      b->entitiesScoreObjectives, (MCObject *)name);
  if (MCObjectHeap_failed(b->object.heap))
    goto done;
  if (!map) {
    map = NativeHashMap_new(b->object.heap, NATIVE_HASH_KEY_IDENTITY);
    if (!map || !NativeHashMap_put(b->entitiesScoreObjectives, (MCObject *)name,
                                   (MCObject *)map))
      goto done;
  }
  out = (Score *)NativeHashMap_get(map, (MCObject *)o);
  if (!out && !MCObjectHeap_failed(b->object.heap)) {
    out = Score_new(b->object.heap, b, o, name);
    if (out && !NativeHashMap_put(map, (MCObject *)o, (MCObject *)out))
      out = NULL;
  }
done:
  MCObjectRootScope_end(&s);
  return out;
}
NativeHashMapView *Scoreboard_getScoreObjectives(Scoreboard *b) {
  return valid(b) ? NativeHashMap_values(b->scoreObjectives) : NULL;
}
NativeHashMapView *Scoreboard_getObjectiveNames(Scoreboard *b) {
  return valid(b) ? NativeHashMap_keys(b->entitiesScoreObjectives) : NULL;
}
NativeHashMap *Scoreboard_getObjectivesForEntity(Scoreboard *b,
                                                 NBTString *name) {
  if (!valid(b))
    return NULL;
  NativeHashMap *map = (NativeHashMap *)NativeHashMap_get(
      b->entitiesScoreObjectives, (MCObject *)name);
  if (!map && !MCObjectHeap_failed(b->object.heap))
    map = NativeHashMap_new(b->object.heap, NATIVE_HASH_KEY_IDENTITY);
  return map;
}
bool Scoreboard_removeObjectiveFromEntity(Scoreboard *b, NBTString *name,
                                          ScoreObjective *o) {
  MCObjectRootScope s = {0};
  if (!begin(b, &s, (MCObject *)name, (MCObject *)o))
    return false;
  bool ok = true;
  if (!o) {
    NativeHashMap *m = (NativeHashMap *)NativeHashMap_remove(
        b->entitiesScoreObjectives, (MCObject *)name);
    if (m)
      ok = Scoreboard_func_96516_a(b, name);
  } else {
    NativeHashMap *m = (NativeHashMap *)NativeHashMap_get(
        b->entitiesScoreObjectives, (MCObject *)name);
    if (m) {
      Score *old = (Score *)NativeHashMap_remove(m, (MCObject *)o);
      if (NativeHashMap_size(m) < 1) {
        NativeHashMap *removed = (NativeHashMap *)NativeHashMap_remove(
            b->entitiesScoreObjectives, (MCObject *)name);
        if (removed)
          ok = Scoreboard_func_96516_a(b, name);
      } else if (old)
        ok = Scoreboard_func_178820_a(b, name, o);
    }
  }
  MCObjectRootScope_end(&s);
  return ok && !MCObjectHeap_failed(b->object.heap);
}
NativeReferenceList *Scoreboard_getScores(Scoreboard *b) {
  MCObjectRootScope s = {0};
  if (!begin(b, &s, NULL, NULL))
    return NULL;
  NativeHashMapView *outer = NativeHashMap_values(b->entitiesScoreObjectives);
  NativeReferenceList *out =
      outer ? NativeReferenceList_new(b->object.heap) : NULL;
  NativeIterator *i = out ? NativeIterator_fromView(outer) : NULL;
  if (!i)
    goto failed;
  while (NativeIterator_hasNext(i)) {
    MCObject *map = NULL;
    if (!NativeIterator_next(i, &map))
      goto failed;
    NativeHashMapView *values = NativeHashMap_values((NativeHashMap *)map);
    NativeIterator *j = values ? NativeIterator_fromView(values) : NULL;
    if (!j)
      goto failed;
    while (NativeIterator_hasNext(j)) {
      MCObject *score = NULL;
      if (!NativeIterator_next(j, &score) ||
          !NativeReferenceList_add(out, score))
        goto failed;
    }
  }
  if (MCObjectHeap_failed(b->object.heap))
    goto failed;
  goto done;
failed:
  out = NULL;
done:
  MCObjectRootScope_end(&s);
  return out;
}
static bool array_valid(Scoreboard *b, int32_t i) {
  ScoreObjectiveArray *a = b->objectiveDisplaySlots;
  return a && a->object.heap == b->object.heap &&
                 a->object.klass == &arrayClass &&
                 MCObjectHeap_objectSize((MCObject *)a) >= sizeof(*a) &&
                 i >= 0 && i < 19
             ? true
             : fail(b->object.heap);
}
bool Scoreboard_setObjectiveInDisplaySlot(Scoreboard *b, int32_t i,
                                          ScoreObjective *o) {
  if (!valid(b) || !array_valid(b, i))
    return false;
  if (o && ((MCObject *)o)->heap != b->object.heap)
    return fail(b->object.heap);
  b->objectiveDisplaySlots->items[i] = o;
  MCObjectHeap_touch(b->object.heap);
  return true;
}
ScoreObjective *Scoreboard_getObjectiveInDisplaySlot(Scoreboard *b, int32_t i) {
  return valid(b) && array_valid(b, i) ? b->objectiveDisplaySlots->items[i]
                                       : NULL;
}
bool Scoreboard_removeObjective(Scoreboard *b, ScoreObjective *o) {
  MCObjectRootScope s = {0};
  if (!begin(b, &s, (MCObject *)o, NULL))
    return false;
  bool ok = false;
  NBTString *name = ScoreObjective_getName(o);
  if (!o || MCObjectHeap_failed(b->object.heap)) {
    fail(b->object.heap);
    goto done;
  }
  NativeHashMap_remove(b->scoreObjectives, (MCObject *)name);
  if (MCObjectHeap_failed(b->object.heap))
    goto done;
  for (int i = 0; i < 19; i++)
    if (Scoreboard_getObjectiveInDisplaySlot(b, i) == o &&
        !Scoreboard_setObjectiveInDisplaySlot(b, i, NULL))
      goto done;
  IScoreObjectiveCriteria *c = ScoreObjective_getCriteria(o);
  NativeReferenceList *list = (NativeReferenceList *)NativeHashMap_get(
      b->scoreObjectiveCriterias, (MCObject *)c);
  if (list) {
    int32_t size = NativeReferenceList_size(list);
    for (int32_t j = 0; j < size; j++)
      if (NativeReferenceList_get(list, j) == (MCObject *)o) {
        NativeReferenceList_remove(list, j);
        break;
      }
  }
  NativeHashMapView *v = NativeHashMap_values(b->entitiesScoreObjectives);
  NativeIterator *i = v ? NativeIterator_fromView(v) : NULL;
  if (!i)
    goto done;
  while (NativeIterator_hasNext(i)) {
    MCObject *map = NULL;
    if (!NativeIterator_next(i, &map))
      goto done;
    NativeHashMap_remove((NativeHashMap *)map, (MCObject *)o);
    if (MCObjectHeap_failed(b->object.heap))
      goto done;
  }
  ok = Scoreboard_onScoreObjectiveRemoved(b, o);
done:
  MCObjectRootScope_end(&s);
  return ok && !MCObjectHeap_failed(b->object.heap);
}
