#ifndef C919_NATIVE_IDENTITY_HASH_MAP_H
#define C919_NATIVE_IDENTITY_HASH_MAP_H
#include "util/NativePrimitiveArray.h"
/* Managed JDK8 IdentityHashMap dependency: identity keys, nullable entries,
   linear table probing/resize/deletion and table-order key iteration. Native
   identityHashCode is supplied by the heap. This is not a full JDK class port;
   entry/value views, serialization and iterator removal are not declared. */
typedef struct NativeIdentityHashMap NativeIdentityHashMap;
typedef struct NativeIdentityKeySet NativeIdentityKeySet;
typedef struct NativeIdentityIterator NativeIdentityIterator;
NativeIdentityHashMap *NativeIdentityHashMap_new(MCObjectHeap *,int32_t expectedMaxSize);
bool NativeIdentityHashMap_isInstance(const MCObject *);
int32_t NativeIdentityHashMap_size(NativeIdentityHashMap *);
MCObject *NativeIdentityHashMap_get(NativeIdentityHashMap *,MCObject *key);
bool NativeIdentityHashMap_containsKey(NativeIdentityHashMap *,MCObject *key);
bool NativeIdentityHashMap_putWithPrevious(NativeIdentityHashMap *,MCObject *key,
                                         MCObject *value,MCObject **previous);
bool NativeIdentityHashMap_put(NativeIdentityHashMap *,MCObject *key,MCObject *value);
MCObject *NativeIdentityHashMap_remove(NativeIdentityHashMap *,MCObject *key);
bool NativeIdentityHashMap_clear(NativeIdentityHashMap *);
NativeIdentityKeySet *NativeIdentityHashMap_keySet(NativeIdentityHashMap *);
bool NativeIdentityKeySet_isInstance(const MCObject *);
NativeIdentityIterator *NativeIdentityKeySet_iterator(NativeIdentityKeySet *);
bool NativeIdentityIterator_isInstance(const MCObject *);
bool NativeIdentityIterator_hasNext(NativeIdentityIterator *);
/* End/failure preserves *out; NULL is a successful key. hasNext does not check
   modCount; next checks it before returning the retained table's key. */
bool NativeIdentityIterator_next(NativeIdentityIterator *,MCObject **out);
#endif
