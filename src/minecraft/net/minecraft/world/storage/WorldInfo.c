#include "world/storage/WorldInfo.h"
#include "nbt/NBTInternal.h"
static bool fail(MCObjectHeap *h){MCObjectHeap_fail(h);return false;}
void WorldInfo_traceFields(WorldInfo *p,MCObjectVisitor v,void *c) {
    p->terrainType=(WorldType *)v((MCObject *)p->terrainType,c);
    p->generatorOptions=(NBTString *)v((MCObject *)p->generatorOptions,c);
    p->playerTag=(NBTTagCompound *)v((MCObject *)p->playerTag,c);
    p->levelName=(NBTString *)v((MCObject *)p->levelName,c);
    p->difficulty=(EnumDifficulty *)v((MCObject *)p->difficulty,c);
    p->theGameRules=(GameRules *)v((MCObject *)p->theGameRules,c);
    p->virtualContext=(MCObject *)v((MCObject *)p->virtualContext,c);
}
static void trace(MCObject *o,MCObjectVisitor v,void *c){WorldInfo_traceFields((WorldInfo *)o,v,c);}
static const MCObjectClass klass={"net.minecraft.world.storage.WorldInfo",MCObjectHeap_plainClone,trace,NULL};
bool WorldInfo_isInstance(const MCObject *o){return o&&o->klass==&klass&&MCObjectHeap_objectSize(o)>=sizeof(WorldInfo);}
static bool valid(WorldInfo *p){return WorldInfo_isInstance((MCObject *)p)&&!MCObjectHeap_failed(p->object.heap)?true:fail(p?p->object.heap:NULL);}
static bool string_ref(MCObjectHeap *h,NBTString *s) {
    if(!s)return true;
    size_t size=MCObjectHeap_objectSize((MCObject *)s);
    return s->object.heap==h&&NBTString_isInstance((MCObject *)s)&&size>=sizeof *s&&s->length<=(size-sizeof *s)/sizeof *s->units?true:fail(h);
}
static bool type_ref(MCObjectHeap *h,WorldType *p){return !p||(WorldType_isInstance((MCObject *)p)&&p->object.heap==h)?true:fail(h);}
static bool difficulty_ref(MCObjectHeap *h,EnumDifficulty *p){return !p||(EnumDifficulty_isInstance((MCObject *)p)&&p->object.heap==h)?true:fail(h);}
static bool rules_ref(MCObjectHeap *h,GameRules *p){return !p||(GameRules_isInstance((MCObject *)p)&&p->object.heap==h)?true:fail(h);}
static bool game_type_ref(MCObjectHeap *h,const WorldSettingsGameType *p){return !p||WorldSettingsGameType_isCanonical(p)?true:fail(h);}
static bool compound_ref(MCObjectHeap *h,NBTTagCompound *p){return !p||(NBTTagCompound_isInstance((MCObject *)p)&&((MCObject *)p)->heap==h)?true:fail(h);}
static bool begin(WorldInfo *p,MCObjectRootScope *scope) {
    if(!valid(p))return false;
    MCObjectHeap *h=p->object.heap;
    if((p->virtualContext&&p->virtualContext->heap!=h)||!MCObjectRootScope_begin(scope,h)||
       !MCObjectRootScope_pin(scope,(MCObject *)p)||!MCObjectRootScope_pin(scope,p->virtualContext)){MCObjectRootScope_end(scope);return fail(h);}
    return true;
}
EnumDifficulty *WorldInfo_defaultDifficulty(MCObjectHeap *h){EnumDifficultyStatics *s=EnumDifficulty_getStatics(h);return s?s->NORMAL:NULL;}
WorldInfo *WorldInfo_nativeAllocate(MCObjectHeap *h,const WorldInfoVirtualMethods *v,MCObject *c) {
    if((c&&c->heap!=h)||!WorldInfo_defaultDifficulty(h)){fail(h);return NULL;}
    WorldInfo *p=(WorldInfo *)MCObjectHeap_alloc(h,sizeof *p,&klass);
    if(p){p->virtualMethods=v;p->virtualContext=c;}return p;
}
static bool initialize(WorldInfo *p) {
    MCObjectHeap *h=p->object.heap;WorldTypeStatics *types=WorldType_getStatics(h);
    if(!types)return false;
    p->terrainType=types->DEFAULT;MCObjectHeap_touch(h);
    NBTString *empty=NBTString_literalASCII(h,"");if(!empty)return false;
    p->generatorOptions=empty;MCObjectHeap_touch(h);
    p->borderCenterX=0.0;p->borderCenterZ=0.0;p->borderSize=6.0E7;p->borderSizeLerpTime=0;
    p->borderSizeLerpTarget=0.0;p->borderSafeZone=5.0;p->borderDamagePerBlock=0.2;
    p->borderWarningDistance=5;p->borderWarningTime=15;MCObjectHeap_touch(h);
    GameRules *rules=GameRules_new(h);if(!rules)return false;
    p->theGameRules=rules;MCObjectHeap_touch(h);return true;
}
bool WorldInfo_construct(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    bool ok=initialize(p);MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_constructSettings(WorldInfo *p,WorldSettings *settings,NBTString *name) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;bool ok=false;
    if(!initialize(p)||!WorldInfo_populateFromWorldSettings(p,settings)||!string_ref(h,name))goto done;
    p->levelName=name;MCObjectHeap_touch(h);
    EnumDifficulty *difficulty=WorldInfo_defaultDifficulty(h);if(!difficulty)goto done;
    p->difficulty=difficulty;p->initialized=false;MCObjectHeap_touch(h);ok=true;
done:
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(h);
}
bool WorldInfo_populateFromWorldSettings_base(WorldInfo *p,WorldSettings *s) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;bool ok=false;
    if(!WorldSettings_isInstance((MCObject *)s)||s->object.heap!=h||!MCObjectRootScope_pin(&scope,(MCObject *)s)){fail(h);goto done;}
    int64_t value0=WorldSettings_getSeed(s);if(MCObjectHeap_failed(h)||!(true))goto done;
    p->randomSeed=value0;MCObjectHeap_touch(h);
    const WorldSettingsGameType * value1=WorldSettings_getGameType(s);if(MCObjectHeap_failed(h)||!(game_type_ref(h,value1)))goto done;
    p->theGameType=value1;MCObjectHeap_touch(h);
    bool value2=WorldSettings_isMapFeaturesEnabled(s);if(MCObjectHeap_failed(h)||!(true))goto done;
    p->mapFeaturesEnabled=value2;MCObjectHeap_touch(h);
    bool value3=WorldSettings_getHardcoreEnabled(s);if(MCObjectHeap_failed(h)||!(true))goto done;
    p->hardcore=value3;MCObjectHeap_touch(h);
    WorldType * value4=WorldSettings_getTerrainType(s);if(MCObjectHeap_failed(h)||!(type_ref(h,value4)))goto done;
    p->terrainType=value4;MCObjectHeap_touch(h);
    NBTString * value5=WorldSettings_getWorldName(s);if(MCObjectHeap_failed(h)||!(string_ref(h,value5)))goto done;
    p->generatorOptions=value5;MCObjectHeap_touch(h);
    bool value6=WorldSettings_areCommandsAllowed(s);if(MCObjectHeap_failed(h)||!(true))goto done;
    p->allowCommands=value6;MCObjectHeap_touch(h);
    ok=true;
