#ifndef C919_SOURCE_CHUNK_PRIMER_H
#define C919_SOURCE_CHUNK_PRIMER_H
#include "util/NativeBlockStateRuntime.h"
typedef struct ChunkPrimer {
    MCObject object;NativeShortArray *data;NativeBlockState *defaultState;
    NativeBlockStateRuntime *nativeRuntime;
} ChunkPrimer;
ChunkPrimer *ChunkPrimer_nativeAllocate(MCObjectHeap *,NativeBlockStateRuntime *);
bool ChunkPrimer_construct(ChunkPrimer *);
ChunkPrimer *ChunkPrimer_new(MCObjectHeap *,NativeBlockStateRuntime *);
bool ChunkPrimer_isInstance(const MCObject *);
NativeBlockState *ChunkPrimer_getBlockState(ChunkPrimer *,int32_t x,int32_t y,int32_t z);
NativeBlockState *ChunkPrimer_getBlockStateAt(ChunkPrimer *,int32_t index);
bool ChunkPrimer_setBlockState(ChunkPrimer *,int32_t x,int32_t y,int32_t z,NativeBlockState *);
bool ChunkPrimer_setBlockStateAt(ChunkPrimer *,int32_t index,NativeBlockState *);
#endif
