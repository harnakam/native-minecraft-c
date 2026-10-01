#ifndef C919_SOURCE_BLOCK_POS_H
#define C919_SOURCE_BLOCK_POS_H
#include "entity/DataWatcher.h"

/* Explicit immutable native view for the original BlockPos/Vec3i coordinates.
   It uses the existing watcher/wire value descriptor, without another owner.
   Vec3i constructors/getters and the other BlockPos methods remain unported. */
typedef DataWatcherBlockPos BlockPos;
bool BlockPos_isInstance(const MCObject *);
/* Original add(int,int,int): zero offsets return the exact receiver; all three
   coordinate additions use Java signed-int wrap before a new value is made. */
BlockPos *BlockPos_add(BlockPos *,int32_t x,int32_t y,int32_t z);
/* Native per-heap class-static lifetime adapter, not a complete Java class
   initializer. One rooted ORIGIN identity is remapped with all graph roots. */
BlockPos *NativeBlockPos_origin(MCObjectHeap *);
#endif
