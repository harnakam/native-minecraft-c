#include "stats/ObjectiveStat.h"
#include "stats/StatBase.h"
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
  if (MCObjectHeap_objectSize(o) < sizeof(ObjectiveStat)) {
    MCObjectHeap_fail(o->heap);
    return;
  }
  ObjectiveStat *s = (ObjectiveStat *)o;
  ScoreDummyCriteria_traceFields(&s->dummy, v, c);
  s->stat = (StatBase *)v((MCObject *)s->stat, c);
}
const MCObjectClass c919_objective_stat_class = {
    "net.minecraft.stats.ObjectiveStat", MCObjectHeap_plainClone, trace, NULL};
bool ObjectiveStat_isInstance(const MCObject *o) {
  return o && o->klass == &c919_objective_stat_class &&
         MCObjectHeap_objectSize(o) >= sizeof(ObjectiveStat);
}
ObjectiveStat *ObjectiveStat_new(MCObjectHeap *h, StatBase *stat) {
  MCObjectRootScope scope = {0};
  if (!MCObjectRootScope_begin(&scope, h))
    return NULL;
  ObjectiveStat *s = (ObjectiveStat *)MCObjectHeap_alloc(
      h, sizeof(*s), &c919_objective_stat_class);
  if (!s)
    goto done;
  if (!stat || StatBase_getKind(stat) == STAT_BASE_KIND_UNKNOWN ||
      !MCObjectRootScope_pin(&scope, (MCObject *)stat)) {
    MCObjectHeap_fail(h);
    s = NULL;
    goto done;
  }
  if (!ScoreDummyCriteria_construct(&s->dummy, stat->statId)) {
    s = NULL;
    goto done;
  }
  s->stat = stat;
  MCObjectHeap_touch(h);
done:
  MCObjectRootScope_end(&scope);
  return s;
}
