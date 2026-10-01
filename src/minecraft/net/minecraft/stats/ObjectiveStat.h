#ifndef C919_SOURCE_OBJECTIVE_STAT_H
#define C919_SOURCE_OBJECTIVE_STAT_H
#include "scoreboard/ScoreDummyCriteria.h"
typedef struct StatBase StatBase;
typedef struct ObjectiveStat {
  ScoreDummyCriteria dummy;
  StatBase *stat;
} ObjectiveStat;
bool ObjectiveStat_isInstance(const MCObject *);
ObjectiveStat *ObjectiveStat_new(MCObjectHeap *, StatBase *);
#endif
