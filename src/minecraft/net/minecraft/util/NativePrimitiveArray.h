#ifndef C919_NATIVE_PRIMITIVE_ARRAY_H
#define C919_NATIVE_PRIMITIVE_ARRAY_H
#include "util/MCObjectHeap.h"
#include "util/NativeJavaClass.h"

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

/* Explicit native Java-array storage; these are not java.lang class ports.
   A fixed Object[] is separate from NativeReferenceList's growable array. */
#define NATIVE_DECLARE_PRIMITIVE_ARRAY(Name, Type) \
typedef struct Name { MCObject object; int32_t length; Type values[]; } Name; \
Name *Name##_new(MCObjectHeap *,int32_t length); \
bool Name##_isInstance(const MCObject *); \
bool Name##_get(const Name *,int32_t index,Type *out); \
bool Name##_set(Name *,int32_t index,Type value)
NATIVE_DECLARE_PRIMITIVE_ARRAY(NativeByteArray,int8_t);
NATIVE_DECLARE_PRIMITIVE_ARRAY(NativeCharArray,uint16_t);
NATIVE_DECLARE_PRIMITIVE_ARRAY(NativeShortArray,int16_t);
NATIVE_DECLARE_PRIMITIVE_ARRAY(NativeBooleanArray,bool);
#undef NATIVE_DECLARE_PRIMITIVE_ARRAY

typedef struct NativeObjectArray {
    MCObject object;
    int32_t length;
    /* NULL denotes the original Object[] runtime component. A typed reference
       array retains its exact managed Class here; all arrays use one owner. */
    NativeJavaClass *componentType;
    MCObject *values[];
} NativeObjectArray;
NativeObjectArray *NativeObjectArray_new(MCObjectHeap *,int32_t length);
bool NativeObjectArray_isInstance(const MCObject *);
/* Exact native runtime class identity, independent of allocation shape. This
   lets dispatch reject a malformed known array without normalizing other types. */
bool NativeObjectArray_isRuntimeClass(const MCObject *);
bool NativeObjectArray_get(const NativeObjectArray *,int32_t index,MCObject **out);
bool NativeObjectArray_set(NativeObjectArray *,int32_t index,MCObject *value);
#endif
