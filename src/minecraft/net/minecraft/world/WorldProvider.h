#ifndef C919_SOURCE_WORLD_PROVIDER_H
#define C919_SOURCE_WORLD_PROVIDER_H
#include "nbt/NBTString.h"
#include "util/NativeFloatArray.h"
#include "world/border/WorldBorder.h"
typedef struct World World;
typedef struct WorldInfo WorldInfo;
typedef struct WorldType WorldType;
typedef struct WorldProvider WorldProvider;
/* Reached World/WorldInfo virtual methods and unported biome/generator/manager
   constructors are explicit required dependencies. A constructor must return
   an actual managed manager; missing leaves never mean empty success. */
typedef struct {
    WorldInfo *(*getWorldInfo)(MCObject *,World *);
    WorldType *(*getTerrainType)(MCObject *,WorldInfo *);
    NBTString *(*getGeneratorOptions)(MCObject *,WorldInfo *);
    MCObject *(*parseFlatGenerator)(MCObject *,NBTString *);
    bool (*getFlatBiome)(MCObject *,MCObject *flat,int32_t *);
    MCObject *(*getFallbackBiome)(MCObject *);
    MCObject *(*getPlainsBiome)(MCObject *);
    MCObject *(*getHellBiome)(MCObject *);
    MCObject *(*getSkyBiome)(MCObject *);
    MCObject *(*getBiomeFromBiomeList)(MCObject *,int32_t id,MCObject *fallback);
    MCObject *(*newWorldChunkManager)(MCObject *,World *);
    MCObject *(*newWorldChunkManagerHell)(MCObject *,MCObject *biome,float rainfall);
    /* Actual virtual overrides; NULL selects the corresponding translated
       concrete provider method, not a successful no-op. */
    bool (*registerWorldChunkManager)(MCObject *,WorldProvider *);
    bool (*generateLightBrightnessTable)(MCObject *,WorldProvider *);
    const WorldBorderDependencies *borderDependencies;
    /* Original virtual celestial method and reached java.lang.Math.cos
       platform dependency. NULL virtual entry inherits the named body. */
    bool (*calculateCelestialAngle)(MCObject *,WorldProvider *,int64_t,float,float *);
    bool (*mathCos)(MCObject *,double,double *);
} WorldProviderDependencies;
struct WorldProvider {
    MCObject object;
    World *worldObj;
    WorldType *terrainType;
    NBTString *generatorSettings;
    MCObject *worldChunkMgr;
    bool isHellWorld,hasNoSky;
    NativeFloatArray *lightBrightnessTable;
    int32_t dimensionId;
    NativeFloatArray *colorsSunriseSunset;
    const WorldProviderDependencies *dependencies;
    MCObject *dependencyContext;
};
/* Native allocation/class-static lifetime boundary. The implicit constructor
   runs against this same most-derived object and does not clear early writes. */
WorldProvider *WorldProvider_nativeAllocate(MCObjectHeap *,const WorldProviderDependencies *,MCObject *);
bool WorldProvider_construct(WorldProvider *);
bool WorldProvider_isInstance(const MCObject *);
void WorldProvider_traceFields(WorldProvider *,MCObjectVisitor,void *);
NativeFloatArray *WorldProvider_moonPhaseFactors(MCObjectHeap *);
WorldProvider *WorldProvider_getProviderForDimension(MCObjectHeap *,int32_t,const WorldProviderDependencies *,MCObject *);
bool WorldProvider_registerWorld(WorldProvider *,World *);
bool WorldProvider_registerWorldChunkManager(WorldProvider *);
bool WorldProvider_generateLightBrightnessTable(WorldProvider *);
MCObject *WorldProvider_getWorldChunkManager(WorldProvider *);
bool WorldProvider_doesWaterVaporize(WorldProvider *);
bool WorldProvider_getHasNoSky(WorldProvider *);
NativeFloatArray *WorldProvider_getLightBrightnessTable(WorldProvider *);
int32_t WorldProvider_getDimensionId(WorldProvider *);
WorldBorder *WorldProvider_getWorldBorder(WorldProvider *);
NBTString *WorldProvider_getDimensionName(WorldProvider *);
NBTString *WorldProvider_getInternalNameSuffix(WorldProvider *);
float WorldProvider_calculateCelestialAngle(WorldProvider *,int64_t worldTime,float partialTicks);
float WorldProvider_calculateCelestialAngle_base(WorldProvider *,int64_t worldTime,float partialTicks);
/* Other terrain generation/biome/weather/provider methods remain unported. */
#endif
