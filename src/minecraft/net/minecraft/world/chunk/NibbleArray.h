#ifndef C919_SOURCE_NIBBLE_ARRAY_H
#define C919_SOURCE_NIBBLE_ARRAY_H
#include "util/NativePrimitiveArray.h"
typedef struct NibbleArray {MCObject object;NativeByteArray *data;} NibbleArray;
NibbleArray *NibbleArray_nativeAllocate(MCObjectHeap *);
bool NibbleArray_construct(NibbleArray *);
bool NibbleArray_constructWithArray(NibbleArray *,NativeByteArray *);
NibbleArray *NibbleArray_new(MCObjectHeap *);
NibbleArray *NibbleArray_newWithArray(MCObjectHeap *,NativeByteArray *);
bool NibbleArray_isInstance(const MCObject *);
int32_t NibbleArray_get(NibbleArray *,int32_t x,int32_t y,int32_t z);
bool NibbleArray_set(NibbleArray *,int32_t x,int32_t y,int32_t z,int32_t value);
int32_t NibbleArray_getFromIndex(NibbleArray *,int32_t index);
bool NibbleArray_setIndex(NibbleArray *,int32_t index,int32_t value);
NativeByteArray *NibbleArray_getData(NibbleArray *);
#endif
