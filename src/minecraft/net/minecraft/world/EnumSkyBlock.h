#ifndef C919_SOURCE_ENUM_SKY_BLOCK_H
#define C919_SOURCE_ENUM_SKY_BLOCK_H
#include "util/MCObjectHeap.h"
typedef struct EnumSkyBlock {
    MCObject object;
    int32_t defaultLightValue;
} EnumSkyBlock;
typedef struct EnumSkyBlockStatics {
    MCObject object;
    EnumSkyBlock *SKY,*BLOCK;
} EnumSkyBlockStatics;
/* Original declared field/private constructor over managed enum identities.
   java.lang.Enum initialization and generated values/valueOf remain native
   dependency boundaries; the per-heap static holder is not a process cache. */
EnumSkyBlock *EnumSkyBlock_nativeAllocate(MCObjectHeap *);
bool EnumSkyBlock_construct(EnumSkyBlock *,int32_t);
bool EnumSkyBlock_isInstance(const MCObject *);
EnumSkyBlockStatics *EnumSkyBlock_getStatics(MCObjectHeap *);
#endif
