#ifndef C919_SOURCE_BLOCK_POS_H
#define C919_SOURCE_BLOCK_POS_H
#include "util/Vec3i.h"

typedef struct BlockPos { Vec3i vec3i; } BlockPos;
extern const NativeJavaClassDescriptor BlockPos_Class;
const MCObjectClass *BlockPos_nativeClass(void);
bool BlockPos_isInstance(const MCObject *);
bool BlockPos_isRuntimeClass(const MCObject *);
BlockPos *BlockPos_nativeAllocate(MCObjectHeap *);
NativeArrayResult BlockPos_constructInt(BlockPos *, int32_t, int32_t, int32_t);
NativeArrayResult BlockPos_constructDouble(BlockPos *, double, double, double);
BlockPos *BlockPos_newInt(MCObjectHeap *, int32_t, int32_t, int32_t);
BlockPos *BlockPos_newDouble(MCObjectHeap *, double, double, double);
BlockPos *BlockPos_add(BlockPos *, int32_t, int32_t, int32_t);
BlockPos *BlockPos_down(BlockPos *);
BlockPos *BlockPos_downN(BlockPos *, int32_t);
NativeArrayResult BlockPos_toLong(BlockPos *, int64_t *out);
BlockPos *BlockPos_fromLong(MCObjectHeap *, int64_t);
/* Separate NEW/constructor phases over the SAME Source owner. */
BlockPos *NativeBlockPos_allocate(MCObjectHeap *);
bool NativeBlockPos_constructCoordinates(BlockPos *, int32_t, int32_t, int32_t);
BlockPos *NativeBlockPos_origin(MCObjectHeap *);
/* add/down use virtual coordinates and preserve zero-offset identity. DOWN's
   immutable (0,-1,0) enum facts and packed 26/12/26 widths are explicit native
   dependencies; general EnumFacing/offset and other methods remain pending.
   Pointer methods translate Source NULL to sticky native failure. Status APIs
   retain healthy EXCEPTION and preserve outputs on unsuccessful completion. */
#endif
