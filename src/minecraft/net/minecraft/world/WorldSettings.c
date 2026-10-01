#include "world/WorldSettings.h"
#include "world/storage/WorldInfo.h"
static bool fail(MCObjectHeap *h){MCObjectHeap_fail(h);return false;}
static void trace(MCObject *o,MCObjectVisitor v,void *c) {
    WorldSettings *s=(WorldSettings *)o;s->terrainType=(WorldType *)v((MCObject *)s->terrainType,c);
    s->worldName=(NBTString *)v((MCObject *)s->worldName,c);
}
static const MCObjectClass klass={"net.minecraft.world.WorldSettings",MCObjectHeap_plainClone,trace,NULL};
bool WorldSettings_isInstance(const MCObject *o){return o&&o->klass==&klass&&MCObjectHeap_objectSize(o)>=sizeof(WorldSettings);}
static bool valid(WorldSettings *s){return WorldSettings_isInstance((MCObject *)s)&&!MCObjectHeap_failed(s->object.heap)?true:fail(s?s->object.heap:NULL);}
WorldSettings *WorldSettings_nativeAllocate(MCObjectHeap *h){return (WorldSettings *)MCObjectHeap_alloc(h,sizeof(WorldSettings),&klass);}
bool WorldSettings_construct(WorldSettings *s,int64_t seed,const WorldSettingsGameType *type,bool map,bool hardcore,WorldType *terrain) {
    if(!valid(s))return false;
    MCObjectHeap *h=s->object.heap;MCObjectRootScope scope={0};
    if(!MCObjectRootScope_begin(&scope,h)||!MCObjectRootScope_pin(&scope,(MCObject *)s)){MCObjectRootScope_end(&scope);return fail(h);}
    NBTString *empty=NBTString_literalASCII(h,"");bool ok=false;
    if(!empty)goto done;
    s->worldName=empty;MCObjectHeap_touch(h);
    s->seed=seed;MCObjectHeap_touch(h);
    if(type&&!WorldSettingsGameType_isCanonical(type)){fail(h);goto done;}
    s->theGameType=type;s->mapFeaturesEnabled=map;s->hardcoreEnabled=hardcore;MCObjectHeap_touch(h);
    if(terrain&&(!WorldType_isInstance((MCObject *)terrain)||terrain->object.heap!=h)){fail(h);goto done;}
    s->terrainType=terrain;MCObjectHeap_touch(h);ok=true;
done:
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldSettings_constructFromInfo(WorldSettings *s,WorldInfo *info) {
    if(!valid(s)||!WorldInfo_isInstance((MCObject *)info)||info->object.heap!=s->object.heap)return fail(s?s->object.heap:NULL);
    MCObjectHeap *h=s->object.heap;MCObjectRootScope scope={0};
    if(!MCObjectRootScope_begin(&scope,h)||!MCObjectRootScope_pin(&scope,(MCObject *)s)||!MCObjectRootScope_pin(&scope,(MCObject *)info)){MCObjectRootScope_end(&scope);return fail(h);}
    bool ok=false;int64_t seed=WorldInfo_getSeed(info);if(MCObjectHeap_failed(h))goto done;
    const WorldSettingsGameType *type=WorldInfo_getGameType(info);if(MCObjectHeap_failed(h))goto done;
    bool map=WorldInfo_isMapFeaturesEnabled(info);if(MCObjectHeap_failed(h))goto done;
    bool hardcore=WorldInfo_isHardcoreModeEnabled(info);if(MCObjectHeap_failed(h))goto done;
    WorldType *terrain=WorldInfo_getTerrainType(info);if(MCObjectHeap_failed(h))goto done;
    ok=WorldSettings_construct(s,seed,type,map,hardcore,terrain);
done:
    MCObjectRootScope_end(&scope);return ok;
}
WorldSettings *WorldSettings_new(MCObjectHeap *h,int64_t seed,const WorldSettingsGameType *type,bool map,bool hardcore,WorldType *terrain) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    WorldSettings *s=WorldSettings_nativeAllocate(h);if(s&&!WorldSettings_construct(s,seed,type,map,hardcore,terrain))s=NULL;
    MCObjectRootScope_end(&scope);return s;
}
WorldSettings *WorldSettings_newFromInfo(MCObjectHeap *h,WorldInfo *info) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    WorldSettings *s=WorldSettings_nativeAllocate(h);if(s&&!WorldSettings_constructFromInfo(s,info))s=NULL;
    MCObjectRootScope_end(&scope);return s;
}
WorldSettings *WorldSettings_enableBonusChest(WorldSettings *s){if(!valid(s))return NULL;s->bonusChestEnabled=true;MCObjectHeap_touch(s->object.heap);return s;}
WorldSettings *WorldSettings_enableCommands(WorldSettings *s){if(!valid(s))return NULL;s->commandsAllowed=true;MCObjectHeap_touch(s->object.heap);return s;}
WorldSettings *WorldSettings_setWorldName(WorldSettings *s,NBTString *name) {
    if(!valid(s)||(name&&(!NBTString_isInstance((MCObject *)name)||((MCObject *)name)->heap!=s->object.heap))){fail(s?s->object.heap:NULL);return NULL;}
    s->worldName=name;MCObjectHeap_touch(s->object.heap);return s;
}
bool WorldSettings_isBonusChestEnabled(WorldSettings *s){return valid(s)&&s->bonusChestEnabled;}
int64_t WorldSettings_getSeed(WorldSettings *s){return valid(s)?s->seed:0;}
const WorldSettingsGameType *WorldSettings_getGameType(WorldSettings *s){return valid(s)?s->theGameType:NULL;}
bool WorldSettings_getHardcoreEnabled(WorldSettings *s){return valid(s)&&s->hardcoreEnabled;}
bool WorldSettings_isMapFeaturesEnabled(WorldSettings *s){return valid(s)&&s->mapFeaturesEnabled;}
WorldType *WorldSettings_getTerrainType(WorldSettings *s){return valid(s)?s->terrainType:NULL;}
bool WorldSettings_areCommandsAllowed(WorldSettings *s){return valid(s)&&s->commandsAllowed;}
const WorldSettingsGameType *WorldSettings_getGameTypeById(int32_t id){return WorldSettingsGameType_getByID(id);}
NBTString *WorldSettings_getWorldName(WorldSettings *s){return valid(s)?s->worldName:NULL;}
