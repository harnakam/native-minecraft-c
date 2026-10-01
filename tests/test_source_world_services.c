#include "world/World.h"
#include "world/WorldProviderHell.h"
#include "world/WorldProviderEnd.h"
#include "world/EnumSkyBlock.h"
#include "util/NativeMathCos.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fenv.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"World services check %u line %d\n",checks,__LINE__);exit(1);}}while(0)
static uint32_t bits(float v){uint32_t out;memcpy(&out,&v,4);return out;}
static World *world(MCObjectHeap *h){World *w=World_nativeAllocate(h,NULL,NULL);CHECK(w);return w;}
static WorldProvider *provider(MCObjectHeap *h){WorldProvider *p=WorldProvider_nativeAllocate(h,NULL,NULL);CHECK(p);CHECK(WorldProvider_construct(p));return p;}
static WorldInfo *info(MCObjectHeap *h){WorldInfo *i=WorldInfo_nativeAllocate(h,NULL,NULL);CHECK(i);CHECK(WorldInfo_construct(i));return i;}
static void first_source_phase(void){
 MCObjectHeap *h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);
 World *w=world(h);w->provider=provider(h);w->worldInfo=info(h);w->worldInfo->worldTime=6000;
 /* Noon must retain the pre-cos local; a missing body or decompiler's
    self-subtraction cannot satisfy the ordinary midnight comparison below. */
 CHECK(bits(World_getCelestialAngle(w,0.0f))==UINT32_C(0x00000000));
 w->worldInfo->worldTime=18000;
 CHECK(bits(World_getCelestialAngle(w,0.0f))==UINT32_C(0x3f000000));
 CHECK(World_calculateSkylightSubtracted(w,1.0f)==11);
 CHECK(World_calculateInitialSkylight(w));CHECK(w->skylightSubtracted==11);
 w->worldInfo->raining=true;w->worldInfo->thundering=true;
 CHECK(World_calculateInitialWeather(w));CHECK(w->rainingStrength==1.0f&&w->thunderingStrength==1.0f);
 CHECK(w->prevRainingStrength==0.0f&&w->prevThunderingStrength==0.0f);
 CHECK(!MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
static void numerical_dependency(void){
 double x,y;uint64_t input=UINT64_C(0x4002e67d03a889d7),want=UINT64_C(0xbfe6c546ebf0093c),got;
 memcpy(&x,&input,8);CHECK(NativeMathCos_cos(x,&y));memcpy(&got,&y,8);CHECK(got==want);
 CHECK(!NativeMathCos_cos(1.0,NULL));
 y=42.0;CHECK(fesetround(FE_DOWNWARD)==0);CHECK(!NativeMathCos_cos(1.0,&y));CHECK(y==42.0);CHECK(fesetround(FE_TONEAREST)==0);
 CHECK(NativeMathCos_cos(-0.0,&y)&&y==1.0);
 CHECK(NativeMathCos_cos(INFINITY,&y)&&isnan(y));
}
typedef struct {
 MCObject object;World *w;WorldInfo *i[2];WorldProvider *p[2];
 unsigned calls,failAt,swap;char events[96];
 bool fixedAngle,changeThunder,changeSky,failCos;
 int64_t timeSeen;float partialSeen;double cosineArgument;
} Witness;
static void witness_trace(MCObject *object,MCObjectVisitor v,void *c){
 Witness *x=(Witness *)object;x->w=(World *)v((MCObject *)x->w,c);
 for(unsigned j=0;j<2;j++){x->i[j]=(WorldInfo *)v((MCObject *)x->i[j],c);x->p[j]=(WorldProvider *)v((MCObject *)x->p[j],c);}
}
static const MCObjectClass witnessClass={"test.WorldServicesWitness",MCObjectHeap_plainClone,witness_trace,NULL};
static bool event(Witness *x,char code){
 CHECK(MCObjectHeap_hasBorrowers(x->object.heap));CHECK(x->calls+1<sizeof x->events);
 x->events[x->calls++]=code;x->events[x->calls]=0;
 if(x->calls==x->failAt){x->w->skylightSubtracted=(int32_t)(100+x->calls);MCObjectHeap_fail(x->object.heap);return false;}return true;
}
static int64_t time_getter(MCObject *o,WorldInfo *i){
 Witness *x=(Witness *)o;if(!event(x,'t'))return 0;
 if(x->swap==1){x->w->provider=x->p[1];x->w->worldInfo=x->i[1];}
 return i->worldTime;
}
static bool raining_getter(MCObject *o,WorldInfo *i){
 Witness *x=(Witness *)o;if(!event(x,'f'))return false;
 if(x->swap==2)x->w->worldInfo=x->i[1];
 if(x->swap==3)x->w->worldInfo=NULL;
 return i->raining;
}
static bool thundering_getter(MCObject *o,WorldInfo *i){Witness *x=(Witness *)o;if(!event(x,'g'))return false;return i->thundering;}
static const WorldInfoVirtualMethods infoVirtual={.getWorldTime=time_getter,.isRaining=raining_getter,.isThundering=thundering_getter};
static bool provider_virtual(MCObject *o,WorldProvider *p,int64_t t,float partial,float *out){
 Witness *x=(Witness *)o;if(!event(x,p==x->p[0]?'p':'q'))return false;
 x->timeSeen=t;x->partialSeen=partial;
 *out=x->fixedAngle?0.25f:WorldProvider_calculateCelestialAngle_base(p,t,partial);
 return !MCObjectHeap_failed(o->heap);
}
static bool cosine_dependency(MCObject *o,double argument,double *out){
 Witness *x=(Witness *)o;CHECK(MCObjectHeap_hasBorrowers(o->heap));x->cosineArgument=argument;
 if(x->failCos){x->p[0]->dimensionId=42;return false;}return NativeMathCos_cos(argument,out);
}
static const WorldProviderDependencies providerVirtual={.calculateCelestialAngle=provider_virtual,.mathCos=cosine_dependency};
static bool rain_virtual(MCObject *o,World *w,float partial,float *out){
 Witness *x=(Witness *)o;if(!event(x,'r'))return false;
 if(x->changeThunder){w->prevThunderingStrength=20.0f;w->thunderingStrength=40.0f;*out=0.75f;}
 else *out=World_getRainStrength_base(w,partial);
 return !MCObjectHeap_failed(o->heap);
}
static bool thunder_virtual(MCObject *o,World *w,float partial,float *out){Witness *x=(Witness *)o;if(!event(x,'h'))return false;*out=World_getThunderStrength_base(w,partial);return !MCObjectHeap_failed(o->heap);}
static bool skylight_virtual(MCObject *o,World *w,float partial,int32_t *out){
 Witness *x=(Witness *)o;if(!event(x,'s'))return false;
 if(x->changeSky){CHECK(partial==1.0f);w->skylightSubtracted=20;*out=7;}
 else *out=World_calculateSkylightSubtracted_base(w,partial);
 return !MCObjectHeap_failed(o->heap);
}
static const WorldDependencies worldVirtual={.getRainStrength=rain_virtual,.getThunderStrength=thunder_virtual,.calculateSkylightSubtracted=skylight_virtual};
static Witness *witness(MCObjectHeap *h){
 Witness *x=(Witness *)MCObjectHeap_alloc(h,sizeof(*x),&witnessClass);CHECK(x);
 x->w=World_nativeAllocate(h,&worldVirtual,(MCObject *)x);CHECK(x->w);
 for(unsigned j=0;j<2;j++){
  x->i[j]=WorldInfo_nativeAllocate(h,&infoVirtual,(MCObject *)x);CHECK(x->i[j]&&WorldInfo_construct(x->i[j]));
  x->p[j]=WorldProvider_nativeAllocate(h,&providerVirtual,(MCObject *)x);CHECK(x->p[j]&&WorldProvider_construct(x->p[j]));
 }
 x->w->provider=x->p[0];x->w->worldInfo=x->i[0];x->i[0]->worldTime=18000;x->i[1]->worldTime=6000;
 x->w->prevRainingStrength=0.125f;x->w->rainingStrength=0.625f;
 x->w->prevThunderingStrength=0.25f;x->w->thunderingStrength=0.75f;return x;
}
static void provider_capture_and_virtual_order(void){
 MCObjectHeap *h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);Witness *x=witness(h);x->swap=1;
 CHECK(bits(World_getCelestialAngle(x->w,0.0f))==UINT32_C(0x3f000000));
 CHECK(!strcmp(x->events,"tp"));CHECK(x->timeSeen==18000&&x->partialSeen==0.0f);
 CHECK(x->w->provider==x->p[1]&&x->w->worldInfo==x->i[1]);
 MCObjectHeap_free(h);
 h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);x=witness(h);x->fixedAngle=true;
 CHECK(World_calculateSkylightSubtracted(x->w,0.5f)==6);CHECK(!strcmp(x->events,"stprhr"));
 x->calls=0;x->events[0]=0;x->changeThunder=true;
 CHECK(World_getThunderStrength(x->w,0.5f)==0.375f);
 CHECK(x->w->thunderingStrength==40.0f);CHECK(!strcmp(x->events,"hr"));
 MCObjectHeap_free(h);
}
static void interpolation_and_initial_fields(void){
 MCObjectHeap *h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);World *w=world(h);
 w->prevRainingStrength=0.25f;w->rainingStrength=0.75f;w->prevThunderingStrength=0.25f;w->thunderingStrength=0.75f;
 CHECK(World_getRainStrength(w,0.5f)==0.5f);CHECK(World_getRainStrength(w,2.0f)==1.25f);
 CHECK(World_getRainStrength(w,-1.0f)==-0.25f);CHECK(World_getThunderStrength(w,0.5f)==0.25f);
 CHECK(isnan(World_getThunderStrength(w,NAN)));CHECK(!MCObjectHeap_failed(h));MCObjectHeap_free(h);
 for(unsigned r=0;r<2;r++)for(unsigned t=0;t<2;t++){
  h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);Witness *x=witness(h);w=x->w;x->i[0]->raining=r!=0;x->i[0]->thundering=t!=0;
  CHECK(World_calculateInitialWeather(w));CHECK(w->rainingStrength==(r?1.0f:0.625f));CHECK(w->thunderingStrength==(r&&t?1.0f:0.75f));
  CHECK(w->prevRainingStrength==0.125f&&w->prevThunderingStrength==0.25f);CHECK(!strcmp(x->events,r?"fg":"f"));MCObjectHeap_free(h);
 }
 h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);Witness *x=witness(h);x->i[0]->raining=true;x->i[0]->thundering=false;x->i[1]->thundering=true;x->swap=2;
 CHECK(World_calculateInitialWeather(x->w));CHECK(x->w->rainingStrength==1.0f&&x->w->thunderingStrength==1.0f);CHECK(x->w->worldInfo==x->i[1]);CHECK(!strcmp(x->events,"fg"));MCObjectHeap_free(h);
 h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);x=witness(h);x->changeSky=true;
 CHECK(World_calculateInitialSkylight(x->w));CHECK(x->w->skylightSubtracted==7);CHECK(!strcmp(x->events,"s"));MCObjectHeap_free(h);
}
static void failures_retain_source_prefix(void){
 for(unsigned failed=1;failed<=6;failed++){
  MCObjectHeap *h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);Witness *x=witness(h);x->fixedAngle=true;x->failAt=failed;
  CHECK(!World_calculateInitialSkylight(x->w));CHECK(MCObjectHeap_failed(h));CHECK(x->calls==failed);
  CHECK(x->w->skylightSubtracted==(int32_t)(100+failed));CHECK(!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);
 }
 for(unsigned failed=1;failed<=2;failed++){
  MCObjectHeap *h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);Witness *x=witness(h);x->i[0]->raining=true;x->i[0]->thundering=true;x->failAt=failed;
  CHECK(!World_calculateInitialWeather(x->w));CHECK(x->w->rainingStrength==(failed==1?0.625f:1.0f));CHECK(x->w->thunderingStrength==0.75f);CHECK(x->calls==failed);MCObjectHeap_free(h);
 }
 MCObjectHeap *h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);Witness *x=witness(h);x->i[0]->raining=true;x->swap=3;
 CHECK(!World_calculateInitialWeather(x->w));CHECK(x->w->rainingStrength==1.0f);CHECK(x->w->thunderingStrength==0.75f);CHECK(!strcmp(x->events,"f"));MCObjectHeap_free(h);
 h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);x=witness(h);x->w->provider=NULL;
 CHECK(isnan(World_getCelestialAngle(x->w,0)));CHECK(MCObjectHeap_failed(h));CHECK(!strcmp(x->events,"t"));MCObjectHeap_free(h);
 h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);x=witness(h);x->failCos=true;
 CHECK(isnan(WorldProvider_calculateCelestialAngle_base(x->p[0],0,0)));CHECK(MCObjectHeap_failed(h));CHECK(x->p[0]->dimensionId==42);CHECK(x->cosineArgument==2.356194490192345);MCObjectHeap_free(h);
}
static void derived_constants_and_native_guards(void){
 MCObjectHeap *h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);Witness *x=witness(h);x->failCos=true;
 const WorldProviderDependencies cosOnly={.mathCos=cosine_dependency};
 WorldProvider *hell=(WorldProvider *)WorldProviderHell_new(h,&cosOnly,(MCObject *)x);CHECK(hell);
 WorldProvider *end=(WorldProvider *)WorldProviderEnd_new(h,&cosOnly,(MCObject *)x);CHECK(end);
 CHECK(WorldProvider_calculateCelestialAngle(hell,INT64_MIN,NAN)==0.5f);
 CHECK(bits(WorldProvider_calculateCelestialAngle(end,INT64_MAX,INFINITY))==0);CHECK(!MCObjectHeap_failed(h));
 MCObject *tiny=MCObjectHeap_alloc(h,sizeof(MCObject),x->p[0]->object.klass);CHECK(tiny);
 CHECK(isnan(WorldProvider_calculateCelestialAngle((WorldProvider *)tiny,0,0)));CHECK(MCObjectHeap_failed(h));MCObjectHeap_free(h);
 h=MCObjectHeap_new(16u*1024u*1024u);MCObjectHeap *foreign=MCObjectHeap_new(16u*1024u*1024u);CHECK(h&&foreign);x=witness(h);x->w->worldInfo=info(foreign);
 CHECK(isnan(World_getCelestialAngle(x->w,0)));CHECK(MCObjectHeap_failed(h));CHECK(!MCObjectHeap_failed(foreign));MCObjectHeap_free(h);MCObjectHeap_free(foreign);
}
static void graph_alias_clone_adopt_collection(void){
 MCObjectHeap *h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);Witness *x=witness(h);MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)x->w));
 CHECK(World_getRainStrength(x->w,0.5f)==0.375f);CHECK(!MCObjectHeap_hasBorrowers(h));CHECK(MCObjectHeap_collect(h));
 MCObjectHeap *working=MCObjectHeap_clone(h);CHECK(working);MCObjectRoot wr={0};CHECK(MCObjectRoot_rebind(&wr,working,&root));
 World *w=(World *)MCObjectRoot_get(&wr);Witness *copy=(Witness *)w->dependencyContext;CHECK(copy!=x&&copy->w==w);
 CHECK(w->provider==copy->p[0]&&w->worldInfo==copy->i[0]);CHECK(w->provider->dependencyContext==(MCObject *)copy&&copy->i[0]->virtualContext==(MCObject *)copy);
 copy->fixedAngle=true;w->rainingStrength=0.875f;CHECK(World_getRainStrength(w,0.5f)==0.5f);CHECK(x->w->rainingStrength==0.625f);
 CHECK(MCObjectHeap_canAdopt(h,working));CHECK(MCObjectHeap_adopt(h,working));MCObjectHeap_free(working);wr=(MCObjectRoot){0};
 w=(World *)MCObjectRoot_get(&root);copy=(Witness *)w->dependencyContext;CHECK(copy->w==w&&w->provider==copy->p[0]);CHECK(MCObjectHeap_collect(h));CHECK(World_calculateInitialSkylight(w));
 CHECK(!MCObjectHeap_failed(h));MCObjectRoot_drop(&root);MCObjectHeap_free(h);
}
static void world_type_and_light_statics(void){
 MCObjectHeap *h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);World *w=world(h);w->worldInfo=info(h);
 WorldTypeStatics *types=WorldType_getStatics(h);CHECK(types);w->worldInfo->terrainType=types->FLAT;
 CHECK(World_getWorldType(w)==types->FLAT);
 EnumSkyBlockStatics *s=EnumSkyBlock_getStatics(h);CHECK(s&&s->SKY&&s->BLOCK);
 CHECK(s->SKY!=s->BLOCK&&EnumSkyBlock_isInstance((MCObject *)s->SKY));
 CHECK(s->SKY->defaultLightValue==15&&s->BLOCK->defaultLightValue==0);
 CHECK(EnumSkyBlock_getStatics(h)==s&&!MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
static WorldType *terrain_getter(MCObject *o,WorldInfo *i){
 Witness *x=(Witness *)o;if(!event(x,'y'))return NULL;
 if(x->swap==4)x->w->worldInfo=x->i[1];
 return i->terrainType;
}
static WorldInfo *unused_info_getter(MCObject *o,World *w){
 Witness *x=(Witness *)o;(void)w;event(x,'X');return x->i[1];
}
static const WorldInfoVirtualMethods terrainVirtual={.getTerrainType=terrain_getter};
static const WorldDependencies worldInfoVirtual={.getWorldInfo=unused_info_getter};
static void world_type_direct_receiver_and_failure(void){
 MCObjectHeap *h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);Witness *x=witness(h);
 WorldTypeStatics *types=WorldType_getStatics(h);CHECK(types);x->i[0]->terrainType=types->FLAT;x->i[1]->terrainType=types->AMPLIFIED;
 x->i[0]->virtualMethods=&terrainVirtual;x->w->dependencies=&worldInfoVirtual;x->swap=4;
 CHECK(World_getWorldType(x->w)==types->FLAT);CHECK(x->w->worldInfo==x->i[1]);CHECK(!strcmp(x->events,"y"));
 x->w->worldInfo=x->i[0];x->i[0]->terrainType=NULL;x->swap=0;
 CHECK(!World_getWorldType(x->w)&&!MCObjectHeap_failed(h));CHECK(!strcmp(x->events,"yy"));MCObjectHeap_free(h);
 h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);x=witness(h);x->i[0]->virtualMethods=&terrainVirtual;x->failAt=1;
 CHECK(!World_getWorldType(x->w));CHECK(MCObjectHeap_failed(h));CHECK(x->w->skylightSubtracted==101);CHECK(!strcmp(x->events,"y"));
 CHECK(!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);
 h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);x=witness(h);x->w->worldInfo=NULL;
 CHECK(!World_getWorldType(x->w)&&MCObjectHeap_failed(h));CHECK(x->calls==0);MCObjectHeap_free(h);
 h=MCObjectHeap_new(16u*1024u*1024u);MCObjectHeap *foreign=MCObjectHeap_new(16u*1024u*1024u);CHECK(h&&foreign);x=witness(h);
 types=WorldType_getStatics(foreign);CHECK(types);x->i[0]->terrainType=types->FLAT;
 CHECK(!World_getWorldType(x->w)&&MCObjectHeap_failed(h));CHECK(!MCObjectHeap_failed(foreign));MCObjectHeap_free(h);MCObjectHeap_free(foreign);
}
static void enum_static_graph_and_native_guards(void){
 MCObjectHeap *h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);EnumSkyBlockStatics *s=EnumSkyBlock_getStatics(h);CHECK(s);
 EnumSkyBlock *sky=s->SKY,*block=s->BLOCK;CHECK(MCObjectHeap_collect(h));CHECK(EnumSkyBlock_getStatics(h)==s&&s->SKY==sky&&s->BLOCK==block);
 MCObjectHeap *copy=MCObjectHeap_clone(h);CHECK(copy);EnumSkyBlockStatics *cs=EnumSkyBlock_getStatics(copy);CHECK(cs&&cs!=s);
 CHECK(cs->SKY!=sky&&cs->BLOCK!=block&&cs->SKY!=cs->BLOCK);CHECK(cs->SKY->defaultLightValue==15&&cs->BLOCK->defaultLightValue==0);
 CHECK(MCObjectHeap_canAdopt(h,copy));CHECK(MCObjectHeap_adopt(h,copy));MCObjectHeap_free(copy);s=EnumSkyBlock_getStatics(h);CHECK(s&&s->SKY->defaultLightValue==15);
 CHECK(MCObjectHeap_collect(h));CHECK(EnumSkyBlock_getStatics(h)==s);CHECK(!MCObjectHeap_failed(h));MCObjectHeap_free(h);
 h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);EnumSkyBlock *custom=EnumSkyBlock_nativeAllocate(h);CHECK(custom);CHECK(EnumSkyBlock_construct(custom,INT32_MIN));CHECK(custom->defaultLightValue==INT32_MIN);
 MCObject *tiny=MCObjectHeap_alloc(h,sizeof(MCObject),custom->object.klass);CHECK(tiny);CHECK(!EnumSkyBlock_isInstance(tiny));CHECK(!EnumSkyBlock_construct((EnumSkyBlock *)tiny,15)&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
 CHECK(!EnumSkyBlock_getStatics(NULL)&&!EnumSkyBlock_nativeAllocate(NULL)&&!EnumSkyBlock_construct(NULL,15));
 h=MCObjectHeap_new(1);CHECK(h);CHECK(!EnumSkyBlock_getStatics(h)&&MCObjectHeap_failed(h));CHECK(!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);
}
int main(void){world_type_and_light_statics();world_type_direct_receiver_and_failure();enum_static_graph_and_native_guards();numerical_dependency();first_source_phase();provider_capture_and_virtual_order();interpolation_and_initial_fields();failures_retain_source_prefix();derived_constants_and_native_guards();graph_alias_clone_adopt_collection();printf("Source world services checks %u\n",checks);return 0;}
