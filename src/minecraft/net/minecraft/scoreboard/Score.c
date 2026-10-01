#include "scoreboard/Scoreboard.h"
#include <string.h>
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
  if (MCObjectHeap_objectSize(o) < sizeof(Score)) {
    MCObjectHeap_fail(o->heap);
    return;
  }
  Score *s = (Score *)o;
  s->theScoreboard = (Scoreboard *)v((MCObject *)s->theScoreboard, c);
  s->theScoreObjective =
      (ScoreObjective *)v((MCObject *)s->theScoreObjective, c);
  s->scorePlayerName = (NBTString *)v((MCObject *)s->scorePlayerName, c);
}
static const MCObjectClass klass = {"net.minecraft.scoreboard.Score",
                                    MCObjectHeap_plainClone, trace, NULL};
bool Score_isInstance(const MCObject *o) {
  return o && o->klass == &klass && MCObjectHeap_objectSize(o) >= sizeof(Score);
}
static bool valid(Score *s) {
  if (!Score_isInstance((MCObject *)s)) {
    MCObjectHeap_fail(s ? s->object.heap : NULL);
    return false;
  }
  return !MCObjectHeap_failed(s->object.heap);
}
Score *Score_new(MCObjectHeap *h, Scoreboard *b, ScoreObjective *o,
                 NBTString *name) {
  if ((b && ((MCObject *)b)->heap != h) || (o && ((MCObject *)o)->heap != h) ||
      (name && ((MCObject *)name)->heap != h)) {
    MCObjectHeap_fail(h);
    return NULL;
  }
  Score *s = (Score *)MCObjectHeap_alloc(h, sizeof(*s), &klass);
  if (s) {
    s->theScoreboard = b;
    s->theScoreObjective = o;
    s->scorePlayerName = name;
    s->forceUpdate = true;
  }
  return s;
}
int32_t Score_getScorePoints(Score *s) { return valid(s) ? s->scorePoints : 0; }
ScoreObjective *Score_getObjective(Score *s) {
  return valid(s) ? s->theScoreObjective : NULL;
}
NBTString *Score_getPlayerName(Score *s) {
  return valid(s) ? s->scorePlayerName : NULL;
}
Scoreboard *Score_getScoreScoreboard(Score *s) {
  return valid(s) ? s->theScoreboard : NULL;
}
bool Score_isLocked(Score *s) { return valid(s) && s->locked; }
bool Score_setLocked(Score *s, bool locked) {
  if (!valid(s))
    return false;
  s->locked = locked;
  MCObjectHeap_touch(s->object.heap);
  return true;
}
bool Score_setScorePoints(Score *s, int32_t points) {
  if (!valid(s))
    return false;
  MCObjectRootScope scope = {0};
  if (!MCObjectRootScope_begin(&scope, s->object.heap))
    return false;
  bool ok = MCObjectRootScope_pin(&scope, (MCObject *)s);
  if (ok) {
    int32_t previous = s->scorePoints;
    s->scorePoints = points;
    MCObjectHeap_touch(s->object.heap);
    if (previous != points || s->forceUpdate) {
      s->forceUpdate = false;
      Scoreboard *board = Score_getScoreScoreboard(s);
      ok = MCObjectRootScope_pin(&scope, (MCObject *)board) &&
           Scoreboard_func_96536_a(board, s);
      if (!ok)
        MCObjectHeap_fail(s->object.heap);
    }
  }
  MCObjectRootScope_end(&scope);
  return ok && !MCObjectHeap_failed(s->object.heap);
}
static bool writable(Score *s) {
  ScoreObjective *objective = s->theScoreObjective;
  if (objective && ((MCObject *)objective)->heap != s->object.heap) {
    MCObjectHeap_fail(s->object.heap);
    return false;
  }
  IScoreObjectiveCriteria *c = ScoreObjective_getCriteria(objective);
  if (!c || ((MCObject *)c)->heap != s->object.heap) {
    MCObjectHeap_fail(s->object.heap);
    return false;
  }
  bool readonly = IScoreObjectiveCriteria_isReadOnly(c);
  if (readonly)
    MCObjectHeap_fail(s->object.heap);
  return !MCObjectHeap_failed(s->object.heap);
}
static int32_t wrap(uint32_t bits) {
  int32_t value;
  memcpy(&value, &bits, sizeof(value));
  return value;
}
static bool change(Score *s, int32_t amount, bool subtract) {
  if (!valid(s))
    return false;
  MCObjectRootScope scope = {0};
  if (!MCObjectRootScope_begin(&scope, s->object.heap))
    return false;
  bool ok = MCObjectRootScope_pin(&scope, (MCObject *)s) && writable(s);
  if (ok) {
    uint32_t old = (uint32_t)Score_getScorePoints(s);
    ok = Score_setScorePoints(
        s, wrap(subtract ? old - (uint32_t)amount : old + (uint32_t)amount));
  }
  MCObjectRootScope_end(&scope);
  return ok;
}
bool Score_increseScore(Score *s, int32_t amount) {
  return change(s, amount, false);
}
bool Score_decreaseScore(Score *s, int32_t amount) {
  return change(s, amount, true);
}
bool Score_func_96648_a(Score *s) {
  if (!valid(s))
    return false;
  MCObjectRootScope scope = {0};
  if (!MCObjectRootScope_begin(&scope, s->object.heap))
    return false;
  bool ok = MCObjectRootScope_pin(&scope, (MCObject *)s) && writable(s) &&
            Score_increseScore(s, 1);
  MCObjectRootScope_end(&scope);
  return ok;
}
