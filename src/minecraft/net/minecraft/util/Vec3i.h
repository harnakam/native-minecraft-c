#ifndef C919_SOURCE_VEC3I_H
#define C919_SOURCE_VEC3I_H
#include "util/NativeTypedObjectArray.h"

/* Actual instance fields. The closed translated hierarchy dispatches virtual
   getters; arbitrary subclasses and unreached Comparable/vector methods remain
   pending. Native allocation, class facts and rooted statics are lifetime
   adapters, not full Object/JVM class initialization implementations. Class
   facts close superclass ancestry only; the original Comparable interface
   and its compareTo dispatch are not represented by this native fact. */
typedef struct Vec3i {
    MCObject object;
    int32_t x;
    int32_t y;
    int32_t z;
} Vec3i;

extern const NativeJavaClassDescriptor Vec3i_Class;
const MCObjectClass *Vec3i_nativeClass(void);
bool Vec3i_isInstance(const MCObject *);
bool Vec3i_isRuntimeClass(const MCObject *);
Vec3i *Vec3i_nativeAllocate(MCObjectHeap *);
NativeArrayResult Vec3i_constructInt(Vec3i *, int32_t, int32_t, int32_t);
NativeArrayResult Vec3i_constructDouble(Vec3i *, double, double, double);
Vec3i *Vec3i_newInt(MCObjectHeap *, int32_t, int32_t, int32_t);
Vec3i *Vec3i_newDouble(MCObjectHeap *, double, double, double);
NativeArrayResult Vec3i_getX(Vec3i *, int32_t *out);
NativeArrayResult Vec3i_getY(Vec3i *, int32_t *out);
NativeArrayResult Vec3i_getZ(Vec3i *, int32_t *out);
NativeArrayResult Vec3i_equals(Vec3i *, MCObject *, bool *out);
NativeArrayResult Vec3i_hashCode(Vec3i *, int32_t *out);
Vec3i *NativeVec3i_nullVector(MCObjectHeap *);
/* NULL receiver: healthy EXCEPTION. Invalid native shape/ownership, OOM and a
   failed heap: sticky FAILURE. Outputs change only on OK. Retain/root borrowed
   refs before GC/adoption. Factories' NULL is only an unwind value. */
#endif
