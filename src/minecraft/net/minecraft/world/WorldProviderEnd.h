#ifndef C919_SOURCE_WORLD_PROVIDER_END_H
#define C919_SOURCE_WORLD_PROVIDER_END_H
#include "world/WorldProvider.h"
typedef struct WorldProviderEnd { WorldProvider provider; } WorldProviderEnd;
bool WorldProviderEnd_isInstance(const MCObject *);
WorldProviderEnd *WorldProviderEnd_nativeAllocate(MCObjectHeap *,const WorldProviderDependencies *,MCObject *);
bool WorldProviderEnd_construct(WorldProviderEnd *);
WorldProviderEnd *WorldProviderEnd_new(MCObjectHeap *,const WorldProviderDependencies *,MCObject *);
bool WorldProviderEnd_registerWorldChunkManager(WorldProviderEnd *);
#endif
