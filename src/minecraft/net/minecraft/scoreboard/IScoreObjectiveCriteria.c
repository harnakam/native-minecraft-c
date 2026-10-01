#include "scoreboard/IScoreObjectiveCriteria.h"
#include "scoreboard/ScoreDummyCriteria.h"
#include "stats/ObjectiveStat.h"
#include "util/NativeHashMap.h"
extern const MCObjectClass c919_score_dummy_class, c919_objective_stat_class;
const ScoreRenderType ScoreRenderType_INTEGER = {0, "integer"},
                      ScoreRenderType_HEARTS = {1, "hearts"};
static bool fail(MCObjectHeap *h) {
  MCObjectHeap_fail(h);
  return false;
}
void IScoreObjectiveCriteria_traceFields(IScoreObjectiveCriteria *s,
                                         MCObjectVisitor v, void *c) {
  if (MCObjectHeap_objectSize((MCObject *)s) < sizeof(*s)) {
    MCObjectHeap_fail(s->object.heap);
    return;
  }
  s->context = v(s->context, c);
}
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
  IScoreObjectiveCriteria_traceFields((IScoreObjectiveCriteria *)o, v, c);
}
static const MCObjectClass customClass = {"native.IScoreObjectiveCriteria",
                                          MCObjectHeap_plainClone, trace, NULL};
bool IScoreObjectiveCriteria_isInstance(const MCObject *o) {
  return o &&
         MCObjectHeap_objectSize(o) >=
             (o->klass == &c919_objective_stat_class ? sizeof(ObjectiveStat)
              : o->klass == &c919_score_dummy_class
                  ? sizeof(ScoreDummyCriteria)
                  : sizeof(IScoreObjectiveCriteria)) &&
         (o->klass == &customClass || o->klass == &c919_score_dummy_class ||
          o->klass == &c919_objective_stat_class);
}
IScoreObjectiveCriteria *
IScoreObjectiveCriteria_nativeNew(MCObjectHeap *h,
                                  const IScoreObjectiveCriteriaMethods *methods,
                                  MCObject *context) {
  if (!methods || (context && context->heap != h)) {
    fail(h);
    return NULL;
  }
  IScoreObjectiveCriteria *s = (IScoreObjectiveCriteria *)MCObjectHeap_alloc(
      h, sizeof(*s), &customClass);
  if (s) {
    s->methods = methods;
    s->context = context;
  }
  return s;
}
static bool begin(IScoreObjectiveCriteria *s, MCObjectRootScope *scope) {
  MCObjectHeap *h = s ? s->object.heap : NULL;
  if (!IScoreObjectiveCriteria_isInstance((MCObject *)s) || !s->methods ||
      MCObjectHeap_failed(h) || !MCObjectRootScope_begin(scope, h))
    return fail(h);
  if (MCObjectRootScope_pin(scope, (MCObject *)s) &&
      MCObjectRootScope_pin(scope, s->context))
    return true;
  MCObjectRootScope_end(scope);
  return false;
}
NBTString *IScoreObjectiveCriteria_getName(IScoreObjectiveCriteria *s) {
  MCObjectRootScope scope = {0};
  if (!begin(s, &scope))
    return NULL;
  NBTString *out = NULL;
  const IScoreObjectiveCriteriaMethods *m = s->methods;
  MCObject *ctx = s->context;
  if (m->getName)
    out = m->getName(ctx, s);
  else
    fail(s->object.heap);
  if (out && (!NBTString_isInstance((MCObject *)out) ||
              ((MCObject *)out)->heap != s->object.heap)) {
    fail(s->object.heap);
    out = NULL;
  }
  MCObjectRootScope_end(&scope);
  return MCObjectHeap_failed(s->object.heap) ? NULL : out;
}
bool IScoreObjectiveCriteria_isReadOnly(IScoreObjectiveCriteria *s) {
  MCObjectRootScope scope = {0};
  if (!begin(s, &scope))
    return false;
  bool out = false;
  const IScoreObjectiveCriteriaMethods *m = s->methods;
  MCObject *ctx = s->context;
  if (!m->isReadOnly || !m->isReadOnly(ctx, s, &out))
    fail(s->object.heap);
  MCObjectRootScope_end(&scope);
  return out;
}
const ScoreRenderType *
IScoreObjectiveCriteria_getRenderType(IScoreObjectiveCriteria *s) {
  MCObjectRootScope scope = {0};
  if (!begin(s, &scope))
    return NULL;
  const ScoreRenderType *out = NULL;
  const IScoreObjectiveCriteriaMethods *m = s->methods;
  MCObject *ctx = s->context;
  if (!m->getRenderType || !m->getRenderType(ctx, s, &out))
    fail(s->object.heap);
  if (out && out != &ScoreRenderType_INTEGER &&
      out != &ScoreRenderType_HEARTS) {
    fail(s->object.heap);
    out = NULL;
  }
  MCObjectRootScope_end(&scope);
  return out;
}
bool IScoreObjectiveCriteria_setScore(IScoreObjectiveCriteria *s,
                                      NativeReferenceList *players,
                                      int32_t *out) {
  MCObjectRootScope scope = {0};
  if (!begin(s, &scope))
    return false;
  int32_t value = 0;
  bool ok = out && MCObjectRootScope_pin(&scope, (MCObject *)players);
  const IScoreObjectiveCriteriaMethods *m = s->methods;
  MCObject *ctx = s->context;
  if (ok)
    ok = m->setScore && m->setScore(ctx, s, players, &value);
  if (!ok)
    fail(s->object.heap);
  ok = ok && !MCObjectHeap_failed(s->object.heap);
  if (ok)
    *out = value;
  MCObjectRootScope_end(&scope);
  return ok;
}
typedef struct {
  MCObject object;
  NativeHashMap *instances;
} Registry;
static void registry_trace(MCObject *o, MCObjectVisitor v, void *c) {
  Registry *r = (Registry *)o;
  r->instances = (NativeHashMap *)v((MCObject *)r->instances, c);
}
static const MCObjectClass registryClass = {
    "native.IScoreObjectiveCriteria.INSTANCES", MCObjectHeap_plainClone,
    registry_trace, NULL};
