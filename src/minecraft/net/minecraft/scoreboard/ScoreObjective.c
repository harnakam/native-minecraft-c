#include "scoreboard/Scoreboard.h"
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
  if (MCObjectHeap_objectSize(o) < sizeof(ScoreObjective)) {
    MCObjectHeap_fail(o->heap);
    return;
  }
  ScoreObjective *s = (ScoreObjective *)o;
  s->theScoreboard = (Scoreboard *)v((MCObject *)s->theScoreboard, c);
  s->name = (NBTString *)v((MCObject *)s->name, c);
  s->objectiveCriteria =
      (IScoreObjectiveCriteria *)v((MCObject *)s->objectiveCriteria, c);
  s->displayName = (NBTString *)v((MCObject *)s->displayName, c);
}
static const MCObjectClass klass = {"net.minecraft.scoreboard.ScoreObjective",
                                    MCObjectHeap_plainClone, trace, NULL};
bool ScoreObjective_isInstance(const MCObject *o) {
  return o && o->klass == &klass &&
         MCObjectHeap_objectSize(o) >= sizeof(ScoreObjective);
}
static bool valid(ScoreObjective *s) {
  if (!ScoreObjective_isInstance((MCObject *)s)) {
    MCObjectHeap_fail(s ? s->object.heap : NULL);
    return false;
  }
  return !MCObjectHeap_failed(s->object.heap);
}
ScoreObjective *ScoreObjective_new(MCObjectHeap *h, Scoreboard *b,
                                   NBTString *name,
                                   IScoreObjectiveCriteria *criteria) {
  MCObjectRootScope scope = {0};
  if (!MCObjectRootScope_begin(&scope, h))
    return NULL;
  ScoreObjective *s =
      (ScoreObjective *)MCObjectHeap_alloc(h, sizeof(*s), &klass);
  if (!s)
    goto done;
  if (!MCObjectRootScope_pin(&scope, (MCObject *)b) ||
      !MCObjectRootScope_pin(&scope, (MCObject *)name) ||
      !MCObjectRootScope_pin(&scope, (MCObject *)criteria)) {
    s = NULL;
    goto done;
  }
  s->theScoreboard = b;
  s->name = name;
  s->objectiveCriteria = criteria;
  s->displayName = name;
  MCObjectHeap_touch(h);
  if (!criteria) {
    MCObjectHeap_fail(h);
    s = NULL;
    goto done;
  }
  s->renderType = IScoreObjectiveCriteria_getRenderType(criteria);
  if (MCObjectHeap_failed(h))
    s = NULL;
done:
  MCObjectRootScope_end(&scope);
  return s;
}
Scoreboard *ScoreObjective_getScoreboard(ScoreObjective *s) {
  return valid(s) ? s->theScoreboard : NULL;
}
NBTString *ScoreObjective_getName(ScoreObjective *s) {
  return valid(s) ? s->name : NULL;
}
IScoreObjectiveCriteria *ScoreObjective_getCriteria(ScoreObjective *s) {
  return valid(s) ? s->objectiveCriteria : NULL;
}
NBTString *ScoreObjective_getDisplayName(ScoreObjective *s) {
  return valid(s) ? s->displayName : NULL;
}
const ScoreRenderType *ScoreObjective_getRenderType(ScoreObjective *s) {
  return valid(s) ? s->renderType : NULL;
}
bool ScoreObjective_setDisplayName(ScoreObjective *s, NBTString *name) {
  if (!valid(s))
    return false;
  MCObjectRootScope scope = {0};
  if (!MCObjectRootScope_begin(&scope, s->object.heap))
    return false;
  bool ok = MCObjectRootScope_pin(&scope, (MCObject *)s) &&
            MCObjectRootScope_pin(&scope, (MCObject *)name);
  if (ok) {
    s->displayName = name;
    MCObjectHeap_touch(s->object.heap);
    Scoreboard *board = s->theScoreboard;
    ok = MCObjectRootScope_pin(&scope, (MCObject *)board) &&
         Scoreboard_onObjectiveDisplayNameChanged(board, s);
    if (!ok)
      MCObjectHeap_fail(s->object.heap);
  }
  MCObjectRootScope_end(&scope);
  return ok;
}
bool ScoreObjective_setRenderType(ScoreObjective *s,
                                  const ScoreRenderType *type) {
  if (!valid(s))
    return false;
  if (type && type != &ScoreRenderType_INTEGER &&
      type != &ScoreRenderType_HEARTS) {
    MCObjectHeap_fail(s->object.heap);
    return false;
  }
  MCObjectRootScope scope = {0};
  if (!MCObjectRootScope_begin(&scope, s->object.heap))
    return false;
  bool ok = MCObjectRootScope_pin(&scope, (MCObject *)s);
  if (ok) {
    s->renderType = type;
    MCObjectHeap_touch(s->object.heap);
    Scoreboard *board = s->theScoreboard;
    ok = MCObjectRootScope_pin(&scope, (MCObject *)board) &&
         Scoreboard_onObjectiveDisplayNameChanged(board, s);
    if (!ok)
      MCObjectHeap_fail(s->object.heap);
  }
  MCObjectRootScope_end(&scope);
  return ok;
}
