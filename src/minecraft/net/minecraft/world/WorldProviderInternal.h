#ifndef C919_NATIVE_WORLD_PROVIDER_INTERNAL_H
#define C919_NATIVE_WORLD_PROVIDER_INTERNAL_H
#include "world/WorldProvider.h"
bool NativeWorldProvider_begin(WorldProvider *,MCObjectRootScope *);
bool NativeWorldProvider_end(WorldProvider *,MCObjectRootScope *,bool);
WorldProvider *NativeWorldProvider_allocate(MCObjectHeap *,size_t,const MCObjectClass *,const WorldProviderDependencies *,MCObject *);
bool NativeWorldProvider_lightTable(WorldProvider *,float minimum);
bool NativeWorldProvider_assignHellManager(WorldProvider *,MCObject *biome,float rainfall);
#endif