static bool any(const MCObject *o, void *c) {
  (void)o;
  (void)c;
  return true;
}
static Registry *registry(MCObjectHeap *h) {
  Registry *r =
      (Registry *)MCObjectHeap_findObject(h, &registryClass, any, NULL);
  if (!r) {
    r = (Registry *)MCObjectHeap_alloc(h, sizeof(*r), &registryClass);
    if (r) {
      r->instances = NativeHashMap_new(h, NATIVE_HASH_KEY_STRING);
      MCObjectRoot root = {0};
      if (!r->instances || !MCObjectRoot_init(&root, h, (MCObject *)r))
        r = NULL;
    }
  }
  return r;
}
bool IScoreObjectiveCriteria_register(IScoreObjectiveCriteria *s,
                                      NBTString *name) {
  MCObjectRootScope scope = {0};
  if (!begin(s, &scope))
    return false;
  bool ok = MCObjectRootScope_pin(&scope, (MCObject *)name);
  Registry *r = ok ? registry(s->object.heap) : NULL;
  ok = r && NativeHashMap_put(r->instances, (MCObject *)name, (MCObject *)s);
  MCObjectRootScope_end(&scope);
  return ok;
}
IScoreObjectiveCriteria *IScoreObjectiveCriteria_find(MCObjectHeap *h,
                                                      NBTString *name) {
  MCObjectRootScope scope = {0};
  if (!MCObjectRootScope_begin(&scope, h))
    return NULL;
  Registry *r =
      MCObjectRootScope_pin(&scope, (MCObject *)name) ? registry(h) : NULL;
  IScoreObjectiveCriteria *out =
      r ? (IScoreObjectiveCriteria *)NativeHashMap_get(r->instances,
                                                       (MCObject *)name)
        : NULL;
  MCObjectRootScope_end(&scope);
  return out;
}