done:
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_populateFromWorldSettings(WorldInfo *p,WorldSettings *s) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    if(s&&(!WorldSettings_isInstance((MCObject *)s)||s->object.heap!=h)){MCObjectRootScope_end(&scope);return fail(h);}
    bool ok=v&&v->populateFromWorldSettings?v->populateFromWorldSettings(c,p,s):WorldInfo_populateFromWorldSettings_base(p,s);
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_constructCopy(WorldInfo *p,WorldInfo *source) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;bool ok=false;
    if(!initialize(p))goto done;
    if(!WorldInfo_isInstance((MCObject *)source)||source->object.heap!=h||!MCObjectRootScope_pin(&scope,(MCObject *)source)){fail(h);goto done;}
    if(!(true))goto done;
    p->randomSeed=source->randomSeed;MCObjectHeap_touch(h);
    if(!(type_ref(h,source->terrainType)))goto done;
    p->terrainType=source->terrainType;MCObjectHeap_touch(h);
    if(!(string_ref(h,source->generatorOptions)))goto done;
    p->generatorOptions=source->generatorOptions;MCObjectHeap_touch(h);
    if(!(game_type_ref(h,source->theGameType)))goto done;
    p->theGameType=source->theGameType;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->mapFeaturesEnabled=source->mapFeaturesEnabled;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->spawnX=source->spawnX;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->spawnY=source->spawnY;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->spawnZ=source->spawnZ;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->totalTime=source->totalTime;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->worldTime=source->worldTime;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->lastTimePlayed=source->lastTimePlayed;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->sizeOnDisk=source->sizeOnDisk;MCObjectHeap_touch(h);
    if(!(compound_ref(h,source->playerTag)))goto done;
    p->playerTag=source->playerTag;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->dimension=source->dimension;MCObjectHeap_touch(h);
    if(!(string_ref(h,source->levelName)))goto done;
    p->levelName=source->levelName;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->saveVersion=source->saveVersion;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->rainTime=source->rainTime;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->raining=source->raining;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->thunderTime=source->thunderTime;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->thundering=source->thundering;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->hardcore=source->hardcore;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->allowCommands=source->allowCommands;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->initialized=source->initialized;MCObjectHeap_touch(h);
    if(!(rules_ref(h,source->theGameRules)))goto done;
    p->theGameRules=source->theGameRules;MCObjectHeap_touch(h);
    if(!(difficulty_ref(h,source->difficulty)))goto done;
    p->difficulty=source->difficulty;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->difficultyLocked=source->difficultyLocked;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->borderCenterX=source->borderCenterX;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->borderCenterZ=source->borderCenterZ;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->borderSize=source->borderSize;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->borderSizeLerpTime=source->borderSizeLerpTime;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->borderSizeLerpTarget=source->borderSizeLerpTarget;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->borderSafeZone=source->borderSafeZone;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->borderDamagePerBlock=source->borderDamagePerBlock;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->borderWarningTime=source->borderWarningTime;MCObjectHeap_touch(h);
    if(!(true))goto done;
    p->borderWarningDistance=source->borderWarningDistance;MCObjectHeap_touch(h);
    ok=true;
