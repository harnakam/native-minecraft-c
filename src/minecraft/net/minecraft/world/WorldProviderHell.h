#ifndef C919_SOURCE_WORLD_PROVIDER_HELL_H
#define C919_SOURCE_WORLD_PROVIDER_HELL_H
#include "world/WorldProvider.h"
typedef struct WorldProviderHell { WorldProvider provider; } WorldProviderHell;
typedef struct WorldProviderHellBorder { WorldBorder border; WorldProviderHell *this_0; } WorldProviderHellBorder;
bool WorldProviderHell_isInstance(const MCObject *);
WorldProviderHell *WorldProviderHell_nativeAllocate(MCObjectHeap *,const WorldProviderDependencies *,MCObject *);
bool WorldProviderHell_construct(WorldProviderHell *);
WorldProviderHell *WorldProviderHell_new(MCObjectHeap *,const WorldProviderDependencies *,MCObject *);
bool WorldProviderHell_registerWorldChunkManager(WorldProviderHell *);
bool WorldProviderHell_generateLightBrightnessTable(WorldProviderHell *);
float WorldProviderHell_calculateCelestialAngle(WorldProviderHell *,int64_t,float);
WorldBorder *WorldProviderHell_getWorldBorder(WorldProviderHell *);
bool WorldProviderHellBorder_isInstance(const MCObject *);
WorldProviderHellBorder *WorldProviderHellBorder_nativeAllocate(MCObjectHeap *);
bool WorldProviderHellBorder_construct(WorldProviderHellBorder *,WorldProviderHell *,const WorldBorderDependencies *,MCObject *);
double WorldProviderHellBorder_getCenterX(WorldBorder *);
double WorldProviderHellBorder_getCenterZ(WorldBorder *);
#endif
