#ifndef C919_NATIVE_JAVA_CLASS_H
#define C919_NATIVE_JAVA_CLASS_H
#include "util/MCObjectHeap.h"

/* Required Class/getClass/isAssignableFrom dependency for the translated
   entity collections. These immutable native facts are not a JDK reflection
   implementation. Each descriptor denotes one runtime class, including its
   interfaces; unknown getClass facts fail instead of inventing a hierarchy. */
typedef struct NativeJavaClassDescriptor NativeJavaClassDescriptor;
struct NativeJavaClassDescriptor {
    const char *name;
    const NativeJavaClassDescriptor *const *supertypes;
    size_t supertypeCount;
    bool (*matchesRuntimeClass)(const MCObject *);
};
/* The same immutable Object fact used by the built-in hierarchy. New native
   Source facts must reference this identity rather than duplicate its name. */
extern const NativeJavaClassDescriptor NativeJavaClass_ObjectClass;
extern const NativeJavaClassDescriptor NativeJavaClass_EntityClass;
typedef struct NativeJavaClass {
    MCObject object;
    const NativeJavaClassDescriptor *descriptor;
} NativeJavaClass;
NativeJavaClass *NativeJavaClass_literal(MCObjectHeap *,const NativeJavaClassDescriptor *);
NativeJavaClass *NativeJavaClass_Object(MCObjectHeap *);
NativeJavaClass *NativeJavaClass_Entity(MCObjectHeap *);
bool NativeJavaClass_isInstance(const MCObject *);
NativeJavaClass *NativeJavaClass_getClass(MCObjectHeap *,MCObject *);
bool NativeJavaClass_isAssignableFrom(NativeJavaClass *,NativeJavaClass *,bool *);
bool NativeJavaClass_isInstanceOf(NativeJavaClass *,MCObject *,bool *);
#endif
