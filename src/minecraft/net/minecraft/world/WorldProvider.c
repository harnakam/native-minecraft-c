#include "world/WorldProviderInternal.h"
#include "world/WorldProviderSurface.h"
#include "world/WorldProviderHell.h"
#include "world/WorldProviderEnd.h"
#include "world/WorldType.h"
#include "util/NativeMathCos.h"
#include <math.h>

static bool fail(WorldProvider *p) { MCObjectHeap_fail(p?p->object.heap:NULL);return false; }
void WorldProvider_traceFields(WorldProvider *p,MCObjectVisitor visit,void *context) {
    if(MCObjectHeap_objectSize((MCObject *)p)<sizeof(*p)){fail(p);return;}
    p->worldObj=(World *)visit((MCObject *)p->worldObj,context);
    p->terrainType=(WorldType *)visit((MCObject *)p->terrainType,context);
    p->generatorSettings=(NBTString *)visit((MCObject *)p->generatorSettings,context);
    p->worldChunkMgr=visit(p->worldChunkMgr,context);
    p->lightBrightnessTable=(NativeFloatArray *)visit((MCObject *)p->lightBrightnessTable,context);
    p->colorsSunriseSunset=(NativeFloatArray *)visit((MCObject *)p->colorsSunriseSunset,context);
    p->dependencyContext=visit(p->dependencyContext,context);
}
static void trace(MCObject *object,MCObjectVisitor visit,void *context) { WorldProvider_traceFields((WorldProvider *)object,visit,context); }
static const MCObjectClass providerClass={"net.minecraft.world.WorldProvider",MCObjectHeap_plainClone,trace,NULL};
bool WorldProvider_isInstance(const MCObject *object) {
    return object&&((object->klass==&providerClass&&MCObjectHeap_objectSize(object)>=sizeof(WorldProvider))||
        WorldProviderSurface_isInstance(object)||WorldProviderHell_isInstance(object)||WorldProviderEnd_isInstance(object));
}
bool NativeWorldProvider_begin(WorldProvider *p,MCObjectRootScope *scope) {
    if(!WorldProvider_isInstance((MCObject *)p)||MCObjectHeap_failed(p->object.heap)||!MCObjectRootScope_begin(scope,p->object.heap))return fail(p);
    if(MCObjectRootScope_pin(scope,(MCObject *)p)&&MCObjectRootScope_pin(scope,p->dependencyContext))return true;
    MCObjectRootScope_end(scope);return fail(p);
}
bool NativeWorldProvider_end(WorldProvider *p,MCObjectRootScope *scope,bool ok) {
    ok=ok&&!MCObjectHeap_failed(p->object.heap);if(!ok)fail(p);
    MCObjectRootScope_end(scope);return ok;
}
typedef struct {MCObject object;NativeFloatArray *moonPhaseFactors;} ProviderStatics;
static void static_trace(MCObject *object,MCObjectVisitor visit,void *context) {
    ProviderStatics *s=(ProviderStatics *)object;s->moonPhaseFactors=(NativeFloatArray *)visit((MCObject *)s->moonPhaseFactors,context);
}
static const MCObjectClass staticClass={"native.WorldProvider.statics",MCObjectHeap_plainClone,static_trace,NULL};
static bool any(const MCObject *object,void *context) {(void)object;(void)context;return true;}
NativeFloatArray *WorldProvider_moonPhaseFactors(MCObjectHeap *heap) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    ProviderStatics *s=(ProviderStatics *)MCObjectHeap_findObject(heap,&staticClass,any,NULL);
    if(s&&MCObjectHeap_objectSize((MCObject *)s)<sizeof(*s)){MCObjectHeap_fail(heap);s=NULL;}
    if(!s) {
        s=(ProviderStatics *)MCObjectHeap_alloc(heap,sizeof(*s),&staticClass);
        if(s&&MCObjectRootScope_pin(&scope,(MCObject *)s))s->moonPhaseFactors=NativeFloatArray_new(heap,8);
        if(s&&s->moonPhaseFactors) {
            const float values[]={1.0f,0.75f,0.5f,0.25f,0.0f,0.25f,0.5f,0.75f};
            for(int i=0;i<8;i++)s->moonPhaseFactors->data[i]=values[i];
            MCObjectRoot root={0};if(!MCObjectRoot_init(&root,heap,(MCObject *)s))s=NULL;
        } else s=NULL;
    }
    NativeFloatArray *result=s?s->moonPhaseFactors:NULL;
    if(!NativeFloatArray_isInstance((MCObject *)result)||result->object.heap!=heap||result->length!=8) {MCObjectHeap_fail(heap);result=NULL;}
    MCObjectRootScope_end(&scope);return result;
}
WorldProvider *NativeWorldProvider_allocate(MCObjectHeap *heap,size_t size,const MCObjectClass *klass,const WorldProviderDependencies *deps,MCObject *context) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    WorldProvider *p=NULL;
    if(!MCObjectRootScope_pin(&scope,context)||!WorldProvider_moonPhaseFactors(heap))goto done;
    p=(WorldProvider *)MCObjectHeap_alloc(heap,size,klass);
    if(p){p->dependencies=deps;p->dependencyContext=context;}
