#include "world/WorldProvider.h"
#include "world/WorldProviderSurface.h"
#include "world/WorldProviderHell.h"
#include "world/WorldProviderEnd.h"
#include "world/WorldType.h"
#include "util/BlockPos.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"provider check %u line %d: %s\n",checks,__LINE__,#x);exit(1);}}while(0)
static uint32_t bits(float x) {uint32_t u;memcpy(&u,&x,sizeof u);return u;}
typedef struct {MCObject object;WorldType *type;NBTString *options;} Info;
typedef struct {MCObject object;Info *info;int id;} TestWorld;
typedef struct {MCObject object;MCObject *world,*biome;float rainfall;} Manager;
typedef struct {
    MCObject object;WorldProvider *provider;TestWorld *worlds[2];Info *infos[2];
    MCObject *biomes[4],*flat;int events[32],count,fail,worldCalls;bool swapWorld;
    float rainfall;World *managerWorld;
} Context;
static void info_trace(MCObject *o,MCObjectVisitor v,void *c) {Info *i=(Info *)o;i->type=(WorldType *)v((MCObject *)i->type,c);i->options=(NBTString *)v((MCObject *)i->options,c);}
static void world_trace(MCObject *o,MCObjectVisitor v,void *c) {TestWorld *w=(TestWorld *)o;w->info=(Info *)v((MCObject *)w->info,c);}
static void manager_trace(MCObject *o,MCObjectVisitor v,void *c) {Manager *m=(Manager *)o;m->world=v(m->world,c);m->biome=v(m->biome,c);}
static void context_trace(MCObject *o,MCObjectVisitor v,void *c) {
    Context *x=(Context *)o;x->provider=(WorldProvider *)v((MCObject *)x->provider,c);
    for(int i=0;i<2;i++){x->worlds[i]=(TestWorld *)v((MCObject *)x->worlds[i],c);x->infos[i]=(Info *)v((MCObject *)x->infos[i],c);}
    for(int i=0;i<4;i++)x->biomes[i]=v(x->biomes[i],c);
    x->flat=v(x->flat,c);x->managerWorld=(World *)v((MCObject *)x->managerWorld,c);
}
static const MCObjectClass infoClass={"fixture.provider.WorldInfo",MCObjectHeap_plainClone,info_trace,NULL};
static const MCObjectClass worldClass={"fixture.provider.World",MCObjectHeap_plainClone,world_trace,NULL};
static const MCObjectClass managerClass={"fixture.provider.manager",MCObjectHeap_plainClone,manager_trace,NULL};
static const MCObjectClass contextClass={"fixture.provider.Context",MCObjectHeap_plainClone,context_trace,NULL};
static const MCObjectClass tokenClass={"fixture.provider.biome",MCObjectHeap_plainClone,NULL,NULL};
enum {INFO=1,TERRAIN,OPTIONS,PARSE,FLAT_BIOME,FALLBACK,BIOME,PLAINS,HELL,SKY,NEW_MANAGER,NEW_HELL_MANAGER,OVERRIDE_MANAGER,OVERRIDE_LIGHT};
static bool event(Context *c,int e) {CHECK(c->count<32);c->events[c->count++]=e;if(c->fail==e){MCObjectHeap_fail(c->object.heap);return false;}return true;}
static WorldInfo *get_info(MCObject *o,World *w) {
    Context *c=(Context *)o;if(!event(c,INFO))return NULL;++c->worldCalls;return (WorldInfo *)((TestWorld *)w)->info;
}
static WorldType *get_terrain(MCObject *o,WorldInfo *i) {
    Context *c=(Context *)o;if(!event(c,TERRAIN))return NULL;
    if(c->swapWorld)c->provider->worldObj=(World *)c->worlds[1];
    return ((Info *)i)->type;
}
static NBTString *get_options(MCObject *o,WorldInfo *i) {Context *c=(Context *)o;return event(c,OPTIONS)?((Info *)i)->options:NULL;}
static MCObject *parse(MCObject *o,NBTString *s) {Context *c=(Context *)o;CHECK(s==c->infos[0]->options);return event(c,PARSE)?c->flat:NULL;}
static bool flat_biome(MCObject *o,MCObject *f,int32_t *out) {Context *c=(Context *)o;CHECK(f==c->flat);if(!event(c,FLAT_BIOME))return false;*out=127;return true;}
static MCObject *fallback(MCObject *o) {Context *c=(Context *)o;return event(c,FALLBACK)?c->biomes[0]:NULL;}
static MCObject *biome(MCObject *o,int32_t id,MCObject *f) {Context *c=(Context *)o;CHECK(id==127&&f==c->biomes[0]);return event(c,BIOME)?c->biomes[1]:NULL;}
static MCObject *plains(MCObject *o) {Context *c=(Context *)o;return event(c,PLAINS)?c->biomes[1]:NULL;}
static MCObject *hell(MCObject *o) {Context *c=(Context *)o;return event(c,HELL)?c->biomes[2]:NULL;}
static MCObject *sky(MCObject *o) {Context *c=(Context *)o;return event(c,SKY)?c->biomes[3]:NULL;}
static MCObject *new_manager(MCObject *o,World *w) {
    Context *c=(Context *)o;if(!event(c,NEW_MANAGER))return NULL;c->managerWorld=w;
    Manager *m=(Manager *)MCObjectHeap_alloc(c->object.heap,sizeof(*m),&managerClass);if(m)m->world=(MCObject *)w;return (MCObject *)m;
}
static MCObject *new_hell_manager(MCObject *o,MCObject *b,float f) {
    Context *c=(Context *)o;if(!event(c,NEW_HELL_MANAGER))return NULL;
    CHECK(!c->provider->isHellWorld&&!c->provider->hasNoSky&&c->provider->dimensionId==0);
    c->rainfall=f;Manager *m=(Manager *)MCObjectHeap_alloc(c->object.heap,sizeof(*m),&managerClass);if(m){m->biome=b;m->rainfall=f;}return (MCObject *)m;
}
static bool manager_override(MCObject *o,WorldProvider *p) {Context *c=(Context *)o;CHECK(p==c->provider);if(!event(c,OVERRIDE_MANAGER))return false;p->worldChunkMgr=c->flat;return true;}
static bool light_override(MCObject *o,WorldProvider *p) {Context *c=(Context *)o;CHECK(p==c->provider);p->lightBrightnessTable->data[0]=0.875f;return event(c,OVERRIDE_LIGHT);}
static const WorldProviderDependencies dependencies={
    .getWorldInfo=get_info,.getTerrainType=get_terrain,.getGeneratorOptions=get_options,
    .parseFlatGenerator=parse,.getFlatBiome=flat_biome,.getFallbackBiome=fallback,
    .getPlainsBiome=plains,.getHellBiome=hell,.getSkyBiome=sky,.getBiomeFromBiomeList=biome,
    .newWorldChunkManager=new_manager,.newWorldChunkManagerHell=new_hell_manager};
