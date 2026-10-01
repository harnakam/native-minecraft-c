#ifndef C919_NATIVE_HASH_MAP_H
#define C919_NATIVE_HASH_MAP_H
#include "nbt/NBTString.h"
#include "util/NativeReferenceList.h"
/* Real managed JDK collection dependency. UTF16 value keys or Object identity,
   nullable entries, retained equal key refs, live views and fail-fast
   iterators. Values-view removal supports identity and String value equality;
   other value-class equals overrides are an unported dependency. Native
   hash-list storage is not a full JDK HashMap/tree-bin/concurrency port. */
typedef enum {
  NATIVE_HASH_KEY_STRING,
  NATIVE_HASH_KEY_IDENTITY
} NativeHashKeyKind;
typedef struct NativeHashMap NativeHashMap;
typedef struct NativeHashMapView NativeHashMapView;
typedef struct NativeIterator NativeIterator;
typedef struct {
  bool (*hashCode)(MCObject *context, MCObject *key, int32_t *out);
  bool (*equals)(MCObject *context, MCObject *query, MCObject *stored,
                 bool *out);
} NativeHashKeyMethods;
NativeHashMap *NativeHashMap_new(MCObjectHeap *, NativeHashKeyKind);
/* Required source key dispatch, immutable methods and a traced context. NULL
   keys bypass dispatch; pointer equality precedes query.equals(stored). */
NativeHashMap *NativeHashMap_newWithKeys(MCObjectHeap *,
                                         const NativeHashKeyMethods *,
                                         MCObject *);
bool NativeHashMap_isInstance(const MCObject *);
int32_t NativeHashMap_size(const NativeHashMap *);
MCObject *NativeHashMap_get(NativeHashMap *, MCObject *key);
bool NativeHashMap_containsKey(NativeHashMap *, MCObject *key);
bool NativeHashMap_put(NativeHashMap *, MCObject *key, MCObject *value);
/* Same single hash/equals/mutation path, with HashMap.put's nullable previous
   value. previous is required; only successful completion updates its value. */
bool NativeHashMap_putWithPrevious(NativeHashMap *, MCObject *key,
                                   MCObject *value, MCObject **previous);
MCObject *NativeHashMap_remove(NativeHashMap *, MCObject *key);
bool NativeHashMap_clear(NativeHashMap *);
NativeHashMapView *NativeHashMap_values(NativeHashMap *);
NativeHashMapView *NativeHashMap_keys(NativeHashMap *);
bool NativeHashMapView_isInstance(const MCObject *);
int32_t NativeHashMapView_size(NativeHashMapView *);
bool NativeHashMapView_remove(NativeHashMapView *, MCObject *);
bool NativeHashMapView_clear(NativeHashMapView *);
NativeIterator *NativeIterator_fromView(NativeHashMapView *);
NativeIterator *NativeIterator_fromList(NativeReferenceList *);
bool NativeIterator_isInstance(const MCObject *);
/* hasNext does not check modCount; next does. NULL is a successful next value.
   Failed next preserves *out and fails the owner heap. */
bool NativeIterator_hasNext(NativeIterator *);
bool NativeIterator_next(NativeIterator *, MCObject **out);
#endif
