#ifndef C919_SOURCE_SCORE_DUMMY_CRITERIA_H
#define C919_SOURCE_SCORE_DUMMY_CRITERIA_H
#include "scoreboard/IScoreObjectiveCriteria.h"
typedef struct ScoreDummyCriteria {
  IScoreObjectiveCriteria criteria;
  NBTString *dummyName;
} ScoreDummyCriteria;
bool ScoreDummyCriteria_isInstance(const MCObject *);
ScoreDummyCriteria *ScoreDummyCriteria_new(MCObjectHeap *, NBTString *);
bool ScoreDummyCriteria_construct(ScoreDummyCriteria *, NBTString *);
void ScoreDummyCriteria_traceFields(ScoreDummyCriteria *, MCObjectVisitor,
                                    void *);
#endif