static Context *setup(MCObjectHeap *h,int dimension,int type) {
    Context *c=(Context *)MCObjectHeap_alloc(h,sizeof(*c),&contextClass);CHECK(c);
    WorldTypeStatics *s=WorldType_getStatics(h);CHECK(s);
    for(int i=0;i<2;i++) {
        c->infos[i]=(Info *)MCObjectHeap_alloc(h,sizeof(Info),&infoClass);CHECK(c->infos[i]);
        c->infos[i]->type=type==1?s->FLAT:type==2?s->DEBUG_WORLD:s->DEFAULT;
        c->infos[i]->options=NBTString_fromASCII(h,i?"second":"first");CHECK(c->infos[i]->options);
        c->worlds[i]=(TestWorld *)MCObjectHeap_alloc(h,sizeof(TestWorld),&worldClass);CHECK(c->worlds[i]);
        c->worlds[i]->info=c->infos[i];c->worlds[i]->id=i;
    }
    for(int i=0;i<4;i++){c->biomes[i]=MCObjectHeap_alloc(h,sizeof(MCObject),&tokenClass);CHECK(c->biomes[i]);}
    c->flat=MCObjectHeap_alloc(h,sizeof(MCObject),&tokenClass);CHECK(c->flat);
    c->provider=WorldProvider_getProviderForDimension(h,dimension,&dependencies,(MCObject *)c);CHECK(c->provider);
    return c;
}
static void factory_and_arrays(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);
    WorldProvider *p=WorldProvider_getProviderForDimension(heap,-1,NULL,NULL);CHECK(p);
    CHECK(p->lightBrightnessTable&&p->lightBrightnessTable->length==16);
    CHECK(p->colorsSunriseSunset&&p->colorsSunriseSunset->length==4);
    CHECK(p->dimensionId==0&&!p->isHellWorld&&!p->hasNoSky);
    CHECK(WorldProviderHell_isInstance((MCObject *)p)&&WorldProvider_isInstance((MCObject *)p));
    CHECK(WorldProvider_getLightBrightnessTable(p)==p->lightBrightnessTable&&!WorldProvider_getWorldChunkManager(p));
    CHECK(!WorldProvider_getHasNoSky(p)&&!WorldProvider_doesWaterVaporize(p)&&WorldProvider_getDimensionId(p)==0);
    for(int i=0;i<16;i++)CHECK(bits(p->lightBrightnessTable->data[i])==0);
    for(int i=0;i<4;i++)CHECK(bits(p->colorsSunriseSunset->data[i])==0);
    NativeFloatArray *moon=WorldProvider_moonPhaseFactors(heap);CHECK(moon&&moon->length==8);
    CHECK(moon==WorldProvider_moonPhaseFactors(heap));moon->data[0]=0.125f;
    CHECK(WorldProvider_moonPhaseFactors(heap)->data[0]==0.125f);
    CHECK(!WorldProvider_getProviderForDimension(heap,2,NULL,NULL)&&!MCObjectHeap_failed(heap));
    for(int dimension=-1;dimension<=1;dimension++) {
        WorldProvider *q=WorldProvider_getProviderForDimension(heap,dimension,NULL,NULL);CHECK(q);
        CHECK(q!=p&&q->lightBrightnessTable!=p->lightBrightnessTable);
        CHECK(q->lightBrightnessTable!=q->colorsSunriseSunset);
        CHECK(NBTString_equalsASCII(WorldProvider_getDimensionName(q),dimension<0?"Nether":dimension==0?"Overworld":"The End"));
        CHECK(NBTString_equalsASCII(WorldProvider_getInternalNameSuffix(q),dimension<0?"_nether":dimension==0?"":"_end"));
    }
    MCObjectHeap_free(heap);
}
static void register_order_and_manager_branches(void) {
    const int ordinary[]={INFO,TERRAIN,INFO,OPTIONS,INFO,TERRAIN,NEW_MANAGER};
    const int flat[]={INFO,TERRAIN,INFO,OPTIONS,INFO,TERRAIN,INFO,OPTIONS,PARSE,FLAT_BIOME,FALLBACK,BIOME,NEW_HELL_MANAGER};
    const int debug[]={INFO,TERRAIN,INFO,OPTIONS,INFO,TERRAIN,PLAINS,NEW_HELL_MANAGER};
    const int nether[]={INFO,TERRAIN,INFO,OPTIONS,HELL,NEW_HELL_MANAGER};
    const int end[]={INFO,TERRAIN,INFO,OPTIONS,SKY,NEW_HELL_MANAGER};
    for(int d=-1;d<=1;d++)for(int type=0;type<3;type++) {
        MCObjectHeap *h=MCObjectHeap_new(4*1024*1024);CHECK(h);Context *c=setup(h,d,type);WorldProvider *p=c->provider;
        CHECK(WorldProvider_registerWorld(p,(World *)c->worlds[0]));
        CHECK(p->worldObj==(World *)c->worlds[0]&&p->terrainType==c->infos[0]->type&&p->generatorSettings==c->infos[0]->options);
        CHECK(p->worldChunkMgr&&WorldProvider_getWorldChunkManager(p)==p->worldChunkMgr);
        const int *expected=d<0?nether:d>0?end:type==1?flat:type==2?debug:ordinary;
        int count=d?6:type==1?13:type==2?8:7;CHECK(c->count==count&&!memcmp(c->events,expected,(size_t)count*sizeof(int)));
        CHECK(p->dimensionId==d&&p->hasNoSky==(d!=0)&&p->isHellWorld==(d<0));
        CHECK(bits(p->lightBrightnessTable->data[0])==(d<0?UINT32_C(0x3dcccccd):0));
        CHECK(p->lightBrightnessTable->data[15]==1.0f);
        if(!d&&!type)CHECK(c->managerWorld==(World *)c->worlds[0]);
        if(!d&&type==1)CHECK(c->rainfall==0.5f);
        MCObjectHeap_free(h);
    }
    MCObjectHeap *h=MCObjectHeap_new(4*1024*1024);CHECK(h);Context *c=setup(h,0,0);c->swapWorld=true;
    CHECK(WorldProvider_registerWorld(c->provider,(World *)c->worlds[0]));
    CHECK(c->provider->generatorSettings==c->infos[0]->options);
    CHECK(c->provider->worldObj==(World *)c->worlds[1]&&c->managerWorld==(World *)c->worlds[1]);
    MCObjectHeap_free(h);
}
static void partial_failure_and_required_dependencies(void) {
    for(int failure=INFO;failure<=NEW_HELL_MANAGER;failure++) {
        MCObjectHeap *h=MCObjectHeap_new(4*1024*1024);CHECK(h);
        int d=failure==HELL?-1:failure==SKY?1:0;int type=failure==PLAINS?2:failure==NEW_MANAGER?0:1;
        Context *c=setup(h,d,type);c->fail=failure;
        CHECK(!WorldProvider_registerWorld(c->provider,(World *)c->worlds[0])&&MCObjectHeap_failed(h));
        CHECK(c->events[c->count-1]==failure&&c->provider->worldObj==(World *)c->worlds[0]);
        CHECK(!c->provider->worldChunkMgr&&!c->provider->hasNoSky&&!c->provider->isHellWorld&&c->provider->dimensionId==0);
        if(failure==INFO||failure==TERRAIN)CHECK(!c->provider->terrainType&&!c->provider->generatorSettings);
        else CHECK(c->provider->terrainType==c->infos[0]->type);
        MCObjectHeap_free(h);
    }
    MCObjectHeap *h=MCObjectHeap_new(4*1024*1024);CHECK(h);Context *c=setup(h,0,0);
    CHECK(!WorldProvider_registerWorld(c->provider,NULL)&&MCObjectHeap_failed(h)&&!c->provider->worldObj&&c->count==0);MCObjectHeap_free(h);
    h=MCObjectHeap_new(4*1024*1024);CHECK(h);c=setup(h,0,0);WorldProviderDependencies missing=dependencies;missing.newWorldChunkManager=NULL;c->provider->dependencies=&missing;
    CHECK(!WorldProvider_registerWorld(c->provider,(World *)c->worlds[0])&&MCObjectHeap_failed(h));
    CHECK(c->provider->generatorSettings==c->infos[0]->options&&!c->provider->worldChunkMgr);MCObjectHeap_free(h);
    for(int f=OVERRIDE_MANAGER;f<=OVERRIDE_LIGHT;f++) {
        h=MCObjectHeap_new(4*1024*1024);CHECK(h);c=setup(h,0,0);WorldProviderDependencies overrides=dependencies;
        overrides.registerWorldChunkManager=manager_override;overrides.generateLightBrightnessTable=light_override;
        c->provider->dependencies=&overrides;c->fail=f;
        CHECK(!WorldProvider_registerWorld(c->provider,(World *)c->worlds[0])&&MCObjectHeap_failed(h));
        if(f==OVERRIDE_MANAGER)CHECK(!c->provider->worldChunkMgr&&c->count==5);
        else CHECK(c->provider->worldChunkMgr==c->flat&&c->provider->lightBrightnessTable->data[0]==0.875f&&c->count==6);
        MCObjectHeap_free(h);
    }
}
static void border_and_block_position(void) {
    MCObjectHeap *h=MCObjectHeap_new(4*1024*1024);CHECK(h);
    WorldProvider *p=WorldProvider_getProviderForDimension(h,-1,NULL,NULL);CHECK(p);
    WorldBorder *a=WorldProvider_getWorldBorder(p),*b=WorldProvider_getWorldBorder(p);CHECK(a&&b&&a!=b);
    CHECK(WorldProviderHellBorder_isInstance((MCObject *)a)&&WorldBorder_isInstance((MCObject *)a));
    CHECK(((WorldProviderHellBorder *)a)->this_0==(WorldProviderHell *)p);
    a->centerX=80;a->centerZ=-64;a->startDiameter=a->endDiameter=20;
    CHECK(WorldBorder_getCenterX(a)==10&&WorldBorder_getCenterZ(a)==-8);
    double bound;CHECK(WorldBorder_minX(a,&bound)&&bound==0);CHECK(WorldBorder_maxX(a,&bound)&&bound==20);
    CHECK(WorldBorder_minZ(a,&bound)&&bound==-18);CHECK(WorldBorder_maxZ(a,&bound)&&bound==2);
    BlockPos *inside=DataWatcher_blockPos(h,0,100,-18);CHECK(inside);bool result=false;
    CHECK(WorldBorder_containsBlockPos(a,inside,&result)&&result);
    inside->x=-1;CHECK(WorldBorder_containsBlockPos(a,inside,&result)&&!result);
    inside->x=INT32_MAX;CHECK(WorldBorder_containsBlockPos(a,inside,&result)&&!result);
    BlockPos *d=BlockPos_newDouble(h,-0.5,NAN,-INFINITY);CHECK(d&&d->x==-1&&d->y==0&&d->z==INT32_MAX);
    CHECK(BlockPos_downN(d,0)==d);d->y=INT32_MIN;
    BlockPos *down=BlockPos_down(d);CHECK(down&&down!=d&&down->x==-1&&down->y==INT32_MAX&&down->z==INT32_MAX);
    down=BlockPos_downN(d,INT32_MIN);CHECK(down&&down->y==0);
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)a));CHECK(MCObjectHeap_collect(h));
    CHECK(((WorldProviderHellBorder *)a)->this_0==(WorldProviderHell *)p&&NativeFloatArray_isInstance((MCObject *)p->lightBrightnessTable));
    MCObjectHeap *clone=MCObjectHeap_clone(h);CHECK(clone);MCObjectRoot copied={0};CHECK(MCObjectRoot_rebind(&copied,clone,&root));
    WorldProviderHellBorder *ca=(WorldProviderHellBorder *)MCObjectRoot_get(&copied);
    CHECK(ca&&ca!=(WorldProviderHellBorder *)a&&ca->this_0!=(WorldProviderHell *)p);
    CHECK(ca->border.overrides==a->overrides&&WorldBorder_getCenterX(&ca->border)==10);
    CHECK(ca->this_0->provider.lightBrightnessTable!=p->lightBrightnessTable);
    CHECK(MCObjectHeap_adopt(h,clone));CHECK(MCObjectRoot_rebind(&copied,h,&copied));MCObjectHeap_free(clone);
    ca=(WorldProviderHellBorder *)MCObjectRoot_get(&root);CHECK(ca&&WorldBorder_getCenterZ(&ca->border)==-8);CHECK(MCObjectHeap_collect(h));
    MCObjectRoot_drop(&root);MCObjectRoot_drop(&copied);CHECK(MCObjectHeap_collect(h));MCObjectHeap_free(h);
}
typedef struct {MCObject object;int32_t x[2],z[2];int nx,nz,calls,failAt;int order[8],orderCount,failEdge,containsCalls;double edges[4];bool acceptNull;} PositionContext;
static const MCObjectClass positionClass={"fixture.provider.PositionContext",MCObjectHeap_plainClone,NULL,NULL};
static bool position_x(MCObject *o,BlockPos *p,int32_t *out) {
    PositionContext *c=(PositionContext *)o;CHECK(BlockPos_isInstance((MCObject *)p)&&MCObjectHeap_hasBorrowers(o->heap));
    if(++c->calls==c->failAt)return false;
    CHECK(c->orderCount<8);c->order[c->orderCount++]=c->nx?3:1;
    CHECK(c->nx<2);*out=c->x[c->nx++];return true;
}
static bool position_z(MCObject *o,BlockPos *p,int32_t *out) {
    PositionContext *c=(PositionContext *)o;CHECK(BlockPos_isInstance((MCObject *)p)&&MCObjectHeap_hasBorrowers(o->heap));
    if(++c->calls==c->failAt)return false;
    CHECK(c->orderCount<8);c->order[c->orderCount++]=c->nz?7:5;
    CHECK(c->nz<2);*out=c->z[c->nz++];return true;
}
static const WorldBorderPositionDependencies positionDependencies={position_x,position_z};
static bool position_edge(WorldBorder *b,double *out,int index) {
    PositionContext *c=(PositionContext *)b->dependencyContext;CHECK(c&&MCObjectHeap_hasBorrowers(b->object.heap));
    CHECK(c->orderCount<8);c->order[c->orderCount++]=2+index*2;
    if(c->failEdge==index+1)return false;
    *out=c->edges[index];return true;
}
static bool position_minx(WorldBorder *b,double *out){return position_edge(b,out,0);}
static bool position_maxx(WorldBorder *b,double *out){return position_edge(b,out,1);}
static bool position_minz(WorldBorder *b,double *out){return position_edge(b,out,2);}
static bool position_maxz(WorldBorder *b,double *out){return position_edge(b,out,3);}
static bool recording_contains(WorldBorder *b,BlockPos *p,bool *out) {
    PositionContext *c=(PositionContext *)b->dependencyContext;CHECK(c&&MCObjectHeap_hasBorrowers(b->object.heap));++c->containsCalls;
    if(!p&&c->acceptNull){*out=true;return true;}
    return WorldBorder_containsBlockPosBase(b,p,out);
}
static const WorldBorderOverrides positionOverrides={.minX=position_minx,.maxX=position_maxx,.minZ=position_minz,.maxZ=position_maxz,.containsBlockPos=recording_contains};
static void virtual_position_order_and_lifetime(void) {
    for(int stop=0;stop<4;stop++) {
        MCObjectHeap *h=MCObjectHeap_new(1024*1024);CHECK(h);WorldBorder *b=WorldBorder_new(h,NULL,NULL);CHECK(b);
        b->startDiameter=b->endDiameter=20;
        PositionContext *c=(PositionContext *)MCObjectHeap_alloc(h,sizeof(*c),&positionClass);CHECK(c);
        c->x[0]=stop==0?-11:0;c->x[1]=stop==1?10:0;c->z[0]=stop==2?-11:0;c->z[1]=stop==3?10:0;
        b->positionDependencies=&positionDependencies;b->positionContext=(MCObject *)c;
        bool value=true;CHECK(WorldBorder_containsBlockPos(b,DataWatcher_blockPos(h,0,0,0),&value)&&!value);
        CHECK(c->calls==stop+1&&c->nx==(stop?2:1)&&c->nz==(stop<2?0:stop-1));MCObjectHeap_free(h);
    }
    for(int failure=1;failure<=4;failure++) {
        MCObjectHeap *h=MCObjectHeap_new(1024*1024);CHECK(h);WorldBorder *b=WorldBorder_new(h,NULL,NULL);CHECK(b);
        b->startDiameter=b->endDiameter=20;
        PositionContext *c=(PositionContext *)MCObjectHeap_alloc(h,sizeof(*c),&positionClass);CHECK(c);c->failAt=failure;
        b->positionDependencies=&positionDependencies;b->positionContext=(MCObject *)c;bool result=true;
        CHECK(!WorldBorder_containsBlockPos(b,DataWatcher_blockPos(h,0,0,0),&result)&&MCObjectHeap_failed(h));
        CHECK(result&&c->calls==failure);MCObjectHeap_free(h);
    }
    MCObjectHeap *h=MCObjectHeap_new(1024*1024);CHECK(h);WorldBorder *b=WorldBorder_new(h,NULL,NULL);CHECK(b);
    PositionContext *c=(PositionContext *)MCObjectHeap_alloc(h,sizeof(*c),&positionClass);CHECK(c);
    b->positionDependencies=&positionDependencies;b->positionContext=(MCObject *)c;MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)b));
    CHECK(MCObjectHeap_collect(h));MCObjectHeap *clone=MCObjectHeap_clone(h);CHECK(clone);MCObjectRoot copy={0};CHECK(MCObjectRoot_rebind(&copy,clone,&root));
    WorldBorder *cb=(WorldBorder *)MCObjectRoot_get(&copy);CHECK(cb&&cb!=b&&cb->positionContext!=(MCObject *)c&&cb->positionDependencies==&positionDependencies);
    CHECK(MCObjectHeap_adopt(h,clone));MCObjectHeap_free(clone);b=(WorldBorder *)MCObjectRoot_get(&root);bool result=false;
    CHECK(WorldBorder_containsBlockPos(b,DataWatcher_blockPos(h,0,0,0),&result)&&result);
    CHECK(((PositionContext *)b->positionContext)->calls==4);MCObjectRoot_drop(&root);CHECK(MCObjectHeap_collect(h));MCObjectHeap_free(h);
    for(int failure=0;failure<=4;failure++) {
        h=MCObjectHeap_new(1024*1024);CHECK(h);b=WorldBorder_new(h,NULL,NULL);CHECK(b);
        c=(PositionContext *)MCObjectHeap_alloc(h,sizeof(*c),&positionClass);CHECK(c);
        c->edges[0]=c->edges[2]=-10;c->edges[1]=c->edges[3]=10;c->failEdge=failure;
        b->positionDependencies=&positionDependencies;b->positionContext=(MCObject *)c;b->dependencyContext=(MCObject *)c;b->overrides=&positionOverrides;
        bool value=false;bool ok=WorldBorder_containsBlockPos(b,DataWatcher_blockPos(h,0,0,0),&value);
        CHECK(c->containsCalls==1);
        if(!failure){CHECK(ok&&value&&c->orderCount==8);for(int i=0;i<8;i++)CHECK(c->order[i]==i+1);}
        else {CHECK(!ok&&!value&&MCObjectHeap_failed(h)&&c->orderCount==failure*2);for(int i=0;i<failure*2;i++)CHECK(c->order[i]==i+1);}
        MCObjectHeap_free(h);
    }
    for(int accept=0;accept<2;accept++) {
        h=MCObjectHeap_new(1024*1024);CHECK(h);b=WorldBorder_new(h,NULL,NULL);CHECK(b);
        c=(PositionContext *)MCObjectHeap_alloc(h,sizeof(*c),&positionClass);CHECK(c);c->acceptNull=accept!=0;
        b->dependencyContext=(MCObject *)c;b->overrides=&positionOverrides;bool value=false;
        bool ok=WorldBorder_containsBlockPos(b,NULL,&value);CHECK(c->containsCalls==1&&c->calls==0&&c->orderCount==0);
        CHECK(accept?(ok&&value&&!MCObjectHeap_failed(h)):(!ok&&!value&&MCObjectHeap_failed(h)));MCObjectHeap_free(h);
    }
}
static void allocation_prefix_and_native_guards(void) {
    bool sawArrays=false,sawFirstArray=false,sawClassOnly=false;
    for(size_t budget=64;budget<1800;budget+=8) {
        MCObjectHeap *h=MCObjectHeap_new(budget);CHECK(h);
        WorldProviderSurface *p=WorldProviderSurface_nativeAllocate(h,NULL,NULL);
        if(p) {
            MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)p));
            bool ok=WorldProviderSurface_construct(p);
            if(ok){CHECK(p->provider.lightBrightnessTable&&p->provider.colorsSunriseSunset);sawArrays=true;}
            else if(p->provider.lightBrightnessTable){CHECK(!p->provider.colorsSunriseSunset);sawFirstArray=true;}
            else {CHECK(!p->provider.colorsSunriseSunset);sawClassOnly=true;}
            MCObjectRoot_drop(&root);
        }
        MCObjectHeap_free(h);
    }
    CHECK(sawArrays&&sawFirstArray&&sawClassOnly);
    MCObjectHeap *h=MCObjectHeap_new(1024*1024);CHECK(h);WorldProvider *p=WorldProvider_getProviderForDimension(h,0,NULL,NULL);CHECK(p);
    MCObject *tiny=MCObjectHeap_alloc(h,sizeof(MCObject),p->object.klass);CHECK(tiny);CHECK(!WorldProvider_isInstance(tiny));
    NativeFloatArray *array=p->lightBrightnessTable;array->length=INT32_MAX;
    CHECK(!NativeFloatArray_isInstance((MCObject *)array));CHECK(!WorldProvider_generateLightBrightnessTable(p)&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
    h=MCObjectHeap_new(1024*1024);CHECK(h);p=WorldProvider_getProviderForDimension(h,0,NULL,NULL);CHECK(p);
    CHECK(!WorldProviderHell_construct((WorldProviderHell *)p)&&MCObjectHeap_failed(h));
    CHECK(!p->isHellWorld&&!p->hasNoSky&&p->dimensionId==0);MCObjectHeap_free(h);
    h=MCObjectHeap_new(1024*1024);CHECK(h);p=WorldProvider_getProviderForDimension(h,-1,NULL,NULL);CHECK(p);
    NativeFloatArray *small=NativeFloatArray_new(h,3);CHECK(small);p->lightBrightnessTable=small;
    CHECK(!WorldProviderHell_generateLightBrightnessTable((WorldProviderHell *)p)&&MCObjectHeap_failed(h));
    CHECK(bits(small->data[0])==UINT32_C(0x3dcccccd)&&bits(small->data[1])==UINT32_C(0x3ded2308)&&bits(small->data[2])==UINT32_C(0x3e088888));MCObjectHeap_free(h);
    h=MCObjectHeap_new(1024*1024);CHECK(h);MCObjectHeap *foreign=MCObjectHeap_new(1024*1024);CHECK(foreign);
    Context *c=setup(h,0,0);TestWorld *other=(TestWorld *)MCObjectHeap_alloc(foreign,sizeof(TestWorld),&worldClass);CHECK(other);
    CHECK(!WorldProvider_registerWorld(c->provider,(World *)other)&&MCObjectHeap_failed(h));
    CHECK(!c->provider->worldObj&&c->count==0&&!MCObjectHeap_failed(foreign));MCObjectHeap_free(h);MCObjectHeap_free(foreign);
}
int main(void) {
    factory_and_arrays();register_order_and_manager_branches();partial_failure_and_required_dependencies();
    border_and_block_position();virtual_position_order_and_lifetime();allocation_prefix_and_native_guards();
    printf("source world provider: %u checks\n",checks);return 0;
}
