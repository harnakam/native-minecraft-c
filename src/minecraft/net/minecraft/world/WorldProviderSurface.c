#include "world/WorldProviderSurface.h"
#include "world/WorldProviderInternal.h"
static void trace(MCObject *object,MCObjectVisitor visit,void *ctx) {WorldProvider_traceFields((WorldProvider *)object,visit,ctx);}
static const MCObjectClass klass={"net.minecraft.world.WorldProviderSurface",MCObjectHeap_plainClone,trace,NULL};
bool WorldProviderSurface_isInstance(const MCObject *o) {return o&&o->klass==&klass&&MCObjectHeap_objectSize(o)>=sizeof(WorldProviderSurface);}
WorldProviderSurface *WorldProviderSurface_nativeAllocate(MCObjectHeap *heap,const WorldProviderDependencies *deps,MCObject *ctx) {
    return (WorldProviderSurface *)NativeWorldProvider_allocate(heap,sizeof(WorldProviderSurface),&klass,deps,ctx);
}
bool WorldProviderSurface_construct(WorldProviderSurface *p) {
    if(!WorldProviderSurface_isInstance((MCObject *)p)){MCObjectHeap_fail(p?p->provider.object.heap:NULL);return false;}
    return WorldProvider_construct((WorldProvider *)p);
}
WorldProviderSurface *WorldProviderSurface_new(MCObjectHeap *heap,const WorldProviderDependencies *deps,MCObject *ctx) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    WorldProviderSurface *p=WorldProviderSurface_nativeAllocate(heap,deps,ctx);
    if(!p||!MCObjectRootScope_pin(&scope,(MCObject *)p)||!WorldProviderSurface_construct(p))p=NULL;
    MCObjectRootScope_end(&scope);return p;
}
