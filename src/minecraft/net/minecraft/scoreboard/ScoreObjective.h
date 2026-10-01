#ifndef C919_SOURCE_SCORE_OBJECTIVE_H
#define C919_SOURCE_SCORE_OBJECTIVE_H
#include "scoreboard/IScoreObjectiveCriteria.h"
typedef struct Scoreboard Scoreboard;
typedef struct ScoreObjective {
  MCObject object;
  Scoreboard *theScoreboard;
  NBTString *name;
  IScoreObjectiveCriteria *objectiveCriteria;
  const ScoreRenderType *renderType;
  NBTString *displayName;
} ScoreObjective;
bool ScoreObjective_isInstance(const MCObject *);
ScoreObjective *ScoreObjective_new(MCObjectHeap *, Scoreboard *, NBTString *,
                                   IScoreObjectiveCriteria *);
Scoreboard *ScoreObjective_getScoreboard(ScoreObjective *);
NBTString *ScoreObjective_getName(ScoreObjective *);
IScoreObjectiveCriteria *ScoreObjective_getCriteria(ScoreObjective *);
NBTString *ScoreObjective_getDisplayName(ScoreObjective *);
bool ScoreObjective_setDisplayName(ScoreObjective *, NBTString *);
const ScoreRenderType *ScoreObjective_getRenderType(ScoreObjective *);
bool ScoreObjective_setRenderType(ScoreObjective *, const ScoreRenderType *);
#endif
