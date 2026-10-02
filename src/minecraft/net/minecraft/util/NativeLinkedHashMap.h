#ifndef C919_NATIVE_LINKED_HASH_MAP_H
#define C919_NATIVE_LINKED_HASH_MAP_H
#include "nbt/NBTString.h"

/* Independently authored, insertion-ordered native collection dependency.
   String keys use exact UTF16 equality/hash; keys and values may be NULL.
   One managed entry owns each key/value pair in both the hash and order links.
   This is not a full JDK LinkedHashMap port: access-order/eldest-entry hooks,
   tree bins and concurrent access are not supported. A reached tree-bin
   transition fails explicitly. All mutation is single-writer on one heap. */
typedef struct NativeLinkedHashMap NativeLinkedHashMap;
typedef struct NativeLinkedHashMapView NativeLinkedHashMapView;
typedef struct NativeLinkedHashMapIterator NativeLinkedHashMapIterator;
typedef enum {
    NATIVE_LINKED_ITERATOR_OK,
    NATIVE_LINKED_ITERATOR_EXCEPTION,
    NATIVE_LINKED_ITERATOR_FAILURE
} NativeLinkedHashMapIteratorResult;

NativeLinkedHashMap *NativeLinkedHashMap_new(MCObjectHeap *);
bool NativeLinkedHashMap_isInstance(const MCObject *);
int32_t NativeLinkedHashMap_size(const NativeLinkedHashMap *);
MCObject *NativeLinkedHashMap_get(NativeLinkedHashMap *, MCObject *key);
bool NativeLinkedHashMap_containsKey(NativeLinkedHashMap *, MCObject *key);
bool NativeLinkedHashMap_put(NativeLinkedHashMap *, MCObject *key, MCObject *value);
/* previous is required and assigned only on success. Equal-key replacement
   retains the original key and order; it is not a structural modification. */
bool NativeLinkedHashMap_putWithPrevious(NativeLinkedHashMap *, MCObject *key, MCObject *value,
                                         MCObject **previous);
MCObject *NativeLinkedHashMap_remove(NativeLinkedHashMap *, MCObject *key);
bool NativeLinkedHashMap_clear(NativeLinkedHashMap *);

/* Cached, live views: no eager snapshot or duplicated authoritative storage. */
NativeLinkedHashMapView *NativeLinkedHashMap_values(NativeLinkedHashMap *);
NativeLinkedHashMapView *NativeLinkedHashMap_keys(NativeLinkedHashMap *);
bool NativeLinkedHashMapView_isInstance(const MCObject *);
int32_t NativeLinkedHashMapView_size(NativeLinkedHashMapView *);
bool NativeLinkedHashMapView_clear(NativeLinkedHashMapView *);
NativeLinkedHashMapIterator *NativeLinkedHashMapIterator_fromView(NativeLinkedHashMapView *);
bool NativeLinkedHashMapIterator_isInstance(const MCObject *);
/* hasNext does not check modCount; next does. NULL can be a successful value.
   Native rejected access/unsupported state fails the heap and preserves *out.
   Structural/empty-iterator failure is represented at that native boundary;
   this API does not model Java Throwable objects or complete iterator methods. */
bool NativeLinkedHashMapIterator_hasNext(NativeLinkedHashMapIterator *);
/* Reached source exception boundary: modCount mismatch or exhausted iteration
   returns healthy EXCEPTION, preserving output and iterator state. Invalid
   native class/ownership/shape returns FAILURE. No Throwable hierarchy is
   claimed. The bool wrapper below converts EXCEPTION into sticky heap failure. */
NativeLinkedHashMapIteratorResult
NativeLinkedHashMapIterator_nextSource(NativeLinkedHashMapIterator *, MCObject **out);
bool NativeLinkedHashMapIterator_next(NativeLinkedHashMapIterator *, MCObject **out);
#endif
