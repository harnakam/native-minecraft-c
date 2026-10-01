#ifndef C919_SOURCE_EMPTY_CHUNK_H
#define C919_SOURCE_EMPTY_CHUNK_H
#include "world/chunk/Chunk.h"
typedef struct EmptyChunk {
    Chunk super;
} EmptyChunk;
EmptyChunk *EmptyChunk_new(MCObjectHeap *, World *, int32_t, int32_t, NativeBlockStateRuntime *);
bool EmptyChunk_isInstance(const MCObject *);
int32_t EmptyChunk_getHeightValue(EmptyChunk *, int32_t, int32_t);
bool EmptyChunk_generateHeightMap(EmptyChunk *);
NativeBlock *EmptyChunk_getBlock(EmptyChunk *, BlockPos *);
int32_t EmptyChunk_getBlockLightOpacity(EmptyChunk *, BlockPos *);
int32_t EmptyChunk_getBlockMetadata(EmptyChunk *, BlockPos *);
int32_t EmptyChunk_getLightFor(EmptyChunk *, EnumSkyBlock *, BlockPos *);
int32_t EmptyChunk_getLightSubtracted(EmptyChunk *, BlockPos *, int32_t);
bool EmptyChunk_canSeeSky(EmptyChunk *, BlockPos *);
bool EmptyChunk_getAreLevelsEmpty(EmptyChunk *, int32_t, int32_t);
bool EmptyChunk_onChunkUnload(EmptyChunk *);
bool EmptyChunk_setChunkModified(EmptyChunk *);
/* These are the reached original read overrides. EmptyChunk inherits actual
   Chunk storage, getBlockState and fillChunk; other declared Java overrides
   will be translated with their caller closures. */
#endif
