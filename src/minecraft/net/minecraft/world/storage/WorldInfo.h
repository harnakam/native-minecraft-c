#ifndef C919_SOURCE_WORLD_INFO_H
#define C919_SOURCE_WORLD_INFO_H
#include "world/WorldSettings.h"
#include "world/EnumDifficulty.h"
#include "world/GameRules.h"
#include "util/BlockPos.h"
#define WORLD_INFO_GETTERS(X) \
    X(int64_t,getSeed,randomSeed) \
    X(int32_t,getSpawnX,spawnX) \
    X(int32_t,getSpawnY,spawnY) \
    X(int32_t,getSpawnZ,spawnZ) \
    X(int64_t,getWorldTotalTime,totalTime) \
    X(int64_t,getWorldTime,worldTime) \
    X(int64_t,getSizeOnDisk,sizeOnDisk) \
    X(NBTTagCompound *,getPlayerNBTTagCompound,playerTag) \
    X(NBTString *,getWorldName,levelName) \
    X(int32_t,getSaveVersion,saveVersion) \
    X(int64_t,getLastTimePlayed,lastTimePlayed) \
    X(int32_t,getCleanWeatherTime,cleanWeatherTime) \
    X(bool,isThundering,thundering) \
    X(int32_t,getThunderTime,thunderTime) \
    X(bool,isRaining,raining) \
    X(int32_t,getRainTime,rainTime) \
    X(const WorldSettingsGameType *,getGameType,theGameType) \
    X(bool,isMapFeaturesEnabled,mapFeaturesEnabled) \
    X(bool,isHardcoreModeEnabled,hardcore) \
    X(WorldType *,getTerrainType,terrainType) \
    X(NBTString *,getGeneratorOptions,generatorOptions) \
    X(bool,areCommandsAllowed,allowCommands) \
    X(bool,isInitialized,initialized) \
    X(GameRules *,getGameRulesInstance,theGameRules) \
    X(double,getBorderCenterX,borderCenterX) \
    X(double,getBorderCenterZ,borderCenterZ) \
    X(double,getBorderSize,borderSize) \
    X(int64_t,getBorderLerpTime,borderSizeLerpTime) \
    X(double,getBorderLerpTarget,borderSizeLerpTarget) \
    X(double,getBorderSafeZone,borderSafeZone) \
    X(double,getBorderDamagePerBlock,borderDamagePerBlock) \
    X(int32_t,getBorderWarningDistance,borderWarningDistance) \
    X(int32_t,getBorderWarningTime,borderWarningTime) \
    X(EnumDifficulty *,getDifficulty,difficulty) \
    X(bool,isDifficultyLocked,difficultyLocked)
#define WORLD_INFO_SETTERS(X) \
    X(int32_t,setSpawnX,spawnX) \
    X(int32_t,setSpawnY,spawnY) \
    X(int32_t,setSpawnZ,spawnZ) \
    X(int64_t,setWorldTotalTime,totalTime) \
    X(int64_t,setWorldTime,worldTime) \
    X(NBTString *,setWorldName,levelName) \
    X(int32_t,setSaveVersion,saveVersion) \
    X(int32_t,setCleanWeatherTime,cleanWeatherTime) \
    X(bool,setThundering,thundering) \
    X(int32_t,setThunderTime,thunderTime) \
    X(bool,setRaining,raining) \
    X(int32_t,setRainTime,rainTime) \
    X(const WorldSettingsGameType *,setGameType,theGameType) \
    X(bool,setMapFeaturesEnabled,mapFeaturesEnabled) \
    X(bool,setHardcore,hardcore) \
    X(WorldType *,setTerrainType,terrainType) \
    X(bool,setAllowCommands,allowCommands) \
    X(bool,setServerInitialized,initialized) \
    X(double,setBorderSize,borderSize) \
    X(int64_t,setBorderLerpTime,borderSizeLerpTime) \
    X(double,setBorderLerpTarget,borderSizeLerpTarget) \
    X(double,getBorderCenterX_double,borderCenterX) \
    X(double,getBorderCenterZ_double,borderCenterZ) \
    X(double,setBorderSafeZone,borderSafeZone) \
    X(double,setBorderDamagePerBlock,borderDamagePerBlock) \
    X(int32_t,setBorderWarningDistance,borderWarningDistance) \
    X(int32_t,setBorderWarningTime,borderWarningTime) \
    X(EnumDifficulty *,setDifficulty,difficulty) \
    X(bool,setDifficultyLocked,difficultyLocked)
