#ifndef C919_STAT_FILE_WRITER_H
#define C919_STAT_FILE_WRITER_H
#include "stats/Achievement.h"
typedef struct StatFileWriter StatFileWriter;
typedef struct StatFileWriterOverrides {
    bool (*unlockAchievement)(StatFileWriter *,MCObject *player,StatBase *,int32_t value);
} StatFileWriterOverrides;
typedef struct StatMap StatMap;
struct StatFileWriter {MCObject object;StatMap *statsData;const StatFileWriterOverrides *overrides;};
/* Shared original base fields for StatisticsFile inheritance. Allocation must
   already have installed the actual subclass descriptor. */
bool StatFileWriter_construct(StatFileWriter *,const StatFileWriterOverrides *);
void StatFileWriter_trace(MCObject *,MCObjectVisitor,void *);
/* Native Map.clear/putAll dependencies retain Map identity and share the
   original Tuple objects; putAll replaces values without replacing equal keys. */
bool StatFileWriter_clear(StatFileWriter *);
bool StatFileWriter_putAll(StatFileWriter *destination,const StatFileWriter *source);
StatFileWriter *StatFileWriter_new(MCObjectHeap *);
/* Explicit dispatch dependency for a later StatisticsFile subclass; omitted
   override uses the actual counter implementation, not an empty callback. */
StatFileWriter *StatFileWriter_newWithOverrides(MCObjectHeap *,const StatFileWriterOverrides *);
int32_t StatFileWriter_readStat(const StatFileWriter *,const StatBase *);
bool StatFileWriter_increaseStat(StatFileWriter *,MCObject *player,StatBase *,int32_t amount);
bool StatFileWriter_unlockAchievement(StatFileWriter *,MCObject *player,StatBase *,int32_t value);
bool StatFileWriter_unlockAchievementBase(StatFileWriter *,MCObject *player,StatBase *,int32_t value);
bool StatFileWriter_hasAchievementUnlocked(const StatFileWriter *,const Achievement *);
bool StatFileWriter_canUnlockAchievement(const StatFileWriter *,const Achievement *);
int32_t StatFileWriter_func_150874_c(const StatFileWriter *,const Achievement *);
MCObject *StatFileWriter_func_150870_b(const StatFileWriter *,const StatBase *);
MCObject *StatFileWriter_func_150872_a(StatFileWriter *,StatBase *,MCObject *progress);
/* Native borrowed enumeration for future save/network adapters. Ordering is
   not Java ConcurrentHashMap iteration order. Call under a RootScope. */
size_t StatFileWriter_statCount(const StatFileWriter *);
bool StatFileWriter_entryAt(const StatFileWriter *,size_t index,StatBase **,
    int32_t *value,MCObject **progress);
/* This base ports the source counter/progress methods. StatisticsFile dirty
   fields and EntityPlayerMP stat/score dispatch are separate class slices;
   JSON/progress serialization, full Scoreboard/S37 transport and Java
   ConcurrentHashMap concurrency remain separate dependencies.
   Native exception/cycle boundaries fail the heap. Keys/progress belong to
   the same managed heap; progress getters retain the original direct ref. */
#endif