done:MCObjectRootScope_end(&scope);return p;
}
WorldProvider *WorldProvider_nativeAllocate(MCObjectHeap *heap,const WorldProviderDependencies *deps,MCObject *context) {
    return NativeWorldProvider_allocate(heap,sizeof(WorldProvider),&providerClass,deps,context);
}
bool WorldProvider_construct(WorldProvider *p) {
    MCObjectRootScope scope={0};if(!NativeWorldProvider_begin(p,&scope))return false;
    NativeFloatArray *array=NativeFloatArray_new(p->object.heap,16);
    if(!array)return NativeWorldProvider_end(p,&scope,false);
    p->lightBrightnessTable=array;
    array=NativeFloatArray_new(p->object.heap,4);
    if(!array)return NativeWorldProvider_end(p,&scope,false);
    p->colorsSunriseSunset=array;
    return NativeWorldProvider_end(p,&scope,true);
}
WorldProvider *WorldProvider_getProviderForDimension(MCObjectHeap *heap,int32_t dimension,const WorldProviderDependencies *deps,MCObject *context) {
    if(!WorldProvider_moonPhaseFactors(heap))return NULL;
    return dimension==-1?(WorldProvider *)WorldProviderHell_new(heap,deps,context):
        dimension==0?(WorldProvider *)WorldProviderSurface_new(heap,deps,context):
        dimension==1?(WorldProvider *)WorldProviderEnd_new(heap,deps,context):NULL;
}
static bool pin(WorldProvider *p,MCObjectRootScope *scope,MCObject *object) {
    return (!object||object->heap==p->object.heap)&&MCObjectRootScope_pin(scope,object)&&!MCObjectHeap_failed(p->object.heap);
}
static WorldInfo *world_info_from(WorldProvider *p,MCObjectRootScope *scope,World *world) {
    const WorldProviderDependencies *d=p->dependencies;
    if(!world||!pin(p,scope,(MCObject *)world)||!d||!d->getWorldInfo){fail(p);return NULL;}
    WorldInfo *info=d->getWorldInfo(p->dependencyContext,world);
    if(!pin(p,scope,(MCObject *)info)){fail(p);return NULL;}
    return info;
}
static WorldInfo *world_info(WorldProvider *p,MCObjectRootScope *scope) {
    return world_info_from(p,scope,p->worldObj);
}
static WorldType *terrain(WorldProvider *p,MCObjectRootScope *scope,WorldInfo *info) {
    const WorldProviderDependencies *d=p->dependencies;
    if(!info||!d||!d->getTerrainType){fail(p);return NULL;}
    WorldType *type=d->getTerrainType(p->dependencyContext,info);
    if(!pin(p,scope,(MCObject *)type)){fail(p);return NULL;}
    return type;
}
static NBTString *options(WorldProvider *p,MCObjectRootScope *scope,WorldInfo *info) {
    const WorldProviderDependencies *d=p->dependencies;
    if(!info||!d||!d->getGeneratorOptions){fail(p);return NULL;}
    NBTString *text=d->getGeneratorOptions(p->dependencyContext,info);
    if(!pin(p,scope,(MCObject *)text)||(text&&!NBTString_isInstance((MCObject *)text))){fail(p);return NULL;}
    return text;
}
bool WorldProvider_registerWorld(WorldProvider *p,World *world) {
    MCObjectRootScope scope={0};if(!NativeWorldProvider_begin(p,&scope))return false;
    bool ok=false;if(!pin(p,&scope,(MCObject *)world))goto done;
    p->worldObj=world;
    WorldInfo *info=world_info_from(p,&scope,world);
    WorldType *type=terrain(p,&scope,info);
    if(MCObjectHeap_failed(p->object.heap))goto done;
    p->terrainType=type;
    info=world_info_from(p,&scope,world);
    NBTString *text=options(p,&scope,info);
    if(MCObjectHeap_failed(p->object.heap))goto done;
    p->generatorSettings=text;
    const WorldProviderDependencies *d=p->dependencies;
    if(d&&d->registerWorldChunkManager)ok=d->registerWorldChunkManager(p->dependencyContext,p);
    else if(WorldProviderHell_isInstance((MCObject *)p))ok=WorldProviderHell_registerWorldChunkManager((WorldProviderHell *)p);
    else if(WorldProviderEnd_isInstance((MCObject *)p))ok=WorldProviderEnd_registerWorldChunkManager((WorldProviderEnd *)p);
    else ok=WorldProvider_registerWorldChunkManager(p);
    if(!ok||MCObjectHeap_failed(p->object.heap))goto done;
    d=p->dependencies;
    if(d&&d->generateLightBrightnessTable)ok=d->generateLightBrightnessTable(p->dependencyContext,p);
    else if(WorldProviderHell_isInstance((MCObject *)p))ok=WorldProviderHell_generateLightBrightnessTable((WorldProviderHell *)p);
    else ok=WorldProvider_generateLightBrightnessTable(p);
done:return NativeWorldProvider_end(p,&scope,ok);
}
bool NativeWorldProvider_assignHellManager(WorldProvider *p,MCObject *biome,float rainfall) {
    MCObjectRootScope scope={0};if(!NativeWorldProvider_begin(p,&scope))return false;
    const WorldProviderDependencies *d=p->dependencies;
    bool ok=false;
    if(!pin(p,&scope,biome)||!d||!d->newWorldChunkManagerHell)goto done;
    MCObject *manager=d->newWorldChunkManagerHell(p->dependencyContext,biome,rainfall);
    if(!manager||!pin(p,&scope,manager))goto done;
    p->worldChunkMgr=manager;ok=true;
done:return NativeWorldProvider_end(p,&scope,ok);
}
bool WorldProvider_registerWorldChunkManager(WorldProvider *p) {
    MCObjectRootScope scope={0};if(!NativeWorldProvider_begin(p,&scope))return false;
    bool ok=false;WorldInfo *info=world_info(p,&scope);
    WorldType *type=terrain(p,&scope,info);
    if(MCObjectHeap_failed(p->object.heap))goto done;
    WorldTypeStatics *statics=WorldType_getStatics(p->object.heap);
    if(!statics||!pin(p,&scope,(MCObject *)statics))goto done;
    const WorldProviderDependencies *d=p->dependencies;
    if(type==statics->FLAT) {
        info=world_info(p,&scope);NBTString *text=options(p,&scope,info);
        if(MCObjectHeap_failed(p->object.heap)||!d||!d->parseFlatGenerator)goto done;
        MCObject *flat=d->parseFlatGenerator(p->dependencyContext,text);
        if(!flat||!pin(p,&scope,flat)||!d->getFlatBiome)goto done;
        int32_t biomeId;if(!d->getFlatBiome(p->dependencyContext,flat,&biomeId)||MCObjectHeap_failed(p->object.heap))goto done;
        if(!d->getFallbackBiome)goto done;
        MCObject *fallback=d->getFallbackBiome(p->dependencyContext);
        if(!pin(p,&scope,fallback)||!d->getBiomeFromBiomeList)goto done;
        MCObject *biome=d->getBiomeFromBiomeList(p->dependencyContext,biomeId,fallback);
        if(!pin(p,&scope,biome))goto done;
        ok=NativeWorldProvider_assignHellManager(p,biome,0.5f);
    } else if(type==statics->DEBUG_WORLD) {
        if(!d||!d->getPlainsBiome)goto done;
        MCObject *biome=d->getPlainsBiome(p->dependencyContext);
        if(!pin(p,&scope,biome))goto done;
        ok=NativeWorldProvider_assignHellManager(p,biome,0.0f);
    } else {
        World *world=p->worldObj;
        if(!d||!d->newWorldChunkManager||!pin(p,&scope,(MCObject *)world))goto done;
        MCObject *manager=d->newWorldChunkManager(p->dependencyContext,world);
        if(!manager||!pin(p,&scope,manager))goto done;
        p->worldChunkMgr=manager;ok=true;
    }
done:return NativeWorldProvider_end(p,&scope,ok);
}
bool NativeWorldProvider_lightTable(WorldProvider *p,float minimum) {
    MCObjectRootScope scope={0};if(!NativeWorldProvider_begin(p,&scope))return false;
    bool ok=false;
    for(int32_t i=0;i<=15;i++) {
        volatile float fraction=(float)i/15.0f;
        volatile float f1=1.0f-fraction;
        NativeFloatArray *array=p->lightBrightnessTable;
        if(!NativeFloatArray_isInstance((MCObject *)array)||!pin(p,&scope,(MCObject *)array)||i>=array->length)goto done;
        volatile float numerator=1.0f-f1;
        volatile float product=f1*3.0f;
        volatile float denominator=product+1.0f;
        volatile float quotient=numerator/denominator;
        volatile float factor=1.0f-minimum;
        volatile float weighted=quotient*factor;
        array->data[i]=weighted+minimum;
    }
    ok=true;
done:return NativeWorldProvider_end(p,&scope,ok);
}
bool WorldProvider_generateLightBrightnessTable(WorldProvider *p) {return NativeWorldProvider_lightTable(p,0.0f);}
static bool valid(WorldProvider *p) {return WorldProvider_isInstance((MCObject *)p)&&!MCObjectHeap_failed(p->object.heap)?true:fail(p);}
MCObject *WorldProvider_getWorldChunkManager(WorldProvider *p) {return valid(p)?p->worldChunkMgr:NULL;}
bool WorldProvider_doesWaterVaporize(WorldProvider *p) {return valid(p)?p->isHellWorld:false;}
bool WorldProvider_getHasNoSky(WorldProvider *p) {return valid(p)?p->hasNoSky:false;}
NativeFloatArray *WorldProvider_getLightBrightnessTable(WorldProvider *p) {return valid(p)?p->lightBrightnessTable:NULL;}
int32_t WorldProvider_getDimensionId(WorldProvider *p) {return valid(p)?p->dimensionId:0;}
WorldBorder *WorldProvider_getWorldBorder(WorldProvider *p) {
    MCObjectRootScope scope={0};if(!NativeWorldProvider_begin(p,&scope))return NULL;
    WorldBorder *border=WorldProviderHell_isInstance((MCObject *)p)?WorldProviderHell_getWorldBorder((WorldProviderHell *)p):
        WorldBorder_new(p->object.heap,p->dependencies?p->dependencies->borderDependencies:NULL,p->dependencyContext);
    bool ok=NativeWorldProvider_end(p,&scope,border!=NULL);return ok?border:NULL;
}
NBTString *WorldProvider_getDimensionName(WorldProvider *p) {
    if(!valid(p))return NULL;
    const char *text=WorldProviderHell_isInstance((MCObject *)p)?"Nether":WorldProviderEnd_isInstance((MCObject *)p)?"The End":WorldProviderSurface_isInstance((MCObject *)p)?"Overworld":NULL;
    if(!text){fail(p);return NULL;}return NBTString_literalASCII(p->object.heap,text);
}
NBTString *WorldProvider_getInternalNameSuffix(WorldProvider *p) {
    if(!valid(p))return NULL;
    const char *text=WorldProviderHell_isInstance((MCObject *)p)?"_nether":WorldProviderEnd_isInstance((MCObject *)p)?"_end":WorldProviderSurface_isInstance((MCObject *)p)?"":NULL;
    if(!text){fail(p);return NULL;}return NBTString_literalASCII(p->object.heap,text);
}
float WorldProvider_calculateCelestialAngle_base(WorldProvider *p,int64_t time,float partial) {
    MCObjectRootScope scope={0};if(!NativeWorldProvider_begin(p,&scope))return NAN;
    float result=NAN;bool ok=false;
    int32_t dayTime=(int32_t)(time%INT64_C(24000));
    volatile float sum=(float)dayTime+partial;
    volatile float quotient=sum/24000.0f;
    float angle=quotient-0.25f;
    if(angle<0.0f)angle=angle+1.0f;
    if(angle>1.0f)angle=angle-1.0f;
    /* The unchanged target bytecode retains this pre-cos local. */
    float original=angle;
    volatile double argument=(double)angle*3.141592653589793;
    double cosine;const WorldProviderDependencies *d=p->dependencies;
    ok=d&&d->mathCos?d->mathCos(p->dependencyContext,argument,&cosine):NativeMathCos_cos(argument,&cosine);
    if(!ok||MCObjectHeap_failed(p->object.heap))goto done;
    volatile double shifted=cosine+1.0;
    volatile double halved=shifted/2.0;
    volatile float narrowed=(float)halved;
    volatile float transformed=1.0f-narrowed;
    volatile float difference=transformed-original;
    volatile float smoothed=difference/3.0f;
    result=original+smoothed;
done:if(!NativeWorldProvider_end(p,&scope,ok))result=NAN;
    return result;
}
float WorldProvider_calculateCelestialAngle(WorldProvider *p,int64_t time,float partial) {
    MCObjectRootScope scope={0};if(!NativeWorldProvider_begin(p,&scope))return NAN;
    const WorldProviderDependencies *d=p->dependencies;float result=NAN;bool ok=true;
    if(d&&d->calculateCelestialAngle)ok=d->calculateCelestialAngle(p->dependencyContext,p,time,partial,&result);
    else if(WorldProviderHell_isInstance((MCObject *)p))result=WorldProviderHell_calculateCelestialAngle((WorldProviderHell *)p,time,partial);
    else if(WorldProviderEnd_isInstance((MCObject *)p))result=WorldProviderEnd_calculateCelestialAngle((WorldProviderEnd *)p,time,partial);
    else result=WorldProvider_calculateCelestialAngle_base(p,time,partial);
    if(!NativeWorldProvider_end(p,&scope,ok))result=NAN;
    return result;
}
