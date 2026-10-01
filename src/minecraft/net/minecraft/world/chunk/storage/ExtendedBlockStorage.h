#ifndef C919_SOURCE_EXTENDED_BLOCK_STORAGE_H
#define C919_SOURCE_EXTENDED_BLOCK_STORAGE_H
#include "world/chunk/NibbleArray.h"
#include "util/NativeBlockStateRuntime.h"
typedef struct ExtendedBlockStorage {
    MCObject object;
    int32_t yBase,blockRefCount,tickRefCount;
    NativeCharArray *data;NibbleArray *blocklightArray,*skylightArray;
    /* Traced native class-static/virtual Block dependency, not a Source field. */
    NativeBlockStateRuntime *nativeRuntime;
} ExtendedBlockStorage;
/* The native runtime may be NULL: the Source ctor touches only arrays/light.
   The first reached Block/Blocks static access resolves the per-heap registry;
   scalar/light/array getters and malformed native array copies do not. */
ExtendedBlockStorage *ExtendedBlockStorage_nativeAllocate(MCObjectHeap *,NativeBlockStateRuntime *);
bool ExtendedBlockStorage_construct(ExtendedBlockStorage *,int32_t y,bool storeSkylight);
ExtendedBlockStorage *ExtendedBlockStorage_new(MCObjectHeap *,int32_t y,bool storeSkylight,NativeBlockStateRuntime *);
bool ExtendedBlockStorage_isInstance(const MCObject *);
NativeBlockState *ExtendedBlockStorage_get(ExtendedBlockStorage *,int32_t x,int32_t y,int32_t z);
bool ExtendedBlockStorage_set(ExtendedBlockStorage *,int32_t x,int32_t y,int32_t z,NativeBlockState *);
NativeBlock *ExtendedBlockStorage_getBlockByExtId(ExtendedBlockStorage *,int32_t x,int32_t y,int32_t z);
int32_t ExtendedBlockStorage_getExtBlockMetadata(ExtendedBlockStorage *,int32_t x,int32_t y,int32_t z);
bool ExtendedBlockStorage_isEmpty(ExtendedBlockStorage *);
bool ExtendedBlockStorage_getNeedsRandomTick(ExtendedBlockStorage *);
int32_t ExtendedBlockStorage_getYLocation(ExtendedBlockStorage *);
bool ExtendedBlockStorage_setExtSkylightValue(ExtendedBlockStorage *,int32_t x,int32_t y,int32_t z,int32_t value);
int32_t ExtendedBlockStorage_getExtSkylightValue(ExtendedBlockStorage *,int32_t x,int32_t y,int32_t z);
bool ExtendedBlockStorage_setExtBlocklightValue(ExtendedBlockStorage *,int32_t x,int32_t y,int32_t z,int32_t value);
int32_t ExtendedBlockStorage_getExtBlocklightValue(ExtendedBlockStorage *,int32_t x,int32_t y,int32_t z);
bool ExtendedBlockStorage_removeInvalidBlocks(ExtendedBlockStorage *);
NativeCharArray *ExtendedBlockStorage_getData(ExtendedBlockStorage *);
bool ExtendedBlockStorage_setData(ExtendedBlockStorage *,NativeCharArray *);
NibbleArray *ExtendedBlockStorage_getBlocklightArray(ExtendedBlockStorage *);
NibbleArray *ExtendedBlockStorage_getSkylightArray(ExtendedBlockStorage *);
bool ExtendedBlockStorage_setBlocklightArray(ExtendedBlockStorage *,NibbleArray *);
bool ExtendedBlockStorage_setSkylightArray(ExtendedBlockStorage *,NibbleArray *);
#endif
