#ifndef C919_NATIVE_REFERENCE_LIST_H
#define C919_NATIVE_REFERENCE_LIST_H
#include "util/MCObjectHeap.h"

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
MCObject *NativeReferenceList_set(NativeReferenceList *,int32_t index,MCObject *);
MCObject *NativeReferenceList_remove(NativeReferenceList *,int32_t index);
bool NativeReferenceList_clear(NativeReferenceList *);
NativeReferenceList *NativeReferenceList_copy(const NativeReferenceList *);
#endif
