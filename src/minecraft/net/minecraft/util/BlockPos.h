#ifndef C919_SOURCE_BLOCK_POS_H
#define C919_SOURCE_BLOCK_POS_H
#include "entity/DataWatcher.h"

/* Explicit immutable native view for the original BlockPos/Vec3i coordinates.
   It uses the existing watcher/wire value descriptor, without another owner.
   Remaining Vec3i/BlockPos methods and arbitrary getter subclasses are unported. */
typedef DataWatcherBlockPos BlockPos;
bool BlockPos_isInstance(const MCObject *);
/* Native construction phases for this same coordinate view. Callers can
   preserve Java NEW before evaluation of constructor arguments. Allocate
   creates one zeroed unpublished coordinate; construct is for that phase,
   not a coordinate mutation method or a full Vec3i/BlockPos constructor port. */
BlockPos *NativeBlockPos_allocate(MCObjectHeap *);
bool NativeBlockPos_constructCoordinates(BlockPos *,int32_t x,int32_t y,int32_t z);
/* Original double constructor delegates ordered MathHelper.floor_double values
   into the shared native immutable coordinate descriptor. */
BlockPos *BlockPos_newDouble(MCObjectHeap *,double x,double y,double z);
BlockPos *BlockPos_down(BlockPos *);
BlockPos *BlockPos_downN(BlockPos *,int32_t n);
/* Original add(int,int,int): zero offsets return the exact receiver; all three
   coordinate additions use Java signed-int wrap before a new value is made. */
BlockPos *BlockPos_add(BlockPos *,int32_t x,int32_t y,int32_t z);
/* Native per-heap class-static lifetime adapter, not a complete Java class
   initializer. One rooted ORIGIN identity is remapped with all graph roots. */
BlockPos *NativeBlockPos_origin(MCObjectHeap *);
#endif