typedef struct WorldInfoVirtualMethods {
    bool (*populateFromWorldSettings)(MCObject *context,WorldInfo *,WorldSettings *);
#define C919_INFO_GETTER_CALLBACK(type,name,field) type (*name)(MCObject *context,WorldInfo *);
    WORLD_INFO_GETTERS(C919_INFO_GETTER_CALLBACK)
#undef C919_INFO_GETTER_CALLBACK
#define C919_INFO_SETTER_CALLBACK(type,name,field) bool (*name)(MCObject *context,WorldInfo *,type);
    WORLD_INFO_SETTERS(C919_INFO_SETTER_CALLBACK)
#undef C919_INFO_SETTER_CALLBACK
    bool (*setSpawn)(MCObject *context,WorldInfo *,BlockPos *);
} WorldInfoVirtualMethods;
struct WorldInfo {
    MCObject object;
    int64_t randomSeed;
    WorldType *terrainType;
    NBTString *generatorOptions;
    int32_t spawnX,spawnY,spawnZ;
    int64_t totalTime,worldTime,lastTimePlayed,sizeOnDisk;
    NBTTagCompound *playerTag;
    int32_t dimension;
    NBTString *levelName;
    int32_t saveVersion,cleanWeatherTime;
    bool raining;
    int32_t rainTime;
    bool thundering;
    int32_t thunderTime;
    const WorldSettingsGameType *theGameType;
    bool mapFeaturesEnabled,hardcore,allowCommands,initialized;
    EnumDifficulty *difficulty;
    bool difficultyLocked;
    double borderCenterX,borderCenterZ,borderSize;
    int64_t borderSizeLerpTime;
    double borderSizeLerpTarget,borderSafeZone,borderDamagePerBlock;
    int32_t borderWarningDistance,borderWarningTime;
    GameRules *theGameRules;
    /* Native virtual-dispatch/lifetime boundary. Missing table entries inherit
       the actual base body. Context is managed; method table is immutable.
       Generic Java subclasses/DerivedWorldInfo are not claimed by allocation. */
    const WorldInfoVirtualMethods *virtualMethods;
    MCObject *virtualContext;
};
bool WorldInfo_isInstance(const MCObject *);
WorldInfo *WorldInfo_nativeAllocate(MCObjectHeap *,const WorldInfoVirtualMethods *,MCObject *context);
void WorldInfo_traceFields(WorldInfo *,MCObjectVisitor,void *);
bool WorldInfo_construct(WorldInfo *); /* protected noarg body + initializers */
bool WorldInfo_constructSettings(WorldInfo *,WorldSettings *,NBTString *name);
bool WorldInfo_constructCopy(WorldInfo *,WorldInfo *);
WorldInfo *WorldInfo_new(MCObjectHeap *,WorldSettings *,NBTString *name);
WorldInfo *WorldInfo_newCopy(MCObjectHeap *,WorldInfo *);
bool WorldInfo_populateFromWorldSettings(WorldInfo *,WorldSettings *);
/* Named base body is used for an actual inherited virtual invocation. */
bool WorldInfo_populateFromWorldSettings_base(WorldInfo *,WorldSettings *);
EnumDifficulty *WorldInfo_defaultDifficulty(MCObjectHeap *);
#define C919_INFO_GETTER_DECLARE(type,name,field) type WorldInfo_##name(WorldInfo *);
WORLD_INFO_GETTERS(C919_INFO_GETTER_DECLARE)
#undef C919_INFO_GETTER_DECLARE
#define C919_INFO_SETTER_DECLARE(type,name,field) bool WorldInfo_##name(WorldInfo *,type);
WORLD_INFO_SETTERS(C919_INFO_SETTER_DECLARE)
#undef C919_INFO_SETTER_DECLARE
bool WorldInfo_setSpawn(WorldInfo *,BlockPos *);
/* Original overloaded getBorderCenterX/Z(double) setters have _double suffix.
   NBT constructors/output/crash-report/Derived class methods are not stubs. */
#endif
