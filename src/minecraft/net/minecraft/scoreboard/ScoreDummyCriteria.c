#include "scoreboard/ScoreDummyCriteria.h"
#include "stats/ObjectiveStat.h"
extern const MCObjectClass c919_objective_stat_class;
static NBTString *name(MCObject *ctx, IScoreObjectiveCriteria *s) {
  (void)ctx;
  return ((ScoreDummyCriteria *)s)->dummyName;
}
static bool value(MCObject *ctx, IScoreObjectiveCriteria *s,
                  NativeReferenceList *p, int32_t *out) {
  (void)ctx;
  (void)s;
  (void)p;
  *out = 0;
  return true;
}
static bool readonly(MCObject *ctx, IScoreObjectiveCriteria *s, bool *out) {
  (void)ctx;
  (void)s;
  *out = false;
  return true;
}
static bool render(MCObject *ctx, IScoreObjectiveCriteria *s,
                   const ScoreRenderType **out) {
  (void)ctx;
  (void)s;
  *out = &ScoreRenderType_INTEGER;
  return true;
}
static const IScoreObjectiveCriteriaMethods methods = {name, value, readonly,
                                                       render};
void ScoreDummyCriteria_traceFields(ScoreDummyCriteria *s, MCObjectVisitor v,
                                    void *c) {
  if (MCObjectHeap_objectSize((MCObject *)s) < sizeof(*s)) {
    MCObjectHeap_fail(s->criteria.object.heap);
    return;
  }
  IScoreObjectiveCriteria_traceFields(&s->criteria, v, c);
  s->dummyName = (NBTString *)v((MCObject *)s->dummyName, c);
}
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
  ScoreDummyCriteria_traceFields((ScoreDummyCriteria *)o, v, c);
}
const MCObjectClass c919_score_dummy_class = {
    "net.minecraft.scoreboard.ScoreDummyCriteria", MCObjectHeap_plainClone,
    trace, NULL};
bool ScoreDummyCriteria_isInstance(const MCObject *o) {
  return IScoreObjectiveCriteria_isInstance(o) &&
         (o->klass == &c919_score_dummy_class ||
          o->klass == &c919_objective_stat_class);
}
bool ScoreDummyCriteria_construct(ScoreDummyCriteria *s, NBTString *n) {
  MCObjectHeap *h = s ? s->criteria.object.heap : NULL;
  if (!ScoreDummyCriteria_isInstance((MCObject *)s) ||
      (n && ((MCObject *)n)->heap != h)) {
    MCObjectHeap_fail(h);
    return false;
  }
  MCObjectRootScope scope = {0};
  if (!MCObjectRootScope_begin(&scope, h))
    return false;
  bool ok = MCObjectRootScope_pin(&scope, (MCObject *)s) &&
            MCObjectRootScope_pin(&scope, (MCObject *)n);
  if (ok) {
    s->criteria.methods = &methods;
    s->dummyName = n;
    MCObjectHeap_touch(h);
    ok = IScoreObjectiveCriteria_register(&s->criteria, n);
  }
  MCObjectRootScope_end(&scope);
  return ok;
}
ScoreDummyCriteria *ScoreDummyCriteria_new(MCObjectHeap *h, NBTString *n) {
  MCObjectRootScope scope = {0};
  if (!MCObjectRootScope_begin(&scope, h))
    return NULL;
  ScoreDummyCriteria *s = (ScoreDummyCriteria *)MCObjectHeap_alloc(
      h, sizeof(*s), &c919_score_dummy_class);
  if (s && !ScoreDummyCriteria_construct(s, n))
    s = NULL;
  MCObjectRootScope_end(&scope);
  return s;
}
