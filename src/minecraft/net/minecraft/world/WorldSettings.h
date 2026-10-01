#ifndef C919_SOURCE_WORLD_SETTINGS_H
#define C919_SOURCE_WORLD_SETTINGS_H
#include "world/WorldSettingsGameType.h"
#include "world/WorldType.h"
typedef struct WorldInfo WorldInfo;
typedef struct WorldSettings {
    MCObject object;
    int64_t seed;
    const WorldSettingsGameType *theGameType;
    bool mapFeaturesEnabled,hardcoreEnabled;
    WorldType *terrainType;
    bool commandsAllowed,bonusChestEnabled;
    NBTString *worldName;
} WorldSettings;
bool WorldSettings_isInstance(const MCObject *);
WorldSettings *WorldSettings_nativeAllocate(MCObjectHeap *);
bool WorldSettings_construct(WorldSettings *,int64_t,const WorldSettingsGameType *,bool,bool,WorldType *);
bool WorldSettings_constructFromInfo(WorldSettings *,WorldInfo *);
WorldSettings *WorldSettings_new(MCObjectHeap *,int64_t,const WorldSettingsGameType *,bool,bool,WorldType *);
WorldSettings *WorldSettings_newFromInfo(MCObjectHeap *,WorldInfo *);
WorldSettings *WorldSettings_enableBonusChest(WorldSettings *);
WorldSettings *WorldSettings_enableCommands(WorldSettings *);
WorldSettings *WorldSettings_setWorldName(WorldSettings *,NBTString *);
bool WorldSettings_isBonusChestEnabled(WorldSettings *);
int64_t WorldSettings_getSeed(WorldSettings *);
const WorldSettingsGameType *WorldSettings_getGameType(WorldSettings *);
bool WorldSettings_getHardcoreEnabled(WorldSettings *);
bool WorldSettings_isMapFeaturesEnabled(WorldSettings *);
WorldType *WorldSettings_getTerrainType(WorldSettings *);
bool WorldSettings_areCommandsAllowed(WorldSettings *);
const WorldSettingsGameType *WorldSettings_getGameTypeById(int32_t);
NBTString *WorldSettings_getWorldName(WorldSettings *);
#endif
