#ifndef C919_SOURCE_ENTITY_PLAYER_MP_STATS_H
#define C919_SOURCE_ENTITY_PLAYER_MP_STATS_H
#include "util/MCGameplayPlayer.h"
#include "stats/StatisticsFile.h"
/* Required actual Scoreboard/criteria/collection/Score dependency dispatch.
   Returned references belong to the actor heap. Iterator operations preserve
   the source foreach order and exceptions. There is no empty-score fallback. */
typedef struct {
    MCObject *(*getWorldScoreboard)(MCObject *context,MCGameplayPlayer *);
    MCObject *(*getCriteria)(MCObject *context,StatBase *);
    MCObject *(*getObjectivesFromCriteria)(MCObject *context,MCObject *scoreboard,MCObject *criteria);
    MCObject *(*iterator)(MCObject *context,MCObject *collection);
    bool (*hasNext)(MCObject *context,MCObject *iterator);
    MCObject *(*next)(MCObject *context,MCObject *iterator);
    NBTString *(*getName)(MCObject *context,MCGameplayPlayer *);
    MCObject *(*getValueFromObjective)(MCObject *context,MCObject *scoreboard,const NBTString *name,MCObject *objective);
    bool (*increseScore)(MCObject *context,MCObject *score,int32_t amount);
    bool (*setScorePoints)(MCObject *context,MCObject *score,int32_t points);
} EntityPlayerMPStatsDependencies;
bool EntityPlayerMP_addStat(MCGameplayPlayer *,StatBase *,int32_t amount,
    MCObject *context,const EntityPlayerMPStatsDependencies *);
bool EntityPlayerMP_func_175145_a(MCGameplayPlayer *,StatBase *,MCObject *context,
    const EntityPlayerMPStatsDependencies *);
/* Source null-stat no-op bypasses all dispatch. The actor's stats reference
   must be an actual StatisticsFile subclass. Source MP constructor, full
   Scoreboard classes and network transport remain separate dependencies. */
#endif
