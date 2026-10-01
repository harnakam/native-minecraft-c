#include "scoreboard/ScoreDummyCriteria.h"
#include "scoreboard/Scoreboard.h"
#include "stats/ObjectiveStat.h"
#include "stats/StatBase.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
static int checks;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      fprintf(stderr, "check %d line %d: %s\n", checks, __LINE__, #x);         \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
typedef struct {
  MCObject object;
  int added, changed, display, removed, all, one, readonlyCalls;
  bool readOnly, fail, mutate;
} Log;
static const MCObjectClass logClass = {"fixture.Scoreboard.Log",
                                       MCObjectHeap_plainClone, NULL, NULL};
static bool added(MCObject *c, Scoreboard *b, ScoreObjective *o) {
  Log *l = (Log *)c;
  ++l->added;
  CHECK(Scoreboard_getObjective(b, o->name) == o);
  CHECK(!MCObjectHeap_collect(b->object.heap));
  return !l->fail;
}
static bool display(MCObject *c, Scoreboard *b, ScoreObjective *o) {
  (void)b;
  (void)o;
  ++((Log *)c)->display;
  return !((Log *)c)->fail;
}
static bool removed(MCObject *c, Scoreboard *b, ScoreObjective *o) {
  (void)b;
  (void)o;
  ++((Log *)c)->removed;
  return !((Log *)c)->fail;
}
static bool changed(MCObject *c, Scoreboard *b, Score *s) {
  (void)b;
  Log *l = (Log *)c;
  ++l->changed;
  CHECK(!s->forceUpdate);
  return !l->fail;
}
static bool all(MCObject *c, Scoreboard *b, NBTString *n) {
  (void)b;
  (void)n;
  ++((Log *)c)->all;
  return !((Log *)c)->fail;
}
static bool one(MCObject *c, Scoreboard *b, NBTString *n, ScoreObjective *o) {
  (void)b;
  (void)n;
  (void)o;
  ++((Log *)c)->one;
  return !((Log *)c)->fail;
}
static const ScoreboardOverrides hooks = {added,   display, removed,
                                          changed, all,     one};
static NBTString *criterionName(MCObject *c, IScoreObjectiveCriteria *s) {
  (void)c;
  (void)s;
  return NULL;
}
static bool criterionValue(MCObject *c, IScoreObjectiveCriteria *s,
                           NativeReferenceList *p, int32_t *out) {
  (void)c;
  (void)s;
  (void)p;
  *out = 7;
  return true;
}
static bool criterionReadonly(MCObject *c, IScoreObjectiveCriteria *s,
                              bool *out) {
  (void)s;
  Log *l = (Log *)c;
  ++l->readonlyCalls;
  *out = l->readOnly;
  if (l->mutate && l->readonlyCalls == 1)
    l->readOnly = true;
  return !l->fail;
}
static bool criterionRender(MCObject *c, IScoreObjectiveCriteria *s,
                            const ScoreRenderType **out) {
  (void)c;
  (void)s;
  *out = &ScoreRenderType_HEARTS;
  return true;
}
static const IScoreObjectiveCriteriaMethods methods = {
    criterionName, criterionValue, criterionReadonly, criterionRender};
static void normal(void) {
  MCObjectHeap *heap = MCObjectHeap_new(1u << 22);
  CHECK(heap);
  Scoreboard *board = Scoreboard_new(heap);
  CHECK(board);
  NativeHashMap *maps[] = {
      board->scoreObjectives, board->scoreObjectiveCriterias,
      board->entitiesScoreObjectives, board->teams, board->teamMemberships};
  for (int i = 0; i < 5; i++) {
    CHECK(maps[i] && NativeHashMap_size(maps[i]) == 0);
    for (int j = 0; j < i; j++)
      CHECK(maps[i] != maps[j]);
  }
  for (int i = 0; i < 19; i++)
    CHECK(!Scoreboard_getObjectiveInDisplaySlot(board, i));
  Log *log = (Log *)MCObjectHeap_alloc(heap, sizeof(*log), &logClass);
  CHECK(log && Scoreboard_nativeBindOverrides(board, &hooks, (MCObject *)log));
  NBTString *a = NBTString_fromASCII(heap, "a"),
            *b = NBTString_fromASCII(heap, "b"),
            *p = NBTString_fromASCII(heap, "player");
  ScoreDummyCriteria *dummy = ScoreDummyCriteria_new(heap, a);
  CHECK(dummy && dummy->dummyName == a);
  CHECK(IScoreObjectiveCriteria_find(heap, a) == &dummy->criteria);
  ScoreDummyCriteria *equal =
      ScoreDummyCriteria_new(heap, NBTString_fromASCII(heap, "a"));
  CHECK(equal && equal != dummy);
  CHECK(IScoreObjectiveCriteria_find(heap, a) == &equal->criteria);
  ScoreObjective *oa = Scoreboard_addScoreObjective(board, a, &dummy->criteria);
  CHECK(oa && log->added == 1);
  CHECK(oa->name == a && oa->displayName == a && oa->theScoreboard == board &&
        oa->renderType == &ScoreRenderType_INTEGER);
  NativeReferenceList *snapshot =
      Scoreboard_getObjectivesFromCriteria(board, &dummy->criteria);
  CHECK(snapshot && NativeReferenceList_size(snapshot) == 1 &&
        NativeReferenceList_get(snapshot, 0) == (MCObject *)oa);
  ScoreObjective *ob = Scoreboard_addScoreObjective(board, b, &dummy->criteria);
  CHECK(ob && log->added == 2);
  CHECK(NativeReferenceList_size(snapshot) == 1);
  CHECK(NativeReferenceList_clear(snapshot));
  NativeReferenceList *later =
      Scoreboard_getObjectivesFromCriteria(board, &dummy->criteria);
  CHECK(NativeReferenceList_size(later) == 2 &&
        NativeReferenceList_get(later, 0) == (MCObject *)oa &&
        NativeReferenceList_get(later, 1) == (MCObject *)ob);
  CHECK(NativeReferenceList_size(Scoreboard_getObjectivesFromCriteria(
            board, &equal->criteria)) == 0);
  NativeHashMapView *view = Scoreboard_getScoreObjectives(board),
                    *names = Scoreboard_getObjectiveNames(board);
  CHECK(NativeHashMapView_size(view) == 2 &&
        NativeHashMapView_size(names) == 0);
  NativeHashMap *absent = Scoreboard_getObjectivesForEntity(board, p);
  CHECK(absent && NativeHashMap_put(absent, NULL, NULL));
  CHECK(NativeHashMapView_size(names) == 0);
  Score *s = Scoreboard_getValueFromObjective(board, p, oa);
  CHECK(s && s->forceUpdate && !s->locked && s->scorePoints == 0 &&
        s->theScoreObjective == oa);
  CHECK(Scoreboard_getValueFromObjective(
            board, NBTString_fromASCII(heap, "player"), oa) == s);
  CHECK(Score_setScorePoints(s, 0) && log->changed == 1 && !s->forceUpdate);
  CHECK(Score_setScorePoints(s, 0) && log->changed == 1);
  CHECK(Score_setScorePoints(s, INT32_MAX));
  CHECK(Score_increseScore(s, 1) && s->scorePoints == INT32_MIN);
  CHECK(Score_decreaseScore(s, 1) && s->scorePoints == INT32_MAX);
  CHECK(Score_setLocked(s, true) && Score_isLocked(s));
  CHECK(Scoreboard_entityHasObjective(board, p, oa));
  CHECK(Scoreboard_getObjectivesForEntity(board, p) != absent);
  CHECK(Scoreboard_getValueFromObjective(board, p, ob));
  CHECK(NativeReferenceList_size(Scoreboard_getScores(board)) == 2);
  CHECK(Scoreboard_removeObjectiveFromEntity(board, p, oa) && log->one == 1 &&
        log->all == 0);
  CHECK(Scoreboard_removeObjectiveFromEntity(board, p, ob) && log->all == 1 &&
        NativeHashMapView_size(names) == 0);
  CHECK(Scoreboard_getValueFromObjective(board, p, oa));
  CHECK(Scoreboard_setObjectiveInDisplaySlot(board, 18, oa));
  CHECK(ScoreObjective_setDisplayName(oa, NULL) && oa->displayName == NULL &&
        log->display == 1);
  CHECK(ScoreObjective_setRenderType(oa, &ScoreRenderType_HEARTS) &&
        log->display == 2);
  CHECK(Scoreboard_removeObjective(board, oa) && log->removed == 1);
  CHECK(!Scoreboard_getObjectiveInDisplaySlot(board, 18));
  CHECK(NativeHashMapView_size(view) == 1 &&
        NativeHashMapView_size(names) == 1);
  CHECK(NativeHashMap_size(Scoreboard_getObjectivesForEntity(board, p)) == 0);
  CHECK(NativeReferenceList_size(Scoreboard_getObjectivesFromCriteria(
            board, &dummy->criteria)) == 1);
  StatBase *stat = StatBase_newIdentity(heap, a, STAT_BASE_KIND_BASE);
  CHECK(stat && stat->statId == a);
  IScoreObjectiveCriteria *criteria = StatBase_getCriteria(stat);
  CHECK(criteria && criteria != (IScoreObjectiveCriteria *)stat &&
        ObjectiveStat_isInstance((MCObject *)criteria));
  CHECK(((ObjectiveStat *)criteria)->stat == stat &&
        IScoreObjectiveCriteria_getName(criteria) == a);
  CHECK(IScoreObjectiveCriteria_find(heap, a) == criteria);
  StatBase *withDeps =
      StatBase_newWithSimpleType(heap, b, (MCObject *)log, (MCObject *)log);
  CHECK(withDeps && withDeps->statName == (MCObject *)log &&
        withDeps->type == (MCObject *)log && !withDeps->isIndependent);
  CHECK(withDeps->objectiveCriteria &&
        ((ObjectiveStat *)withDeps->objectiveCriteria)->stat == withDeps);
  MCObjectRoot root = {0};
  CHECK(MCObjectRoot_init(&root, heap, (MCObject *)board));
  CHECK(MCObjectHeap_collect(heap));
  MCObjectHeap *clone = MCObjectHeap_clone(heap);
  CHECK(clone);
  MCObjectRoot copiedRoot = {0};
  CHECK(MCObjectRoot_rebind(&copiedRoot, clone, &root));
  Scoreboard *copy = (Scoreboard *)MCObjectRoot_get(&copiedRoot);
  CHECK(copy && copy != board &&
        copy->scoreObjectives != board->scoreObjectives);
  ScoreObjective *copyB =
      Scoreboard_getObjective(copy, NBTString_fromASCII(clone, "b"));
  CHECK(copyB && copyB != ob && copyB->theScoreboard == copy &&
        copyB->name != (NBTString *)b);
  ObjectiveStat *copyStat = (ObjectiveStat *)IScoreObjectiveCriteria_find(
      clone, NBTString_fromASCII(clone, "b"));
  CHECK(copyStat && copyStat->stat != withDeps &&
        copyStat->stat->objectiveCriteria == &copyStat->dummy.criteria);
  CHECK(copyStat->stat->statName == copy->context &&
        copyStat->stat->type == copy->context);
  CHECK(ScoreObjective_setDisplayName(copyB, NULL));
  CHECK(ob->displayName == b);
  CHECK(MCObjectHeap_adopt(heap, clone));
  MCObjectHeap_free(clone);
  board = (Scoreboard *)MCObjectRoot_get(&root);
  CHECK(board && Scoreboard_getObjective(
                     board, NBTString_fromASCII(heap, "b")) == copyB);
  CHECK(!MCObjectHeap_failed(heap));
  MCObjectRoot_drop(&root);
  MCObjectHeap_free(heap);
}
static void failures(void) {
  MCObjectHeap *h = MCObjectHeap_new(1u << 20);
  Scoreboard *b = Scoreboard_new(h);
  Log *l = (Log *)MCObjectHeap_alloc(h, sizeof(*l), &logClass);
  CHECK(Scoreboard_nativeBindOverrides(b, &hooks, (MCObject *)l));
  IScoreObjectiveCriteria *c =
      IScoreObjectiveCriteria_nativeNew(h, &methods, (MCObject *)l);
  ScoreObjective *o =
      Scoreboard_addScoreObjective(b, NBTString_fromASCII(h, "ro"), c);
  CHECK(o);
  Score *s =
      Scoreboard_getValueFromObjective(b, NBTString_fromASCII(h, "p"), o);
  CHECK(s);
  l->readOnly = true;
  CHECK(!Score_increseScore(s, 1) && s->scorePoints == 0 && s->forceUpdate &&
        l->readonlyCalls == 1 && MCObjectHeap_failed(h));
  MCObjectHeap_free(h);
  h = MCObjectHeap_new(1u << 20);
  b = Scoreboard_new(h);
  l = (Log *)MCObjectHeap_alloc(h, sizeof(*l), &logClass);
  c = IScoreObjectiveCriteria_nativeNew(h, &methods, (MCObject *)l);
  o = Scoreboard_addScoreObjective(b, NBTString_fromASCII(h, "x"), c);
  s = Scoreboard_getValueFromObjective(b, NBTString_fromASCII(h, "p"), o);
  l->mutate = true;
  CHECK(!Score_func_96648_a(s) && l->readonlyCalls == 2 &&
        s->scorePoints == 0 && MCObjectHeap_failed(h));
  MCObjectHeap_free(h);
  h = MCObjectHeap_new(1u << 20);
  b = Scoreboard_new(h);
  l = (Log *)MCObjectHeap_alloc(h, sizeof(*l), &logClass);
  CHECK(Scoreboard_nativeBindOverrides(b, &hooks, (MCObject *)l));
  ScoreDummyCriteria *d = ScoreDummyCriteria_new(h, NULL);
  o = Scoreboard_addScoreObjective(b, NBTString_fromASCII(h, "x"),
                                   &d->criteria);
  s = Scoreboard_getValueFromObjective(b, NBTString_fromASCII(h, "p"), o);
  l->fail = true;
  CHECK(!Score_setScorePoints(s, 9) && s->scorePoints == 9 && !s->forceUpdate &&
        l->changed == 1 && MCObjectHeap_failed(h));
  MCObjectHeap_free(h);
  h = MCObjectHeap_new(1u << 20);
  b = Scoreboard_new(h);
  d = ScoreDummyCriteria_new(h, NULL);
  NBTString *name = NBTString_fromASCII(h, "12345678901234567");
  CHECK(!Scoreboard_addScoreObjective(b, name, &d->criteria) &&
        MCObjectHeap_failed(h) &&
        NativeHashMap_isInstance((MCObject *)b->scoreObjectives));
  MCObjectHeap_free(h);
  h = MCObjectHeap_new(1u << 20);
  b = Scoreboard_new(h);
  CHECK(!Scoreboard_addScoreObjective(b, NBTString_fromASCII(h, "x"), NULL) &&
        MCObjectHeap_failed(h));
  MCObjectHeap_free(h);
  h = MCObjectHeap_new(1u << 20);
  b = Scoreboard_new(h);
  s = Scoreboard_getValueFromObjective(b, NBTString_fromASCII(h, "null"), NULL);
  CHECK(s && Score_setScorePoints(s, 1));
  CHECK(!Score_increseScore(s, 1) && s->scorePoints == 1 &&
        MCObjectHeap_failed(h));
  MCObjectHeap_free(h);
  h = MCObjectHeap_new(1u << 20);
  s = Score_new(h, NULL, NULL, NULL);
  CHECK(s && !Score_setScorePoints(s, 9) && s->scorePoints == 9 &&
        !s->forceUpdate && MCObjectHeap_failed(h));
  MCObjectHeap_free(h);
  h = MCObjectHeap_new(1u << 20);
  b = Scoreboard_new(h);
  d = ScoreDummyCriteria_new(h, NULL);
  o = Scoreboard_addScoreObjective(b, NBTString_fromASCII(h, "x"),
                                   &d->criteria);
  CHECK(o);
  CHECK(!Scoreboard_addScoreObjective(b, NBTString_fromASCII(h, "x"),
                                      &d->criteria) &&
        MCObjectHeap_failed(h));
  MCObjectHeap_free(h);
  h = MCObjectHeap_new(1u << 20);
  b = Scoreboard_new(h);
  l = (Log *)MCObjectHeap_alloc(h, sizeof(*l), &logClass);
  CHECK(Scoreboard_nativeBindOverrides(b, &hooks, (MCObject *)l));
  d = ScoreDummyCriteria_new(h, NULL);
  o = Scoreboard_addScoreObjective(b, NBTString_fromASCII(h, "x"),
                                   &d->criteria);
  l->fail = true;
  NBTString *replacement = NBTString_fromASCII(h, "changed");
  CHECK(!ScoreObjective_setDisplayName(o, replacement) &&
        o->displayName == replacement && l->display == 1 &&
        MCObjectHeap_failed(h));
  MCObjectHeap_free(h);
  h = MCObjectHeap_new(1u << 20);
  b = Scoreboard_new(h);
  ScoreboardOverrides missing = {0};
  CHECK(Scoreboard_nativeBindOverrides(b, &missing, NULL));
  d = ScoreDummyCriteria_new(h, NULL);
  CHECK(!Scoreboard_addScoreObjective(b, NBTString_fromASCII(h, "x"),
                                      &d->criteria) &&
        MCObjectHeap_failed(h));
  MCObjectHeap_free(h);
  h = MCObjectHeap_new(1u << 20);
  b = Scoreboard_new(h);
  CHECK(!Scoreboard_setObjectiveInDisplaySlot(b, 19, NULL) &&
        MCObjectHeap_failed(h));
  MCObjectHeap_free(h);
  h = MCObjectHeap_new(1u << 20);
  b = Scoreboard_new(h);
  MCObjectHeap *foreign = MCObjectHeap_new(1u << 20);
  d = ScoreDummyCriteria_new(foreign, NULL);
  CHECK(!Scoreboard_addScoreObjective(b, NBTString_fromASCII(h, "x"),
                                      &d->criteria) &&
        MCObjectHeap_failed(h) && !MCObjectHeap_failed(foreign));
  MCObjectHeap_free(h);
  MCObjectHeap_free(foreign);
  h = MCObjectHeap_new(1u << 20);
  b = Scoreboard_new(h);
  foreign = MCObjectHeap_new(1u << 20);
  d = ScoreDummyCriteria_new(h, NULL);
  o = Scoreboard_addScoreObjective(b, NBTString_fromASCII(h, "x"),
                                   &d->criteria);
  s = Scoreboard_getValueFromObjective(b, NBTString_fromASCII(h, "p"), o);
  s->theScoreboard = Scoreboard_new(foreign);
  CHECK(!Score_setScorePoints(s, 2) && s->scorePoints == 2 && !s->forceUpdate &&
        MCObjectHeap_failed(h) && !MCObjectHeap_failed(foreign));
  MCObjectHeap_free(h);
  MCObjectHeap_free(foreign);
  h = MCObjectHeap_new(1u << 20);
  b = Scoreboard_new(h);
  const MCObjectClass *arrayType = b->objectiveDisplaySlots->object.klass;
  b->objectiveDisplaySlots =
      (ScoreObjectiveArray *)MCObjectHeap_alloc(h, sizeof(MCObject), arrayType);
  MCObjectRoot root = {0};
  CHECK(MCObjectRoot_init(&root, h, (MCObject *)b));
  CHECK(!MCObjectHeap_collect(h) && MCObjectHeap_failed(h));
  MCObjectHeap_free(h);
}
int main(void) {
  normal();
  failures();
  printf("source scoreboard: %d checks\n", checks);
  return 0;
}
