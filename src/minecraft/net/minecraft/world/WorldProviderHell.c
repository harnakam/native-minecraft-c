#include "world/WorldProviderHell.h"
#include "world/WorldProviderInternal.h"
#include <math.h>
static void trace(MCObject *object,MCObjectVisitor visit,void *ctx) {WorldProvider_traceFields((WorldProvider *)object,visit,ctx);}
static const MCObjectClass klass={"net.minecraft.world.WorldProviderHell",MCObjectHeap_plainClone,trace,NULL};
bool WorldProviderHell_isInstance(const MCObject *o) {return o&&o->klass==&klass&&MCObjectHeap_objectSize(o)>=sizeof(WorldProviderHell);}
WorldProviderHell *WorldProviderHell_nativeAllocate(MCObjectHeap *heap,const WorldProviderDependencies *deps,MCObject *ctx) {
    return (WorldProviderHell *)NativeWorldProvider_allocate(heap,sizeof(WorldProviderHell),&klass,deps,ctx);
}
bool WorldProviderHell_construct(WorldProviderHell *p) {
    if(!WorldProviderHell_isInstance((MCObject *)p)){MCObjectHeap_fail(p?p->provider.object.heap:NULL);return false;}
    return WorldProvider_construct((WorldProvider *)p);
}
WorldProviderHell *WorldProviderHell_new(MCObjectHeap *heap,const WorldProviderDependencies *deps,MCObject *ctx) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    WorldProviderHell *p=WorldProviderHell_nativeAllocate(heap,deps,ctx);
    if(!p||!MCObjectRootScope_pin(&scope,(MCObject *)p)||!WorldProviderHell_construct(p))p=NULL;
    MCObjectRootScope_end(&scope);return p;
}
bool WorldProviderHell_registerWorldChunkManager(WorldProviderHell *self) {
    if(!WorldProviderHell_isInstance((MCObject *)self)){MCObjectHeap_fail(self?self->provider.object.heap:NULL);return false;}
    WorldProvider *p=(WorldProvider *)self;MCObjectRootScope scope={0};if(!NativeWorldProvider_begin(p,&scope))return false;
    const WorldProviderDependencies *d=p->dependencies;bool ok=false;
    if(!d||!d->getHellBiome)goto done;
    MCObject *biome=d->getHellBiome(p->dependencyContext);
    if(MCObjectHeap_failed(p->object.heap)||!NativeWorldProvider_assignHellManager(p,biome,0.0f))goto done;
    p->isHellWorld=true;
    p->hasNoSky=true;
    p->dimensionId=-1;ok=true;
done:return NativeWorldProvider_end(p,&scope,ok);
}
bool WorldProviderHell_generateLightBrightnessTable(WorldProviderHell *p) {
    if(!WorldProviderHell_isInstance((MCObject *)p)){MCObjectHeap_fail(p?p->provider.object.heap:NULL);return false;}
    return NativeWorldProvider_lightTable((WorldProvider *)p,0.1f);
}
static void border_trace(MCObject *object,MCObjectVisitor visit,void *ctx) {
    WorldProviderHellBorder *b=(WorldProviderHellBorder *)object;
    if(MCObjectHeap_objectSize(object)<sizeof(*b)){MCObjectHeap_fail(object->heap);return;}
    WorldBorder_traceFields(&b->border,visit,ctx);
    b->this_0=(WorldProviderHell *)visit((MCObject *)b->this_0,ctx);
}
static const MCObjectClass borderClass={"net.minecraft.world.WorldProviderHell$1",MCObjectHeap_plainClone,border_trace,NULL};
static const WorldBorderOverrides overrides={.getCenterX=WorldProviderHellBorder_getCenterX,.getCenterZ=WorldProviderHellBorder_getCenterZ};
bool WorldProviderHellBorder_isInstance(const MCObject *o) {return o&&o->klass==&borderClass&&MCObjectHeap_objectSize(o)>=sizeof(WorldProviderHellBorder);}
WorldProviderHellBorder *WorldProviderHellBorder_nativeAllocate(MCObjectHeap *heap) {
    WorldProviderHellBorder *b=(WorldProviderHellBorder *)MCObjectHeap_alloc(heap,sizeof(*b),&borderClass);
    if(b)b->border.overrides=&overrides;
    return b;
}
bool WorldProviderHellBorder_construct(WorldProviderHellBorder *b,WorldProviderHell *outer,const WorldBorderDependencies *deps,MCObject *ctx) {
    MCObjectHeap *heap=b?b->border.object.heap:NULL;MCObjectRootScope scope={0};
    if(!WorldProviderHellBorder_isInstance((MCObject *)b)||!MCObjectRootScope_begin(&scope,heap)) {MCObjectHeap_fail(heap);return false;}
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)b)&&MCObjectRootScope_pin(&scope,(MCObject *)outer)&&MCObjectRootScope_pin(&scope,ctx);
    if(ok&&outer&&!WorldProviderHell_isInstance((MCObject *)outer))ok=false;
    if(ok){b->this_0=outer;b->border.dependencies=deps;b->border.dependencyContext=ctx;ok=WorldBorder_construct(&b->border);}
    if(!ok)MCObjectHeap_fail(heap);
    MCObjectRootScope_end(&scope);return ok;
}
double WorldProviderHellBorder_getCenterX(WorldBorder *b) {
    if(!WorldProviderHellBorder_isInstance((MCObject *)b)){MCObjectHeap_fail(b?b->object.heap:NULL);return NAN;}
    return WorldBorder_getCenterXBase(b)/8.0;
}
double WorldProviderHellBorder_getCenterZ(WorldBorder *b) {
    if(!WorldProviderHellBorder_isInstance((MCObject *)b)){MCObjectHeap_fail(b?b->object.heap:NULL);return NAN;}
    return WorldBorder_getCenterZBase(b)/8.0;
}
WorldBorder *WorldProviderHell_getWorldBorder(WorldProviderHell *self) {
    if(!WorldProviderHell_isInstance((MCObject *)self)){MCObjectHeap_fail(self?self->provider.object.heap:NULL);return NULL;}
    WorldProvider *p=(WorldProvider *)self;MCObjectRootScope scope={0};if(!NativeWorldProvider_begin(p,&scope))return NULL;
    WorldProviderHellBorder *b=WorldProviderHellBorder_nativeAllocate(p->object.heap);
    bool ok=b&&MCObjectRootScope_pin(&scope,(MCObject *)b)&&WorldProviderHellBorder_construct(b,self,p->dependencies?p->dependencies->borderDependencies:NULL,p->dependencyContext);
    ok=NativeWorldProvider_end(p,&scope,ok);return ok?&b->border:NULL;
}
