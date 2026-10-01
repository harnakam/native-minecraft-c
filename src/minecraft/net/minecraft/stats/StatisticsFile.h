#ifndef C919_SOURCE_STATISTICS_FILE_H
#define C919_SOURCE_STATISTICS_FILE_H
#include "stats/StatFileWriter.h"
typedef struct StatisticsFileStatSet StatisticsFileStatSet;
typedef struct StatisticsFileIntMap StatisticsFileIntMap;
typedef struct {
    bool (*isAnnouncingPlayerAchievements)(MCObject *context,MCObject *server);
    bool (*sendAchievementChat)(MCObject *context,MCObject *server,MCObject *player,StatBase *,bool taken);
    int32_t (*getTickCounter)(MCObject *context,MCObject *server);
    /* Actual S37/NetworkManager dependency. Retain the managed map/packet as
       an effect; no socket write before the enclosing durable graph commit. */
    bool (*sendStatistics)(MCObject *context,MCObject *player,StatisticsFileIntMap *);
} StatisticsFileDependencies;
typedef struct {
    StatFileWriter base;
    MCObject *mcServer,*dependencyContext;
    NBTString *statsFilePath;
    StatisticsFileStatSet *field_150888_e;
    int32_t field_150885_f;
    bool field_150886_g;
    const StatisticsFileDependencies *dependencies;
} StatisticsFile;
/* Native File/server constructor boundary; source field initialization and
   counter inheritance are real. The source JSON/FileUtils/Gson/progress and
   sendAchievements methods remain undeclared until their dependencies exist. */
StatisticsFile *StatisticsFile_nativeNew(MCObjectHeap *,MCObject *server,NBTString *path,
    MCObject *context,const StatisticsFileDependencies *);
bool StatisticsFile_isInstance(const MCObject *);
bool StatisticsFile_unlockAchievement(StatisticsFile *,MCObject *player,StatBase *,int32_t value);
StatisticsFileStatSet *StatisticsFile_func_150878_c(StatisticsFile *);
bool StatisticsFile_func_150877_d(StatisticsFile *);
bool StatisticsFile_func_150876_a(StatisticsFile *,MCObject *player);
bool StatisticsFile_func_150879_e(const StatisticsFile *);
/* Native managed HashSet/HashMap enumeration; equals retains the first key,
   and snapshot cloning remaps direct references. JDK iteration is pending. */
size_t StatisticsFileStatSet_size(const StatisticsFileStatSet *);
StatBase *StatisticsFileStatSet_get(StatisticsFileStatSet *,size_t index);
size_t StatisticsFileIntMap_size(const StatisticsFileIntMap *);
bool StatisticsFileIntMap_entry(StatisticsFileIntMap *,size_t index,StatBase **,int32_t *);
#endif
