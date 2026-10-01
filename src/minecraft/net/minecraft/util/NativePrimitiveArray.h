#ifndef C919_NATIVE_PRIMITIVE_ARRAY_H
#define C919_NATIVE_PRIMITIVE_ARRAY_H
#include "util/MCObjectHeap.h"

/* Managed Java int[] storage. Array identity belongs to the graph; snapshots
   copy its elements once and preserve every referring alias. */
typedef struct NativeIntArray {
    MCObject object;
    int32_t length;
    int32_t values[];
} NativeIntArray;
NativeIntArray *NativeIntArray_new(MCObjectHeap *,int32_t length);
bool NativeIntArray_isInstance(const MCObject *);
bool NativeIntArray_get(const NativeIntArray *,int32_t index,int32_t *out);
bool NativeIntArray_set(NativeIntArray *,int32_t index,int32_t value);
#endif