done:
    MCObjectRootScope_end(&scope);return ok;
}
WorldInfo *WorldInfo_new(MCObjectHeap *h,WorldSettings *s,NBTString *name) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    WorldInfo *p=WorldInfo_nativeAllocate(h,NULL,NULL);if(p&&!WorldInfo_constructSettings(p,s,name))p=NULL;
    MCObjectRootScope_end(&scope);return p;
}
WorldInfo *WorldInfo_newCopy(MCObjectHeap *h,WorldInfo *s) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    WorldInfo *p=WorldInfo_nativeAllocate(h,NULL,NULL);if(p&&!WorldInfo_constructCopy(p,s))p=NULL;
    MCObjectRootScope_end(&scope);return p;
}
int64_t WorldInfo_getSeed(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (int64_t)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    int64_t value=v&&v->getSeed?v->getSeed(c,p):p->randomSeed;
    if(MCObjectHeap_failed(h)||!(true))value=(int64_t)0;
    MCObjectRootScope_end(&scope);return value;
}
int32_t WorldInfo_getSpawnX(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (int32_t)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    int32_t value=v&&v->getSpawnX?v->getSpawnX(c,p):p->spawnX;
    if(MCObjectHeap_failed(h)||!(true))value=(int32_t)0;
    MCObjectRootScope_end(&scope);return value;
}
int32_t WorldInfo_getSpawnY(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (int32_t)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    int32_t value=v&&v->getSpawnY?v->getSpawnY(c,p):p->spawnY;
    if(MCObjectHeap_failed(h)||!(true))value=(int32_t)0;
    MCObjectRootScope_end(&scope);return value;
}
int32_t WorldInfo_getSpawnZ(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (int32_t)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    int32_t value=v&&v->getSpawnZ?v->getSpawnZ(c,p):p->spawnZ;
    if(MCObjectHeap_failed(h)||!(true))value=(int32_t)0;
    MCObjectRootScope_end(&scope);return value;
}
int64_t WorldInfo_getWorldTotalTime(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (int64_t)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    int64_t value=v&&v->getWorldTotalTime?v->getWorldTotalTime(c,p):p->totalTime;
    if(MCObjectHeap_failed(h)||!(true))value=(int64_t)0;
    MCObjectRootScope_end(&scope);return value;
}
int64_t WorldInfo_getWorldTime(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (int64_t)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    int64_t value=v&&v->getWorldTime?v->getWorldTime(c,p):p->worldTime;
    if(MCObjectHeap_failed(h)||!(true))value=(int64_t)0;
    MCObjectRootScope_end(&scope);return value;
}
int64_t WorldInfo_getSizeOnDisk(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (int64_t)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    int64_t value=v&&v->getSizeOnDisk?v->getSizeOnDisk(c,p):p->sizeOnDisk;
    if(MCObjectHeap_failed(h)||!(true))value=(int64_t)0;
    MCObjectRootScope_end(&scope);return value;
}
NBTTagCompound * WorldInfo_getPlayerNBTTagCompound(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (NBTTagCompound *)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    NBTTagCompound * value=v&&v->getPlayerNBTTagCompound?v->getPlayerNBTTagCompound(c,p):p->playerTag;
    if(MCObjectHeap_failed(h)||!(compound_ref(h,value)))value=(NBTTagCompound *)0;
    MCObjectRootScope_end(&scope);return value;
}
NBTString * WorldInfo_getWorldName(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (NBTString *)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    NBTString * value=v&&v->getWorldName?v->getWorldName(c,p):p->levelName;
    if(MCObjectHeap_failed(h)||!(string_ref(h,value)))value=(NBTString *)0;
    MCObjectRootScope_end(&scope);return value;
}
int32_t WorldInfo_getSaveVersion(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (int32_t)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    int32_t value=v&&v->getSaveVersion?v->getSaveVersion(c,p):p->saveVersion;
    if(MCObjectHeap_failed(h)||!(true))value=(int32_t)0;
    MCObjectRootScope_end(&scope);return value;
}
int64_t WorldInfo_getLastTimePlayed(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (int64_t)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    int64_t value=v&&v->getLastTimePlayed?v->getLastTimePlayed(c,p):p->lastTimePlayed;
    if(MCObjectHeap_failed(h)||!(true))value=(int64_t)0;
    MCObjectRootScope_end(&scope);return value;
}
int32_t WorldInfo_getCleanWeatherTime(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (int32_t)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    int32_t value=v&&v->getCleanWeatherTime?v->getCleanWeatherTime(c,p):p->cleanWeatherTime;
    if(MCObjectHeap_failed(h)||!(true))value=(int32_t)0;
    MCObjectRootScope_end(&scope);return value;
}
bool WorldInfo_isThundering(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (bool)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool value=v&&v->isThundering?v->isThundering(c,p):p->thundering;
    if(MCObjectHeap_failed(h)||!(true))value=(bool)0;
    MCObjectRootScope_end(&scope);return value;
}
int32_t WorldInfo_getThunderTime(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (int32_t)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    int32_t value=v&&v->getThunderTime?v->getThunderTime(c,p):p->thunderTime;
    if(MCObjectHeap_failed(h)||!(true))value=(int32_t)0;
    MCObjectRootScope_end(&scope);return value;
}
bool WorldInfo_isRaining(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (bool)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool value=v&&v->isRaining?v->isRaining(c,p):p->raining;
    if(MCObjectHeap_failed(h)||!(true))value=(bool)0;
    MCObjectRootScope_end(&scope);return value;
}
int32_t WorldInfo_getRainTime(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (int32_t)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    int32_t value=v&&v->getRainTime?v->getRainTime(c,p):p->rainTime;
    if(MCObjectHeap_failed(h)||!(true))value=(int32_t)0;
    MCObjectRootScope_end(&scope);return value;
}
const WorldSettingsGameType * WorldInfo_getGameType(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (const WorldSettingsGameType *)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    const WorldSettingsGameType * value=v&&v->getGameType?v->getGameType(c,p):p->theGameType;
    if(MCObjectHeap_failed(h)||!(game_type_ref(h,value)))value=(const WorldSettingsGameType *)0;
    MCObjectRootScope_end(&scope);return value;
}
bool WorldInfo_isMapFeaturesEnabled(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (bool)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool value=v&&v->isMapFeaturesEnabled?v->isMapFeaturesEnabled(c,p):p->mapFeaturesEnabled;
    if(MCObjectHeap_failed(h)||!(true))value=(bool)0;
    MCObjectRootScope_end(&scope);return value;
}
bool WorldInfo_isHardcoreModeEnabled(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (bool)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool value=v&&v->isHardcoreModeEnabled?v->isHardcoreModeEnabled(c,p):p->hardcore;
    if(MCObjectHeap_failed(h)||!(true))value=(bool)0;
    MCObjectRootScope_end(&scope);return value;
}
WorldType * WorldInfo_getTerrainType(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (WorldType *)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    WorldType * value=v&&v->getTerrainType?v->getTerrainType(c,p):p->terrainType;
    if(MCObjectHeap_failed(h)||!(type_ref(h,value)))value=(WorldType *)0;
    MCObjectRootScope_end(&scope);return value;
}
NBTString * WorldInfo_getGeneratorOptions(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (NBTString *)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    NBTString * value=v&&v->getGeneratorOptions?v->getGeneratorOptions(c,p):p->generatorOptions;
    if(MCObjectHeap_failed(h)||!(string_ref(h,value)))value=(NBTString *)0;
    MCObjectRootScope_end(&scope);return value;
}
bool WorldInfo_areCommandsAllowed(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (bool)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool value=v&&v->areCommandsAllowed?v->areCommandsAllowed(c,p):p->allowCommands;
    if(MCObjectHeap_failed(h)||!(true))value=(bool)0;
    MCObjectRootScope_end(&scope);return value;
}
bool WorldInfo_isInitialized(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (bool)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool value=v&&v->isInitialized?v->isInitialized(c,p):p->initialized;
    if(MCObjectHeap_failed(h)||!(true))value=(bool)0;
    MCObjectRootScope_end(&scope);return value;
}
GameRules * WorldInfo_getGameRulesInstance(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (GameRules *)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    GameRules * value=v&&v->getGameRulesInstance?v->getGameRulesInstance(c,p):p->theGameRules;
    if(MCObjectHeap_failed(h)||!(rules_ref(h,value)))value=(GameRules *)0;
    MCObjectRootScope_end(&scope);return value;
}
double WorldInfo_getBorderCenterX(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (double)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    double value=v&&v->getBorderCenterX?v->getBorderCenterX(c,p):p->borderCenterX;
    if(MCObjectHeap_failed(h)||!(true))value=(double)0;
    MCObjectRootScope_end(&scope);return value;
}
double WorldInfo_getBorderCenterZ(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (double)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    double value=v&&v->getBorderCenterZ?v->getBorderCenterZ(c,p):p->borderCenterZ;
    if(MCObjectHeap_failed(h)||!(true))value=(double)0;
    MCObjectRootScope_end(&scope);return value;
}
double WorldInfo_getBorderSize(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (double)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    double value=v&&v->getBorderSize?v->getBorderSize(c,p):p->borderSize;
    if(MCObjectHeap_failed(h)||!(true))value=(double)0;
    MCObjectRootScope_end(&scope);return value;
}
int64_t WorldInfo_getBorderLerpTime(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (int64_t)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    int64_t value=v&&v->getBorderLerpTime?v->getBorderLerpTime(c,p):p->borderSizeLerpTime;
    if(MCObjectHeap_failed(h)||!(true))value=(int64_t)0;
    MCObjectRootScope_end(&scope);return value;
}
double WorldInfo_getBorderLerpTarget(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (double)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    double value=v&&v->getBorderLerpTarget?v->getBorderLerpTarget(c,p):p->borderSizeLerpTarget;
    if(MCObjectHeap_failed(h)||!(true))value=(double)0;
    MCObjectRootScope_end(&scope);return value;
}
double WorldInfo_getBorderSafeZone(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (double)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    double value=v&&v->getBorderSafeZone?v->getBorderSafeZone(c,p):p->borderSafeZone;
    if(MCObjectHeap_failed(h)||!(true))value=(double)0;
    MCObjectRootScope_end(&scope);return value;
}
double WorldInfo_getBorderDamagePerBlock(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (double)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    double value=v&&v->getBorderDamagePerBlock?v->getBorderDamagePerBlock(c,p):p->borderDamagePerBlock;
    if(MCObjectHeap_failed(h)||!(true))value=(double)0;
    MCObjectRootScope_end(&scope);return value;
}
int32_t WorldInfo_getBorderWarningDistance(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (int32_t)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    int32_t value=v&&v->getBorderWarningDistance?v->getBorderWarningDistance(c,p):p->borderWarningDistance;
    if(MCObjectHeap_failed(h)||!(true))value=(int32_t)0;
    MCObjectRootScope_end(&scope);return value;
}
int32_t WorldInfo_getBorderWarningTime(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (int32_t)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    int32_t value=v&&v->getBorderWarningTime?v->getBorderWarningTime(c,p):p->borderWarningTime;
    if(MCObjectHeap_failed(h)||!(true))value=(int32_t)0;
    MCObjectRootScope_end(&scope);return value;
}
EnumDifficulty * WorldInfo_getDifficulty(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (EnumDifficulty *)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    EnumDifficulty * value=v&&v->getDifficulty?v->getDifficulty(c,p):p->difficulty;
    if(MCObjectHeap_failed(h)||!(difficulty_ref(h,value)))value=(EnumDifficulty *)0;
    MCObjectRootScope_end(&scope);return value;
}
bool WorldInfo_isDifficultyLocked(WorldInfo *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return (bool)0;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool value=v&&v->isDifficultyLocked?v->isDifficultyLocked(c,p):p->difficultyLocked;
    if(MCObjectHeap_failed(h)||!(true))value=(bool)0;
    MCObjectRootScope_end(&scope);return value;
}
bool WorldInfo_setSpawnX(WorldInfo *p,int32_t value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setSpawnX)ok=v->setSpawnX(c,p,value);else {p->spawnX=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setSpawnY(WorldInfo *p,int32_t value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setSpawnY)ok=v->setSpawnY(c,p,value);else {p->spawnY=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setSpawnZ(WorldInfo *p,int32_t value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setSpawnZ)ok=v->setSpawnZ(c,p,value);else {p->spawnZ=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setWorldTotalTime(WorldInfo *p,int64_t value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setWorldTotalTime)ok=v->setWorldTotalTime(c,p,value);else {p->totalTime=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setWorldTime(WorldInfo *p,int64_t value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setWorldTime)ok=v->setWorldTime(c,p,value);else {p->worldTime=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setWorldName(WorldInfo *p,NBTString * value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=string_ref(h,value);
    if(ok) {if(v&&v->setWorldName)ok=v->setWorldName(c,p,value);else {p->levelName=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setSaveVersion(WorldInfo *p,int32_t value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setSaveVersion)ok=v->setSaveVersion(c,p,value);else {p->saveVersion=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setCleanWeatherTime(WorldInfo *p,int32_t value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setCleanWeatherTime)ok=v->setCleanWeatherTime(c,p,value);else {p->cleanWeatherTime=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setThundering(WorldInfo *p,bool value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setThundering)ok=v->setThundering(c,p,value);else {p->thundering=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setThunderTime(WorldInfo *p,int32_t value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setThunderTime)ok=v->setThunderTime(c,p,value);else {p->thunderTime=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setRaining(WorldInfo *p,bool value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setRaining)ok=v->setRaining(c,p,value);else {p->raining=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setRainTime(WorldInfo *p,int32_t value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setRainTime)ok=v->setRainTime(c,p,value);else {p->rainTime=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setGameType(WorldInfo *p,const WorldSettingsGameType * value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=game_type_ref(h,value);
    if(ok) {if(v&&v->setGameType)ok=v->setGameType(c,p,value);else {p->theGameType=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setMapFeaturesEnabled(WorldInfo *p,bool value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setMapFeaturesEnabled)ok=v->setMapFeaturesEnabled(c,p,value);else {p->mapFeaturesEnabled=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setHardcore(WorldInfo *p,bool value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setHardcore)ok=v->setHardcore(c,p,value);else {p->hardcore=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setTerrainType(WorldInfo *p,WorldType * value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=type_ref(h,value);
    if(ok) {if(v&&v->setTerrainType)ok=v->setTerrainType(c,p,value);else {p->terrainType=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setAllowCommands(WorldInfo *p,bool value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setAllowCommands)ok=v->setAllowCommands(c,p,value);else {p->allowCommands=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setServerInitialized(WorldInfo *p,bool value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setServerInitialized)ok=v->setServerInitialized(c,p,value);else {p->initialized=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setBorderSize(WorldInfo *p,double value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setBorderSize)ok=v->setBorderSize(c,p,value);else {p->borderSize=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setBorderLerpTime(WorldInfo *p,int64_t value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setBorderLerpTime)ok=v->setBorderLerpTime(c,p,value);else {p->borderSizeLerpTime=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setBorderLerpTarget(WorldInfo *p,double value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setBorderLerpTarget)ok=v->setBorderLerpTarget(c,p,value);else {p->borderSizeLerpTarget=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_getBorderCenterX_double(WorldInfo *p,double value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->getBorderCenterX_double)ok=v->getBorderCenterX_double(c,p,value);else {p->borderCenterX=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_getBorderCenterZ_double(WorldInfo *p,double value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->getBorderCenterZ_double)ok=v->getBorderCenterZ_double(c,p,value);else {p->borderCenterZ=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setBorderSafeZone(WorldInfo *p,double value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setBorderSafeZone)ok=v->setBorderSafeZone(c,p,value);else {p->borderSafeZone=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setBorderDamagePerBlock(WorldInfo *p,double value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setBorderDamagePerBlock)ok=v->setBorderDamagePerBlock(c,p,value);else {p->borderDamagePerBlock=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setBorderWarningDistance(WorldInfo *p,int32_t value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setBorderWarningDistance)ok=v->setBorderWarningDistance(c,p,value);else {p->borderWarningDistance=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setBorderWarningTime(WorldInfo *p,int32_t value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setBorderWarningTime)ok=v->setBorderWarningTime(c,p,value);else {p->borderWarningTime=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setDifficulty(WorldInfo *p,EnumDifficulty * value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=difficulty_ref(h,value);
    if(ok) {if(v&&v->setDifficulty)ok=v->setDifficulty(c,p,value);else {p->difficulty=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setDifficultyLocked(WorldInfo *p,bool value) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    bool ok=true;
    if(ok) {if(v&&v->setDifficultyLocked)ok=v->setDifficultyLocked(c,p,value);else {p->difficultyLocked=value;MCObjectHeap_touch(h);}}
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool WorldInfo_setSpawn(WorldInfo *p,BlockPos *position) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *h=p->object.heap;bool ok=false;
    const WorldInfoVirtualMethods *v=p->virtualMethods;MCObject *c=p->virtualContext;
    if(v&&v->setSpawn)ok=v->setSpawn(c,p,position);
    else if(BlockPos_isInstance((MCObject *)position)&&((MCObject *)position)->heap==h) {
        p->spawnX=position->x;MCObjectHeap_touch(h);p->spawnY=position->y;MCObjectHeap_touch(h);p->spawnZ=position->z;MCObjectHeap_touch(h);ok=true;
    }
    if(!ok||MCObjectHeap_failed(h))ok=fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
