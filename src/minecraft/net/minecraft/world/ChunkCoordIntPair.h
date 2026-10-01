#ifndef C919_SOURCE_CHUNK_COORD_INT_PAIR_H
#define C919_SOURCE_CHUNK_COORD_INT_PAIR_H
#include "util/BlockPos.h"
typedef struct {MCObject object;int32_t chunkXPos,chunkZPos;} ChunkCoordIntPair;
ChunkCoordIntPair *ChunkCoordIntPair_new(MCObjectHeap *,int32_t,int32_t);
bool ChunkCoordIntPair_isInstance(const MCObject *);
int64_t ChunkCoordIntPair_chunkXZ2Int(int32_t,int32_t);
int32_t ChunkCoordIntPair_hashCode(ChunkCoordIntPair *);
bool ChunkCoordIntPair_equals(ChunkCoordIntPair *,MCObject *);
int32_t ChunkCoordIntPair_getCenterXPos(ChunkCoordIntPair *);
int32_t ChunkCoordIntPair_getCenterZPosition(ChunkCoordIntPair *);
int32_t ChunkCoordIntPair_getXStart(ChunkCoordIntPair *);
int32_t ChunkCoordIntPair_getZStart(ChunkCoordIntPair *);
int32_t ChunkCoordIntPair_getXEnd(ChunkCoordIntPair *);
int32_t ChunkCoordIntPair_getZEnd(ChunkCoordIntPair *);
BlockPos *ChunkCoordIntPair_getBlock(ChunkCoordIntPair *,int32_t,int32_t,int32_t);
BlockPos *ChunkCoordIntPair_getCenterBlock(ChunkCoordIntPair *,int32_t);
/* StringBuilder/toString is a separate dependency. */
#endif
