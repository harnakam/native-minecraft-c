#ifndef C919_NATIVE_REFERENCE_LIST_H
#define C919_NATIVE_REFERENCE_LIST_H
#include "util/MCObjectHeap.h"
#include "util/NativePrimitiveArray.h"
#include "util/NativeTypedObjectArray.h"

/* Native ordered reference collection, not a complete java.util class port.
   NULL and repeated references are real entries. Both list and growable storage
   are managed; source shallow snapshots retain edges, heap snapshots remap them. */
typedef struct NativeReferenceArray {
    MCObject object;
    int32_t capacity;
    MCObject *items[];
} NativeReferenceArray;
typedef struct NativeReferenceList {
    MCObject object;
    NativeReferenceArray *storage;
    int32_t size;
    uint32_t modCount;
} NativeReferenceList;
NativeReferenceList *NativeReferenceList_new(MCObjectHeap *);
bool NativeReferenceList_isInstance(const MCObject *);
int32_t NativeReferenceList_size(const NativeReferenceList *);
/* NULL is a valid value; a rejected access/mutation also fails the owner heap. */
MCObject *NativeReferenceList_get(const NativeReferenceList *,int32_t index);
bool NativeReferenceList_add(NativeReferenceList *,MCObject *);
/* Native ArrayList.addAll reached storage boundary, after Collection.toArray.
   One modCount change (including an empty append) precedes capacity growth.
   Existing native capacity policy is retained, not a full JDK ArrayList port.
   References are shallow; output is assigned only on success. */
bool NativeReferenceList_addAllArray(NativeReferenceList *,NativeObjectArray *,bool *changed);
MCObject *NativeReferenceList_set(NativeReferenceList *,int32_t index,MCObject *);
MCObject *NativeReferenceList_remove(NativeReferenceList *,int32_t index);
/* Reached ArrayList Object-removal semantics. The query's actual equals is
   called even for identical references; NULL uses the direct NULL branch.
   The list remains live across callbacks. A healthy Source exception keeps
   all completed mutations, including fastRemove's modCount/size prefix.
   These methods retain native storage/capacity policy, not a full JDK port. */
typedef struct NativeReferenceListEqualsMethods {
    NativeArrayResult (*equals)(MCObject *context,MCObject *query,MCObject *stored,bool *out);
} NativeReferenceListEqualsMethods;
NativeArrayResult NativeReferenceList_removeObjectSource(NativeReferenceList *,MCObject *query,
    const NativeReferenceListEqualsMethods *,MCObject *nullableContext,bool *removed);
NativeArrayResult NativeReferenceList_sizeSource(const NativeReferenceList *,int32_t *out);
NativeArrayResult NativeReferenceList_getSource(const NativeReferenceList *,int32_t index,MCObject **out);
bool NativeReferenceList_clear(NativeReferenceList *);
NativeReferenceList *NativeReferenceList_copy(const NativeReferenceList *);
#endif
