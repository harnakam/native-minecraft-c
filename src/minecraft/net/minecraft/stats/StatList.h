#ifndef C919_SOURCE_STAT_LIST_H
#define C919_SOURCE_STAT_LIST_H
#include "stats/StatCrafting.h"
#include "stats/Achievement.h"
#include "item/crafting/CraftingManager.h"
#include "item/crafting/FurnaceRecipes.h"
#define STATLIST_CRAFT_STATS_COUNT 32000u
typedef struct StatListList StatListList;
struct StatList {
    MCObject object;
    StatBaseRegistry *oneShotStats;
    StatListList *allStats,*generalStats,*objectMineStats;
    StatBase *objectCraftStats[STATLIST_CRAFT_STATS_COUNT];
    StatBase *dropStat;
};
/* Native per-heap ownership of original static fields. Static general/mining/
   use/break/entity registration, display, and scoreboard classes are separate
   undeclared dependencies. This creates empty fields, not a full StatList.init. */
StatList *StatList_new(MCObjectHeap *);
StatBase *StatList_registerStat(StatList *,StatBase *);
StatBase *StatList_getOneShotStat(StatList *,const NBTString *);
StatBase *StatList_getOneShotStat_ascii(StatList *,const char *);
bool StatList_initCraftableStats(StatList *,CraftingManager *,FurnaceRecipes *);
bool StatList_replaceAllSimilarBlocks(StatList *,StatBase **array,size_t count);
bool StatList_mergeStatBases(StatList *,StatBase **array,size_t count,int32_t firstBlock,int32_t secondBlock);
/* Exact same-heap reference attachment, no generic stat for absent IDs. */
bool StatList_fillCraftStats(StatList *,StatBase **output,size_t count);
int32_t StatListList_size(const StatListList *);
StatBase *StatListList_get(StatListList *,int32_t index);
bool StatListList_add(StatListList *,StatBase *);
bool StatListList_remove(StatListList *,StatBase *);
/* Optional native AchievementList identity registration: source IDs/parents/
   special/independent facts and dropStat. Original display/icon/layout and
   ObjectiveStat constructor behavior remain explicit dependencies. */
bool StatList_initAchievementIdentities(StatList *);
#endif
