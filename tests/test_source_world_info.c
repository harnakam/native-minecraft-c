#include "world/storage/WorldInfo.h"
#include "util/NativeJavaNumber.h"
#include "util/NativeJavaString.h"
#include "nbt/NBTInternal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static unsigned checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"Source WorldInfo %u line%d: %s\n",checks,__LINE__,#x);exit(1);}}while(0)
static MCObjectHeap *heap(void){MCObjectHeap *h=MCObjectHeap_new(1024u*1024u);CHECK(h);return h;}
static NBTString *s(MCObjectHeap *h,const char *text){NBTString *a=NBTString_fromASCII(h,text);CHECK(a);return a;}
static uint64_t dbits(double value){uint64_t n;memcpy(&n,&value,8);return n;}
typedef struct {MCObject object;WorldSettings *settings;WorldInfo *info;int calls,failAt;char events[16];bool failedPopulate;} Fixture;
static void fixture_trace(MCObject *o,MCObjectVisitor v,void *ctx){Fixture *f=(Fixture *)o;f->settings=(WorldSettings *)v((MCObject *)f->settings,ctx);f->info=(WorldInfo *)v((MCObject *)f->info,ctx);}
static const MCObjectClass fixtureClass={"test.WorldInfo.Dispatch",MCObjectHeap_plainClone,fixture_trace,NULL};
static Fixture *fixture(MCObjectHeap *h){Fixture *f=(Fixture *)MCObjectHeap_alloc(h,sizeof *f,&fixtureClass);CHECK(f);return f;}
static bool event(Fixture *f,char c){CHECK(MCObjectHeap_hasBorrowers(f->object.heap));CHECK(!MCObjectHeap_collect(f->object.heap));f->events[f->calls++]=c;if(f->calls==f->failAt){MCObjectHeap_fail(f->object.heap);return false;}return true;}
static int64_t getSeed(MCObject *ctx,WorldInfo *p){Fixture *f=(Fixture *)ctx;CHECK(p==f->info);return event(f,'s')?p->randomSeed+1:0;}
static const WorldSettingsGameType *getType(MCObject *ctx,WorldInfo *p){Fixture *f=(Fixture *)ctx;CHECK(p==f->info);event(f,'g');return &WorldSettingsGameType_ADVENTURE;}
static bool getMap(MCObject *ctx,WorldInfo *p){Fixture *f=(Fixture *)ctx;CHECK(p==f->info);event(f,'m');return true;}
static bool getHard(MCObject *ctx,WorldInfo *p){Fixture *f=(Fixture *)ctx;CHECK(p==f->info);event(f,'h');return true;}
static WorldType *getTerrain(MCObject *ctx,WorldInfo *p){Fixture *f=(Fixture *)ctx;CHECK(p==f->info);return event(f,'t')?WorldType_getStatics(p->object.heap)->AMPLIFIED:NULL;}
static bool populate(MCObject *ctx,WorldInfo *p,WorldSettings *settings){Fixture *f=(Fixture *)ctx;CHECK(settings==f->settings);CHECK(p==f->info);CHECK(p->theGameRules&&p->difficulty==NULL&&p->levelName==NULL);CHECK(p->borderSize==6e7);p->randomSeed=77;p->initialized=true;f->calls++;if(f->failedPopulate){MCObjectHeap_fail(p->object.heap);return false;}return true;}
static const WorldInfoVirtualMethods getters={.getSeed=getSeed,.getGameType=getType,.isMapFeaturesEnabled=getMap,.isHardcoreModeEnabled=getHard,.getTerrainType=getTerrain};
static const WorldInfoVirtualMethods populater={.populateFromWorldSettings=populate};
static void defaults_and_refs(void) {
    MCObjectHeap *h=heap();WorldTypeStatics *types=WorldType_getStatics(h);EnumDifficultyStatics *difficulty=EnumDifficulty_getStatics(h);CHECK(types&&difficulty);
    const int ids[]={0,1,2,3,4,5,8};WorldType *t[]={types->DEFAULT,types->FLAT,types->LARGE_BIOMES,types->AMPLIFIED,types->CUSTOMIZED,types->DEBUG_WORLD,types->DEFAULT_1_1};
    const char *names[]={"default","flat","largeBiomes","amplified","customized","debug_all_block_states","default_1_1"};
    for(size_t i=0;i<7;i++){CHECK(types->worldTypes->items[ids[i]]==t[i]);CHECK(WorldType_getWorldTypeID(t[i])==ids[i]);CHECK(NBTString_equalsASCII(WorldType_getWorldTypeName(t[i]),names[i]));CHECK(WorldType_getGeneratorVersion(t[i])==(i?0:1));CHECK(WorldType_getCanBeCreated(t[i])==(i!=6));CHECK(WorldType_isVersioned(t[i])==(i==0));CHECK(WorldType_showWorldInfoNotice(t[i])==(i==3));}
    CHECK(WorldType_getWorldTypeForGeneratorVersion(types->DEFAULT,0)==types->DEFAULT_1_1);CHECK(WorldType_getWorldTypeForGeneratorVersion(types->FLAT,0)==types->FLAT);
    CHECK(NBTString_equalsASCII(WorldType_getTranslateName(types->FLAT),"generator.flat"));CHECK(NBTString_equalsASCII(WorldType_getTranslatedInfo(types->FLAT),"generator.flat.info"));
    CHECK(WorldType_parseWorldType(h,s(h,"LARGEBIOMES"))==types->LARGE_BIOMES);CHECK(WorldType_parseWorldType(h,NULL)==NULL&&!MCObjectHeap_failed(h));
    uint16_t dotless[]={'l','a','r','g','e','B',0x0131,'o','m','e','s'};CHECK(WorldType_parseWorldType(h,NBTString_fromUTF16(h,dotless,11))==types->LARGE_BIOMES);
    WorldType *replacement=WorldType_nativeAllocate(h);CHECK(replacement&&WorldType_construct(replacement,0,s(h,"flat"),17));CHECK(types->DEFAULT!=replacement&&types->worldTypes->items[0]==replacement);CHECK(WorldType_parseWorldType(h,s(h,"flat"))==replacement&&WorldType_getWorldTypeForGeneratorVersion(replacement,0)==replacement);types->worldTypes->items[0]=types->DEFAULT;MCObjectHeap_touch(h);
    CHECK(EnumDifficulty_getDifficultyEnum(h,-4)==difficulty->PEACEFUL);CHECK(EnumDifficulty_getDifficultyEnum(h,7)==difficulty->HARD);CHECK(EnumDifficulty_getDifficultyId(difficulty->NORMAL)==2);CHECK(NBTString_equalsASCII(EnumDifficulty_getDifficultyResourceKey(difficulty->NORMAL),"options.difficulty.normal"));
    WorldInfo *info=WorldInfo_nativeAllocate(h,NULL,NULL);CHECK(info&&WorldInfo_construct(info));
    CHECK(info->terrainType==types->DEFAULT&&NBTString_equalsASCII(info->generatorOptions,""));CHECK(info->difficulty==NULL&&info->levelName==NULL&&info->theGameType==NULL);CHECK(info->borderSize==6e7&&info->borderSafeZone==5.0&&info->borderDamagePerBlock==0.2&&info->borderWarningDistance==5&&info->borderWarningTime==15);CHECK(info->theGameRules&&info->theGameRules->theGameRules->size==15);
    NBTString *name=s(h,"World");GameRules *rules=info->theGameRules;NBTTagCompound *tag=NBTTagCompound_new(h);CHECK(tag);
    info->randomSeed=INT64_C(-123456789012345);
    info->spawnX=-1234567;
    info->spawnY=-1234567;
    info->spawnZ=-1234567;
    info->totalTime=INT64_C(-123456789012345);
    info->worldTime=INT64_C(-123456789012345);
    info->sizeOnDisk=INT64_C(-123456789012345);
    info->playerTag=tag;
    info->levelName=name;
    info->saveVersion=-1234567;
    info->lastTimePlayed=INT64_C(-123456789012345);
    info->cleanWeatherTime=-1234567;
    info->thundering=true;
    info->thunderTime=-1234567;
    info->raining=true;
    info->rainTime=-1234567;
    info->theGameType=&WorldSettingsGameType_CREATIVE;
    info->mapFeaturesEnabled=true;
    info->hardcore=true;
    info->terrainType=types->FLAT;
    info->generatorOptions=name;
    info->allowCommands=true;
    info->initialized=true;
    info->theGameRules=rules;
    info->borderCenterX=-12345.625;
    info->borderCenterZ=-12345.625;
    info->borderSize=-12345.625;
    info->borderSizeLerpTime=INT64_C(-123456789012345);
    info->borderSizeLerpTarget=-12345.625;
    info->borderSafeZone=-12345.625;
    info->borderDamagePerBlock=-12345.625;
    info->borderWarningDistance=-1234567;
    info->borderWarningTime=-1234567;
    info->difficulty=difficulty->HARD;
    info->difficultyLocked=true;
    CHECK(WorldInfo_getSeed(info)==info->randomSeed);
    CHECK(WorldInfo_getSpawnX(info)==info->spawnX);
    CHECK(WorldInfo_getSpawnY(info)==info->spawnY);
    CHECK(WorldInfo_getSpawnZ(info)==info->spawnZ);
    CHECK(WorldInfo_getWorldTotalTime(info)==info->totalTime);
    CHECK(WorldInfo_getWorldTime(info)==info->worldTime);
    CHECK(WorldInfo_getSizeOnDisk(info)==info->sizeOnDisk);
    CHECK(WorldInfo_getPlayerNBTTagCompound(info)==info->playerTag);
    CHECK(WorldInfo_getWorldName(info)==info->levelName);
    CHECK(WorldInfo_getSaveVersion(info)==info->saveVersion);
    CHECK(WorldInfo_getLastTimePlayed(info)==info->lastTimePlayed);
    CHECK(WorldInfo_getCleanWeatherTime(info)==info->cleanWeatherTime);
    CHECK(WorldInfo_isThundering(info)==info->thundering);
    CHECK(WorldInfo_getThunderTime(info)==info->thunderTime);
    CHECK(WorldInfo_isRaining(info)==info->raining);
    CHECK(WorldInfo_getRainTime(info)==info->rainTime);
    CHECK(WorldInfo_getGameType(info)==info->theGameType);
    CHECK(WorldInfo_isMapFeaturesEnabled(info)==info->mapFeaturesEnabled);
    CHECK(WorldInfo_isHardcoreModeEnabled(info)==info->hardcore);
    CHECK(WorldInfo_getTerrainType(info)==info->terrainType);
    CHECK(WorldInfo_getGeneratorOptions(info)==info->generatorOptions);
    CHECK(WorldInfo_areCommandsAllowed(info)==info->allowCommands);
    CHECK(WorldInfo_isInitialized(info)==info->initialized);
    CHECK(WorldInfo_getGameRulesInstance(info)==info->theGameRules);
    CHECK(WorldInfo_getBorderCenterX(info)==info->borderCenterX);
    CHECK(WorldInfo_getBorderCenterZ(info)==info->borderCenterZ);
    CHECK(WorldInfo_getBorderSize(info)==info->borderSize);
    CHECK(WorldInfo_getBorderLerpTime(info)==info->borderSizeLerpTime);
    CHECK(WorldInfo_getBorderLerpTarget(info)==info->borderSizeLerpTarget);
    CHECK(WorldInfo_getBorderSafeZone(info)==info->borderSafeZone);
    CHECK(WorldInfo_getBorderDamagePerBlock(info)==info->borderDamagePerBlock);
    CHECK(WorldInfo_getBorderWarningDistance(info)==info->borderWarningDistance);
    CHECK(WorldInfo_getBorderWarningTime(info)==info->borderWarningTime);
    CHECK(WorldInfo_getDifficulty(info)==info->difficulty);
    CHECK(WorldInfo_isDifficultyLocked(info)==info->difficultyLocked);
    CHECK(WorldInfo_setSpawnX(info,-1234567));CHECK(info->spawnX==-1234567);
    CHECK(WorldInfo_setSpawnY(info,-1234567));CHECK(info->spawnY==-1234567);
    CHECK(WorldInfo_setSpawnZ(info,-1234567));CHECK(info->spawnZ==-1234567);
    CHECK(WorldInfo_setWorldTotalTime(info,INT64_C(-123456789012345)));CHECK(info->totalTime==INT64_C(-123456789012345));
    CHECK(WorldInfo_setWorldTime(info,INT64_C(-123456789012345)));CHECK(info->worldTime==INT64_C(-123456789012345));
    CHECK(WorldInfo_setWorldName(info,name));CHECK(info->levelName==name);
    CHECK(WorldInfo_setSaveVersion(info,-1234567));CHECK(info->saveVersion==-1234567);
    CHECK(WorldInfo_setCleanWeatherTime(info,-1234567));CHECK(info->cleanWeatherTime==-1234567);
    CHECK(WorldInfo_setThundering(info,true));CHECK(info->thundering==true);
    CHECK(WorldInfo_setThunderTime(info,-1234567));CHECK(info->thunderTime==-1234567);
    CHECK(WorldInfo_setRaining(info,true));CHECK(info->raining==true);
    CHECK(WorldInfo_setRainTime(info,-1234567));CHECK(info->rainTime==-1234567);
    CHECK(WorldInfo_setGameType(info,&WorldSettingsGameType_CREATIVE));CHECK(info->theGameType==&WorldSettingsGameType_CREATIVE);
    CHECK(WorldInfo_setMapFeaturesEnabled(info,true));CHECK(info->mapFeaturesEnabled==true);
    CHECK(WorldInfo_setHardcore(info,true));CHECK(info->hardcore==true);
    CHECK(WorldInfo_setTerrainType(info,types->FLAT));CHECK(info->terrainType==types->FLAT);
    CHECK(WorldInfo_setAllowCommands(info,true));CHECK(info->allowCommands==true);
    CHECK(WorldInfo_setServerInitialized(info,true));CHECK(info->initialized==true);
    CHECK(WorldInfo_setBorderSize(info,-12345.625));CHECK(info->borderSize==-12345.625);
    CHECK(WorldInfo_setBorderLerpTime(info,INT64_C(-123456789012345)));CHECK(info->borderSizeLerpTime==INT64_C(-123456789012345));
    CHECK(WorldInfo_setBorderLerpTarget(info,-12345.625));CHECK(info->borderSizeLerpTarget==-12345.625);
    CHECK(WorldInfo_getBorderCenterX_double(info,-12345.625));CHECK(info->borderCenterX==-12345.625);
    CHECK(WorldInfo_getBorderCenterZ_double(info,-12345.625));CHECK(info->borderCenterZ==-12345.625);
    CHECK(WorldInfo_setBorderSafeZone(info,-12345.625));CHECK(info->borderSafeZone==-12345.625);
    CHECK(WorldInfo_setBorderDamagePerBlock(info,-12345.625));CHECK(info->borderDamagePerBlock==-12345.625);
    CHECK(WorldInfo_setBorderWarningDistance(info,-1234567));CHECK(info->borderWarningDistance==-1234567);
    CHECK(WorldInfo_setBorderWarningTime(info,-1234567));CHECK(info->borderWarningTime==-1234567);
    CHECK(WorldInfo_setDifficulty(info,difficulty->HARD));CHECK(info->difficulty==difficulty->HARD);
    CHECK(WorldInfo_setDifficultyLocked(info,true));CHECK(info->difficultyLocked==true);
    BlockPos *pos=DataWatcher_blockPos(h,INT32_MIN,INT32_MAX,-1);CHECK(pos&&WorldInfo_setSpawn(info,pos));CHECK(info->spawnX==INT32_MIN&&info->spawnY==INT32_MAX&&info->spawnZ==-1);
    CHECK(WorldInfo_setBorderSafeZone(info,-0.0)&&dbits(WorldInfo_getBorderSafeZone(info))==UINT64_C(0x8000000000000000));
    info->dimension=-77;info->cleanWeatherTime=919;
    WorldInfo *copy=WorldInfo_nativeAllocate(h,NULL,NULL);CHECK(copy);copy->cleanWeatherTime=331;size_t before=MCObjectHeap_liveObjects(h);CHECK(WorldInfo_constructCopy(copy,info));CHECK(MCObjectHeap_liveObjects(h)>before+30);CHECK(copy->cleanWeatherTime==331&&copy->dimension==info->dimension);
    CHECK(copy->randomSeed==info->randomSeed);
    CHECK(copy->spawnX==info->spawnX);
    CHECK(copy->spawnY==info->spawnY);
    CHECK(copy->spawnZ==info->spawnZ);
    CHECK(copy->totalTime==info->totalTime);
    CHECK(copy->worldTime==info->worldTime);
    CHECK(copy->sizeOnDisk==info->sizeOnDisk);
    CHECK(copy->playerTag==info->playerTag);
    CHECK(copy->levelName==info->levelName);
    CHECK(copy->saveVersion==info->saveVersion);
    CHECK(copy->lastTimePlayed==info->lastTimePlayed);
    CHECK(copy->thundering==info->thundering);
    CHECK(copy->thunderTime==info->thunderTime);
    CHECK(copy->raining==info->raining);
    CHECK(copy->rainTime==info->rainTime);
    CHECK(copy->theGameType==info->theGameType);
    CHECK(copy->mapFeaturesEnabled==info->mapFeaturesEnabled);
    CHECK(copy->hardcore==info->hardcore);
    CHECK(copy->terrainType==info->terrainType);
    CHECK(copy->generatorOptions==info->generatorOptions);
    CHECK(copy->allowCommands==info->allowCommands);
    CHECK(copy->initialized==info->initialized);
    CHECK(copy->theGameRules==info->theGameRules);
    CHECK(copy->borderCenterX==info->borderCenterX);
    CHECK(copy->borderCenterZ==info->borderCenterZ);
    CHECK(copy->borderSize==info->borderSize);
    CHECK(copy->borderSizeLerpTime==info->borderSizeLerpTime);
    CHECK(copy->borderSizeLerpTarget==info->borderSizeLerpTarget);
    CHECK(copy->borderSafeZone==info->borderSafeZone);
    CHECK(copy->borderDamagePerBlock==info->borderDamagePerBlock);
    CHECK(copy->borderWarningDistance==info->borderWarningDistance);
    CHECK(copy->borderWarningTime==info->borderWarningTime);
    CHECK(copy->difficulty==info->difficulty);
    CHECK(copy->difficultyLocked==info->difficultyLocked);
    CHECK(copy->theGameRules==info->theGameRules&&copy->playerTag==info->playerTag&&copy->levelName==info->levelName);CHECK(GameRules_setOrCreateGameRule(copy->theGameRules,s(h,"keepInventory"),s(h,"true")));CHECK(GameRules_getBoolean(info->theGameRules,s(h,"keepInventory")));
    MCObjectRoot original={0},duplicate={0};CHECK(MCObjectRoot_init(&original,h,(MCObject *)info)&&MCObjectRoot_init(&duplicate,h,(MCObject *)copy));CHECK(MCObjectHeap_collect(h));
    MCObjectHeap *branch=MCObjectHeap_clone(h);CHECK(branch);MCObjectRoot a={0},b={0};CHECK(MCObjectRoot_rebind(&a,branch,&original)&&MCObjectRoot_rebind(&b,branch,&duplicate));WorldInfo *ca=(WorldInfo *)MCObjectRoot_get(&a),*cb=(WorldInfo *)MCObjectRoot_get(&b);CHECK(ca!=info&&ca->theGameRules==cb->theGameRules&&ca->playerTag==cb->playerTag);CHECK(ca->terrainType==WorldType_getStatics(branch)->FLAT);CHECK(WorldType_getStatics(branch)->DEFAULT==WorldType_getStatics(branch)->worldTypes->items[0]);CHECK(WorldInfo_setSpawnY(ca,17));CHECK(info->spawnY==INT32_MAX);CHECK(MCObjectHeap_adopt(h,branch));MCObjectHeap_free(branch);info=(WorldInfo *)MCObjectRoot_get(&original);copy=(WorldInfo *)MCObjectRoot_get(&duplicate);CHECK(info->spawnY==17&&info->theGameRules==copy->theGameRules);CHECK(MCObjectHeap_collect(h));
    MCObjectRoot_drop(&original);MCObjectRoot_drop(&duplicate);MCObjectHeap_free(h);
}
static void settings_and_virtual(void) {
    MCObjectHeap *h=heap();WorldTypeStatics *types=WorldType_getStatics(h);CHECK(types);
    WorldSettings *settings=WorldSettings_new(h,INT64_MIN,&WorldSettingsGameType_CREATIVE,true,true,types->FLAT);CHECK(settings);CHECK(NBTString_equalsASCII(WorldSettings_getWorldName(settings),""));CHECK(!WorldSettings_areCommandsAllowed(settings)&&!WorldSettings_isBonusChestEnabled(settings));CHECK(WorldSettings_enableCommands(settings)==settings&&WorldSettings_enableBonusChest(settings)==settings);NBTString *options=s(h,"custom");CHECK(WorldSettings_setWorldName(settings,options)==settings);
    WorldInfo *info=WorldInfo_new(h,settings,NULL);CHECK(info);CHECK(info->randomSeed==INT64_MIN&&info->theGameType==&WorldSettingsGameType_CREATIVE&&info->terrainType==types->FLAT&&info->generatorOptions==options&&info->levelName==NULL&&info->hardcore&&info->allowCommands&&!info->initialized&&info->difficulty==EnumDifficulty_getStatics(h)->NORMAL);
    WorldSettings *from=WorldSettings_newFromInfo(h,info);CHECK(from&&from->seed==info->randomSeed&&from->terrainType==types->FLAT&&!from->commandsAllowed&&!from->bonusChestEnabled&&NBTString_equalsASCII(from->worldName,""));CHECK(WorldSettings_getGameTypeById(919)==&WorldSettingsGameType_SURVIVAL);
    WorldSettings *nullable=WorldSettings_new(h,1,NULL,false,false,NULL);CHECK(nullable&&nullable->theGameType==NULL&&nullable->terrainType==NULL);CHECK(WorldSettings_setWorldName(nullable,NULL)==nullable);WorldInfo *ni=WorldInfo_new(h,nullable,NULL);CHECK(ni&&ni->terrainType==NULL&&ni->generatorOptions==NULL&&ni->theGameType==NULL);
    Fixture *f=fixture(h);f->settings=settings;f->info=WorldInfo_nativeAllocate(h,&populater,(MCObject *)f);CHECK(f->info&&WorldInfo_constructSettings(f->info,settings,options));CHECK(f->calls==1&&f->info->randomSeed==77&&!f->info->initialized&&f->info->levelName==options&&f->info->difficulty==EnumDifficulty_getStatics(h)->NORMAL);
    Fixture *g=fixture(h);g->info=WorldInfo_nativeAllocate(h,&getters,(MCObject *)g);CHECK(g->info&&WorldInfo_construct(g->info));g->info->randomSeed=41;from=WorldSettings_newFromInfo(h,g->info);CHECK(from&&g->calls==5&&!memcmp(g->events,"sgmht",5));CHECK(from->seed==42&&from->theGameType==&WorldSettingsGameType_ADVENTURE&&from->mapFeaturesEnabled&&from->hardcoreEnabled&&from->terrainType==types->AMPLIFIED);
    WorldInfo *copy=WorldInfo_newCopy(h,g->info);CHECK(copy&&copy->randomSeed==41&&copy->theGameType==NULL&&g->calls==5);
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)g->info));CHECK(MCObjectHeap_collect(h));MCObjectHeap *branch=MCObjectHeap_clone(h);CHECK(branch);MCObjectRoot br={0};CHECK(MCObjectRoot_rebind(&br,branch,&root));WorldInfo *gi=(WorldInfo *)MCObjectRoot_get(&br);Fixture *gf=(Fixture *)gi->virtualContext;CHECK(gf->info==gi&&WorldInfo_getSeed(gi)==42&&gf->calls==6&&g->calls==5);MCObjectHeap_free(branch);MCObjectRoot_drop(&root);MCObjectHeap_free(h);
    for(int failAt=1;failAt<=5;failAt++){h=heap();Fixture *v=fixture(h);v->failAt=failAt;v->info=WorldInfo_nativeAllocate(h,&getters,(MCObject *)v);CHECK(v->info&&WorldInfo_construct(v->info));WorldSettings *target=WorldSettings_nativeAllocate(h);CHECK(target);target->seed=919;CHECK(!WorldSettings_constructFromInfo(target,v->info)&&MCObjectHeap_failed(h));CHECK(v->calls==failAt&&target->seed==919&&target->worldName==NULL);MCObjectHeap_free(h);}
    h=heap();Fixture *v=fixture(h);v->settings=WorldSettings_new(h,1,NULL,false,false,NULL);v->failedPopulate=true;v->info=WorldInfo_nativeAllocate(h,&populater,(MCObject *)v);CHECK(v->info&&!WorldInfo_constructSettings(v->info,v->settings,NULL));CHECK(v->calls==1&&v->info->randomSeed==77&&v->info->initialized&&v->info->levelName==NULL&&v->info->difficulty==NULL);MCObjectHeap_free(h);
}
static void rule_values(void) {
    MCObjectHeap *h=heap();GameRules *g=GameRules_new(h);CHECK(g);GameRulesStringArray *keys=GameRules_getRules(g);CHECK(keys&&keys->length==15);
    for(int32_t i=1;i<keys->length;i++){const uint16_t *a=NBTString_units(keys->items[i-1]),*b=NBTString_units(keys->items[i]);CHECK(a[0]<=b[0]);}
    NBTString *key=s(h,"custom"),*text=s(h,"2.5");CHECK(GameRules_setOrCreateGameRule(g,key,text));GameRulesValue *value=(GameRulesValue *)NativeSortedStringMap_get(g->theGameRules,key);CHECK(value&&value->valueString==text&&value->valueDouble==2.5&&value->valueInteger==0&&value->type==&GameRules_ANY_VALUE);CHECK(GameRules_areSameType(g,key,&GameRules_ANY_VALUE)&&!GameRules_areSameType(g,key,&GameRules_BOOLEAN_VALUE));
    CHECK(GameRules_setOrCreateGameRule(g,s(h,"custom"),s(h,"garbage")));CHECK(NativeSortedStringMap_get(g->theGameRules,key)==(MCObject *)value&&value->valueDouble==2.5&&value->valueInteger==0);CHECK(GameRulesValue_setValue(value,s(h,"TrUe"))&&value->valueBoolean&&value->valueInteger==1&&value->valueDouble==2.5);CHECK(GameRulesValue_setValue(value,s(h," 1 "))&&!value->valueBoolean&&value->valueInteger==0&&value->valueDouble==1);
    CHECK(GameRules_addGameRule(g,s(h,"typed"),s(h,"7"),&GameRules_NUMERICAL_VALUE));CHECK(GameRules_setOrCreateGameRule(g,s(h,"typed"),s(h,"true")));CHECK(GameRules_areSameType(g,s(h,"typed"),&GameRules_NUMERICAL_VALUE)&&GameRules_areSameType(g,s(h,"typed"),&GameRules_ANY_VALUE));CHECK(GameRules_addGameRule(g,s(h,"nulltype"),s(h,"false"),NULL));CHECK(GameRules_areSameType(g,s(h,"nulltype"),NULL));
    NBTTagCompound *tag=GameRules_writeToNBT(g);CHECK(tag&&NBTTagCompound_getString(tag,key)==value->valueString);CHECK(NBTTagCompound_setInteger_ascii(tag,"typed",7));CHECK(NBTTagCompound_setString_ascii(tag,"new",s(h,"17")));CHECK(GameRules_readFromNBT(g,tag));CHECK(GameRules_hasRule(g,s(h,"doFireTick"))&&GameRules_getInt(g,s(h,"new"))==17&&NBTString_equalsASCII(GameRules_getString(g,s(h,"typed")),""));
    CHECK(!GameRules_hasRule(g,s(h,"missing"))&&!GameRules_getBoolean(g,s(h,"missing"))&&GameRules_getInt(g,s(h,"missing"))==0&&NBTString_equalsASCII(GameRules_getString(g,s(h,"missing")),""));
    CHECK(!GameRulesValue_setValue(value,NULL)&&MCObjectHeap_failed(h));CHECK(value->valueString==NULL&&!value->valueBoolean&&value->valueInteger==0&&value->valueDouble==1);MCObjectHeap_free(h);
}
static void fill(MCObjectHeap *h,size_t budget,size_t remaining){static const MCObjectClass filler={"test.filler",MCObjectHeap_plainClone,NULL,NULL};size_t n=budget-MCObjectHeap_liveBytes(h)-remaining;CHECK(n>=sizeof(MCObject));CHECK(MCObjectHeap_alloc(h,n,&filler));}
static void boundaries_and_failure(void) {
    MCObjectHeap *h=heap();CHECK(!EnumDifficulty_getDifficultyEnum(h,-1)&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
    h=heap();WorldType *t=WorldType_nativeAllocate(h);CHECK(t);CHECK(!WorldType_construct(t,16,NULL,7)&&MCObjectHeap_failed(h));CHECK(t->worldTypeId==16&&t->canBeCreated&&t->generatorVersion==7);MCObjectHeap_free(h);
    h=heap();GameRules *g=GameRules_new(h);CHECK(g);size_t n=MCObjectHeap_liveObjects(h);CHECK(!GameRules_addGameRule(g,NULL,s(h,"5"),NULL)&&MCObjectHeap_failed(h));CHECK(g->theGameRules->size==15&&MCObjectHeap_liveObjects(h)>n);MCObjectHeap_free(h);
    h=heap();g=GameRules_nativeAllocate(h);CHECK(g);CHECK(!GameRules_getRules(g)&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
    h=heap();MCObjectHeap *other=heap();WorldInfo *p=WorldInfo_nativeAllocate(h,NULL,NULL);CHECK(p&&WorldInfo_construct(p));CHECK(!WorldInfo_setWorldName(p,s(other,"foreign"))&&MCObjectHeap_failed(h)&&!MCObjectHeap_failed(other));MCObjectHeap_free(h);MCObjectHeap_free(other);
    h=heap();p=WorldInfo_nativeAllocate(h,NULL,NULL);CHECK(p);MCObject *small=MCObjectHeap_alloc(h,sizeof(MCObject),p->object.klass);CHECK(small&&!WorldInfo_isInstance(small));CHECK(!WorldInfo_construct((WorldInfo *)small)&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
    const size_t budget=128u*1024u;h=MCObjectHeap_new(budget);CHECK(h);p=WorldInfo_nativeAllocate(h,NULL,NULL);CHECK(p&&WorldInfo_construct(p));GameRules *old=p->theGameRules;p->borderSize=919;fill(h,budget,sizeof(GameRules)-1);CHECK(!WorldInfo_construct(p)&&MCObjectHeap_failed(h));CHECK(p->theGameRules==old&&p->borderSize==6e7);MCObjectHeap_free(h);
    h=MCObjectHeap_new(budget);CHECK(h);g=GameRules_new(h);CHECK(g);NBTString *key=s(h,"new"),*text=s(h,"1");fill(h,budget,sizeof(GameRulesValue));CHECK(!GameRules_addGameRule(g,key,text,NULL)&&MCObjectHeap_failed(h));CHECK(g->theGameRules->size==15&&!NativeSortedStringMap_isInstance((MCObject *)text));MCObjectHeap_free(h);
    h=MCObjectHeap_new(sizeof(WorldSettings));CHECK(h);WorldSettings *settings=WorldSettings_nativeAllocate(h);CHECK(settings);settings->seed=99;CHECK(!WorldSettings_construct(settings,17,NULL,true,true,NULL)&&MCObjectHeap_failed(h));CHECK(settings->seed==99&&settings->worldName==NULL);MCObjectHeap_free(h);
}
int main(void){defaults_and_refs();settings_and_virtual();rule_values();boundaries_and_failure();printf("Source WorldInfo: %u checks\n",checks);return 0;}
