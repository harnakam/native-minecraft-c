#ifndef C919_SOURCE_SCOREBOARD_H
#define C919_SOURCE_SCOREBOARD_H
#include "scoreboard/Score.h"
#include "util/NativeHashMap.h"
typedef struct {
  MCObject object;
  ScoreObjective *items[19];
} ScoreObjectiveArray;
typedef struct ScoreboardOverrides {
  bool (*onScoreObjectiveAdded)(MCObject *, Scoreboard *, ScoreObjective *);
  bool (*onObjectiveDisplayNameChanged)(MCObject *, Scoreboard *,
                                        ScoreObjective *);
  bool (*onScoreObjectiveRemoved)(MCObject *, Scoreboard *, ScoreObjective *);
  bool (*onScoreChanged)(MCObject *, Scoreboard *, Score *);
  bool (*onEntityScoresRemoved)(MCObject *, Scoreboard *, NBTString *);
  bool (*onEntityObjectiveRemoved)(MCObject *, Scoreboard *, NBTString *,
                                   ScoreObjective *);
} ScoreboardOverrides;
struct Scoreboard {
  MCObject object;
  NativeHashMap *scoreObjectives, *scoreObjectiveCriterias,
      *entitiesScoreObjectives;
  ScoreObjectiveArray *objectiveDisplaySlots;
  NativeHashMap *teams, *teamMemberships;
  const ScoreboardOverrides *overrides;
  MCObject *context;
};
Scoreboard *Scoreboard_new(MCObjectHeap *);
bool Scoreboard_isInstance(const MCObject *);
/* Native effect binding installs required real hooks. NULL overrides executes
   original empty base hooks; this does not implement ServerScoreboard effects.
 */
bool Scoreboard_nativeBindOverrides(Scoreboard *, const ScoreboardOverrides *,
                                    MCObject *);
ScoreObjective *Scoreboard_getObjective(Scoreboard *, NBTString *);
ScoreObjective *Scoreboard_addScoreObjective(Scoreboard *, NBTString *,
                                             IScoreObjectiveCriteria *);
NativeReferenceList *
Scoreboard_getObjectivesFromCriteria(Scoreboard *, IScoreObjectiveCriteria *);
bool Scoreboard_entityHasObjective(Scoreboard *, NBTString *, ScoreObjective *);
Score *Scoreboard_getValueFromObjective(Scoreboard *, NBTString *,
                                        ScoreObjective *);
NativeHashMapView *Scoreboard_getScoreObjectives(Scoreboard *);
NativeHashMapView *Scoreboard_getObjectiveNames(Scoreboard *);
NativeReferenceList *Scoreboard_getScores(Scoreboard *);
NativeHashMap *Scoreboard_getObjectivesForEntity(Scoreboard *, NBTString *);
bool Scoreboard_removeObjectiveFromEntity(Scoreboard *, NBTString *,
                                          ScoreObjective *);
bool Scoreboard_removeObjective(Scoreboard *, ScoreObjective *);
bool Scoreboard_setObjectiveInDisplaySlot(Scoreboard *, int32_t,
                                          ScoreObjective *);
ScoreObjective *Scoreboard_getObjectiveInDisplaySlot(Scoreboard *, int32_t);
bool Scoreboard_onScoreObjectiveAdded(Scoreboard *, ScoreObjective *);
bool Scoreboard_onObjectiveDisplayNameChanged(Scoreboard *, ScoreObjective *);
bool Scoreboard_onScoreObjectiveRemoved(Scoreboard *, ScoreObjective *);
bool Scoreboard_func_96536_a(Scoreboard *, Score *);
bool Scoreboard_func_96516_a(Scoreboard *, NBTString *);
bool Scoreboard_func_178820_a(Scoreboard *, NBTString *, ScoreObjective *);
/* Team bodies, static display-slot names, sorted scores, entity-death cleanup,
   ServerScoreboard overrides/init/save/network are not declared as completed.
 */
#endif
