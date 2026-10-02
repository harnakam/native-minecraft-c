#ifndef C919_SOURCE_BLOCK_POS_MUTABLE_BLOCK_POS_H
#define C919_SOURCE_BLOCK_POS_MUTABLE_BLOCK_POS_H
#include "util/BlockPos.h"

/* Actual nested final class: these fields shadow the immutable parent zeros. */
typedef struct BlockPosMutableBlockPos {
    BlockPos blockPos;
    int32_t x;
    int32_t y;
    int32_t z;
} BlockPosMutableBlockPos;
extern const NativeJavaClassDescriptor BlockPosMutableBlockPos_Class;
const MCObjectClass *BlockPosMutableBlockPos_nativeClass(void);
bool BlockPosMutableBlockPos_isInstance(const MCObject *);
bool BlockPosMutableBlockPos_isRuntimeClass(const MCObject *);
BlockPosMutableBlockPos *BlockPosMutableBlockPos_nativeAllocate(MCObjectHeap *);
NativeArrayResult BlockPosMutableBlockPos_constructEmpty(BlockPosMutableBlockPos *);
NativeArrayResult BlockPosMutableBlockPos_constructInt(BlockPosMutableBlockPos *, int32_t,
                                                      int32_t, int32_t);
BlockPosMutableBlockPos *BlockPosMutableBlockPos_newEmpty(MCObjectHeap *);
BlockPosMutableBlockPos *BlockPosMutableBlockPos_newInt(MCObjectHeap *, int32_t, int32_t, int32_t);
NativeArrayResult BlockPosMutableBlockPos_getX(BlockPosMutableBlockPos *, int32_t *out);
NativeArrayResult BlockPosMutableBlockPos_getY(BlockPosMutableBlockPos *, int32_t *out);
NativeArrayResult BlockPosMutableBlockPos_getZ(BlockPosMutableBlockPos *, int32_t *out);
NativeArrayResult BlockPosMutableBlockPos_set(BlockPosMutableBlockPos *, int32_t, int32_t,
                                             int32_t, BlockPosMutableBlockPos **out);
/* set allocates nothing and returns this. NULL: healthy EXCEPTION; invalid
   native shape/ownership or failed heap: sticky FAILURE. */
#endif
