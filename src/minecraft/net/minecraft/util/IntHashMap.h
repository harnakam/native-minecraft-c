#ifndef C919_SOURCE_INT_HASH_MAP_H
#define C919_SOURCE_INT_HASH_MAP_H
#include "util/MCObjectHeap.h"

typedef struct IntHashMapEntry IntHashMapEntry;
typedef struct IntHashMapSlots {
    MCObject object;
    int32_t length;
    IntHashMapEntry *values[];
} IntHashMapSlots;
/* The four original fields. slots is the explicitly managed Entry[] boundary. */
typedef struct IntHashMap {
    MCObject object;
    IntHashMapSlots *slots;
    int32_t count,threshold;
    float growFactor;
} IntHashMap;
struct IntHashMapEntry {
    MCObject object;
    int32_t hashEntry;
    MCObject *valueEntry;
    IntHashMapEntry *nextEntry;
    int32_t slotHash;
};
IntHashMap *IntHashMap_nativeAllocate(MCObjectHeap *);
bool IntHashMap_construct(IntHashMap *);
IntHashMap *IntHashMap_new(MCObjectHeap *);
bool IntHashMap_isInstance(const MCObject *);
int32_t IntHashMap_computeHash(int32_t);
int32_t IntHashMap_getSlotIndex(int32_t hash,int32_t slotCount);
MCObject *IntHashMap_lookup(IntHashMap *,int32_t key);
IntHashMapEntry *IntHashMap_lookupEntry(IntHashMap *,int32_t key);
bool IntHashMap_containsItem(IntHashMap *,int32_t key);
bool IntHashMap_addKey(IntHashMap *,int32_t key,MCObject *value);
IntHashMapEntry *IntHashMap_removeEntry(IntHashMap *,int32_t key);
MCObject *IntHashMap_removeObject(IntHashMap *,int32_t key);
bool IntHashMap_clearMap(IntHashMap *);
bool IntHashMapEntry_isInstance(const MCObject *);
int32_t IntHashMapEntry_getHash(IntHashMapEntry *);
MCObject *IntHashMapEntry_getValue(IntHashMapEntry *);
int32_t IntHashMapEntry_hashCode(IntHashMapEntry *);
/* Entry.equals/toString and their arbitrary Object virtual calls are a
   separate source dependency; this header does not provide success stubs. */
#endif
