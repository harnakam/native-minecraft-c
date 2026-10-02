#ifndef C919_NATIVE_COLLECTION_TYPED_H
#define C919_NATIVE_COLLECTION_TYPED_H
#include "util/NativeLinkedHashMap.h"
#include "util/NativeTypedObjectArray.h"

/* Reached Collection.size/toArray(T[]) dependency, not a complete JDK class.
   The captured receiver remains the invocation receiver. Immutable methods
   have process lifetime; the nullable contextField addresses the caller's
   current traced managed context, not a cached copy of that reference. */
typedef struct NativeCollectionTypedMethods {
    NativeArrayResult (*size)(MCObject *context, MCObject *receiver, int32_t *out);
    NativeArrayResult (*toArray)(MCObject *context, MCObject *receiver,
                                 NativeTypedObjectArray *requested, MCObject **out);
} NativeCollectionTypedMethods;

NativeArrayResult NativeCollectionTyped_size(MCObjectHeap *workingHeap,
                                             MCObject *capturedCollection,
                                             const NativeCollectionTypedMethods *nullableMethods,
                                             MCObject **nullableNativeContextField, int32_t *out);
NativeArrayResult NativeCollectionTyped_toArray(MCObjectHeap *workingHeap,
                                                MCObject *capturedCollection,
                                                const NativeCollectionTypedMethods *nullableMethods,
                                                MCObject **nullableNativeContextField,
                                                NativeTypedObjectArray *requested, MCObject **out);

/* Reached AbstractCollection storage dependency for concrete custom adapters.
   The caller has already evaluated size, prepared the runtime-typed working
   array, THEN created the actual linked iterator. This consumes that captured
   iterator; it never invents one or changes initial allocation order. Working
   and original arrays have the same runtime component, as in the real branch. */
NativeArrayResult NativeCollectionTyped_fromLinkedIterator(
    MCObjectHeap *, NativeLinkedHashMapIterator *capturedIterator, NativeTypedObjectArray *working,
    NativeTypedObjectArray *originalRequested, MCObject **out);

/* Native ownership/OOM/missing dependency is sticky FAILURE. Source NULL/store
   or iterator exceptions are healthy EXCEPTION and retain completed writes.
   Outputs change only on healthy OK. Custom toArray may return NULL or a real
   managed non-array; caller's later checkcast is a separate Source operation.
   Defaults implement real NativeReferenceList and insertion-ordered linked
   key/value views only. Unknown default receivers fail explicitly. */
#endif
