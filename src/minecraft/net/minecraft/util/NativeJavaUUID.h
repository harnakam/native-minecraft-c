#ifndef C919_NATIVE_JAVA_UUID_H
#define C919_NATIVE_JAVA_UUID_H
#include "nbt/NBTString.h"

/* Independently authored immutable Java UUID API value adapter. This is not a
   translation of java.util.UUID or its serialization/concurrency machinery. */
typedef struct NativeJavaUUID {
    MCObject object;
    int64_t mostSignificantBits,leastSignificantBits;
} NativeJavaUUID;

NativeJavaUUID *NativeJavaUUID_new(MCObjectHeap *,int64_t most,int64_t least);
bool NativeJavaUUID_isInstance(const MCObject *);
/* UTF-16 Java8-compatible permissive parsing. Invalid, foreign-heap or missing
   input returns NULL and fails the target heap; the immutable input is retained
   only for this call. Results are borrowed under the caller's RootScope. */
NativeJavaUUID *NativeJavaUUID_fromString(MCObjectHeap *,const NBTString *);
#endif
