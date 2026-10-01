#ifndef C919_SOURCE_SCORE_H
#define C919_SOURCE_SCORE_H
#include "scoreboard/ScoreObjective.h"
typedef struct Score {
  MCObject object;
  Scoreboard *theScoreboard;
  ScoreObjective *theScoreObjective;
  NBTString *scorePlayerName;
  int32_t scorePoints;
  bool locked, forceUpdate;
} Score;
bool Score_isInstance(const MCObject *);
Score *Score_new(MCObjectHeap *, Scoreboard *, ScoreObjective *, NBTString *);
bool Score_increseScore(Score *, int32_t);
bool Score_decreaseScore(Score *, int32_t);
bool Score_func_96648_a(Score *);
int32_t Score_getScorePoints(Score *);
bool Score_setScorePoints(Score *, int32_t);
ScoreObjective *Score_getObjective(Score *);
NBTString *Score_getPlayerName(Score *);
Scoreboard *Score_getScoreScoreboard(Score *);
bool Score_isLocked(Score *);
bool Score_setLocked(Score *, bool);
/* Health list aggregation/sorting comparator remain undeclared dependencies. */
#endif
