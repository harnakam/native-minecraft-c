#include "world/WorldProviderEnd.h"
#include "world/WorldProviderInternal.h"
#include <math.h>
static void trace(MCObject *object,MCObjectVisitor visit,void *ctx) {WorldProvider_traceFields((WorldProvider *)object,visit,ctx);}
static const MCObjectClass klass={"net.minecraft.world.WorldProviderEnd",MCObjectHeap_plainClone,trace,NULL};
bool WorldProviderEnd_isInstance(const MCObject *o) {return o&&o->klass==&klass&&MCObjectHeap_objectSize(o)>=sizeof(WorldProviderEnd);}
WorldProviderEnd *WorldProviderEnd_nativeAllocate(MCObjectHeap *heap,const WorldProviderDependencies *deps,MCObject *ctx) {
    return (WorldProviderEnd *)NativeWorldProvider_allocate(heap,sizeof(WorldProviderEnd),&klass,deps,ctx);
}
bool WorldProviderEnd_construct(WorldProviderEnd *p) {
    if(!WorldProviderEnd_isInstance((MCObject *)p)){MCObjectHeap_fail(p?p->provider.object.heap:NULL);return false;}
    return WorldProvider_construct((WorldProvider *)p);
}
WorldProviderEnd *WorldProviderEnd_new(MCObjectHeap *heap,const WorldProviderDependencies *deps,MCObject *ctx) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    WorldProviderEnd *p=WorldProviderEnd_nativeAllocate(heap,deps,ctx);
    if(!p||!MCObjectRootScope_pin(&scope,(MCObject *)p)||!WorldProviderEnd_construct(p))p=NULL;
    MCObjectRootScope_end(&scope);return p;
}
bool WorldProviderEnd_registerWorldChunkManager(WorldProviderEnd *self) {
    if(!WorldProviderEnd_isInstance((MCObject *)self)){MCObjectHeap_fail(self?self->provider.object.heap:NULL);return false;}
    WorldProvider *p=(WorldProvider *)self;MCObjectRootScope scope={0};if(!NativeWorldProvider_begin(p,&scope))return false;
    const WorldProviderDependencies *d=p->dependencies;bool ok=false;
    if(!d||!d->getSkyBiome)goto done;
    MCObject *biome=d->getSkyBiome(p->dependencyContext);
    if(MCObjectHeap_failed(p->object.heap)||!NativeWorldProvider_assignHellManager(p,biome,0.0f))goto done;
    p->dimensionId=1;
    p->hasNoSky=true;ok=true;
done:return NativeWorldProvider_end(p,&scope,ok);
}
float WorldProviderEnd_calculateCelestialAngle(WorldProviderEnd *p,int64_t time,float partial) {
    (void)time;(void)partial;
    if(!WorldProviderEnd_isInstance((MCObject *)p)||MCObjectHeap_failed(p->provider.object.heap)) {
        MCObjectHeap_fail(p?p->provider.object.heap:NULL);return NAN;
    }
    return 0.0f;
}
