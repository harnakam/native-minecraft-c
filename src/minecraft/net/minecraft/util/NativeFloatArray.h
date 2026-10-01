#ifndef C919_NATIVE_FLOAT_ARRAY_H
#define C919_NATIVE_FLOAT_ARRAY_H
#include "util/MCObjectHeap.h"
/* Managed float[] storage. This native lifetime/array adapter retains mutable
   array identity and clone aliases; it is not a translated JVM array class. */
typedef struct NativeFloatArray { MCObject object; int32_t length; float data[]; } NativeFloatArray;
NativeFloatArray *NativeFloatArray_new(MCObjectHeap *,int32_t length);
bool NativeFloatArray_isInstance(const MCObject *);
#endif
