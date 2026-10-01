#ifndef C919_SOURCE_WORLD_PROVIDER_SURFACE_H
#define C919_SOURCE_WORLD_PROVIDER_SURFACE_H
#include "world/WorldProvider.h"
typedef struct WorldProviderSurface { WorldProvider provider; } WorldProviderSurface;
bool WorldProviderSurface_isInstance(const MCObject *);
WorldProviderSurface *WorldProviderSurface_nativeAllocate(MCObjectHeap *,const WorldProviderDependencies *,MCObject *);
bool WorldProviderSurface_construct(WorldProviderSurface *);
WorldProviderSurface *WorldProviderSurface_new(MCObjectHeap *,const WorldProviderDependencies *,MCObject *);
#endif
