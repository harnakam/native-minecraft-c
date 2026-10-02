#ifndef C919_NATIVE_TYPED_OBJECT_ARRAY_H
#define C919_NATIVE_TYPED_OBJECT_ARRAY_H
#include "util/NativePrimitiveArray.h"

/* One canonical reference-array storage type, with a genuine runtime component.
   A typed array is also an Object[]; this alias needs no layout/payload cast. */
typedef NativeObjectArray NativeTypedObjectArray;
typedef enum { NATIVE_ARRAY_OK, NATIVE_ARRAY_EXCEPTION, NATIVE_ARRAY_FAILURE } NativeArrayResult;
/* EXCEPTION is a healthy Source null/index/negative-length/array-store result;
   prior caller writes remain. FAILURE is sticky native class/heap/OOM failure.
   Success alone assigns out. Unknown non-Object component facts fail explicitly. */
NativeArrayResult NativeTypedObjectArray_new(MCObjectHeap *, NativeJavaClass *, int32_t,
                                             NativeTypedObjectArray **out);
bool NativeTypedObjectArray_isInstance(const MCObject *);
NativeArrayResult NativeTypedObjectArray_get(const NativeTypedObjectArray *, int32_t,
                                             MCObject **out);
NativeArrayResult NativeTypedObjectArray_set(NativeTypedObjectArray *, int32_t, MCObject *);
/* Runtime component covariance, distinct from testing each contained value.
   For S34, Object[] containing Vec4bs fails a Vec4b[] cast. */
bool NativeTypedObjectArray_isAssignableTo(NativeTypedObjectArray *, NativeJavaClass *, bool *out);
#endif
