#ifndef C919_SOURCE_OBJECT_INT_IDENTITY_MAP_H
#define C919_SOURCE_OBJECT_INT_IDENTITY_MAP_H
#include "util/NativeIdentityHashMap.h"
#include "util/NativeReferenceList.h"
/* Original ObjectIntIdentityMap fields/methods. Boxed Integer/valueOf,
   IdentityHashMap and ArrayList storage are named managed native dependencies. */
typedef struct ObjectIntIdentityMap {
    MCObject object;
    NativeIdentityHashMap *identityMap;
    NativeReferenceList *objectList;
} ObjectIntIdentityMap;
typedef struct ObjectIntIdentityIterator ObjectIntIdentityIterator;
ObjectIntIdentityMap *ObjectIntIdentityMap_nativeAllocate(MCObjectHeap *);
bool ObjectIntIdentityMap_construct(ObjectIntIdentityMap *);
ObjectIntIdentityMap *ObjectIntIdentityMap_new(MCObjectHeap *);
bool ObjectIntIdentityMap_isInstance(const MCObject *);
bool ObjectIntIdentityMap_put(ObjectIntIdentityMap *,MCObject *key,int32_t value);
int32_t ObjectIntIdentityMap_get(ObjectIntIdentityMap *,MCObject *key);
MCObject *ObjectIntIdentityMap_getByValue(ObjectIntIdentityMap *,int32_t value);
ObjectIntIdentityIterator *ObjectIntIdentityMap_iterator(ObjectIntIdentityMap *);
bool ObjectIntIdentityIterator_isInstance(const MCObject *);
bool ObjectIntIdentityIterator_hasNext(ObjectIntIdentityIterator *);
bool ObjectIntIdentityIterator_next(ObjectIntIdentityIterator *,MCObject **out);
#endif
