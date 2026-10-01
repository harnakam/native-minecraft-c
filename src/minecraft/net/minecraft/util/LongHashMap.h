#ifndef C919_SOURCE_LONG_HASH_MAP_H
#define C919_SOURCE_LONG_HASH_MAP_H
#include "util/MCObjectHeap.h"

typedef struct LongHashMapEntry LongHashMapEntry;
/* Native managed representation of the original Entry[] reference array. */
typedef struct LongHashMapEntryArray {
    MCObject object;
    int32_t length;
    LongHashMapEntry *values[];
} LongHashMapEntryArray;
typedef struct LongHashMap {
    MCObject object;
    /* Original fields, in declaration order. The native caller is single-writer;
       modCount does not implement Java volatile/concurrent-memory semantics. */
    LongHashMapEntryArray *hashArray;
    int32_t numHashElements;
    int32_t mask;
    int32_t capacity;
    float percentUseable;
    int32_t modCount;
} LongHashMap;
struct LongHashMapEntry {
    MCObject object;
    int64_t key;
    MCObject *value;
    LongHashMapEntry *nextEntry;
    int32_t hash;
};

LongHashMap *LongHashMap_nativeAllocate(MCObjectHeap *);
bool LongHashMap_construct(LongHashMap *);
LongHashMap *LongHashMap_new(MCObjectHeap *);
bool LongHashMap_isInstance(const MCObject *);
int32_t LongHashMap_getNumHashElements(LongHashMap *);
MCObject *LongHashMap_getValueByKey(LongHashMap *,int64_t key);
bool LongHashMap_containsItem(LongHashMap *,int64_t key);
LongHashMapEntry *LongHashMap_getEntry(LongHashMap *,int64_t key);
bool LongHashMap_add(LongHashMap *,int64_t key,MCObject *value);
MCObject *LongHashMap_remove(LongHashMap *,int64_t key);
LongHashMapEntry *LongHashMap_removeKey(LongHashMap *,int64_t key);

/* Translated private source bodies exposed for native class integration and
   focused verification; these are not additional Java public methods. */
int32_t LongHashMap_getHashedKey(int64_t key);
int32_t LongHashMap_hash(int32_t integer);
int32_t LongHashMap_getHashIndex(int32_t hash,int32_t mask);
bool LongHashMap_resizeTable(LongHashMap *,int32_t newSize);
bool LongHashMap_copyHashTableTo(LongHashMap *,LongHashMapEntryArray *destination);
bool LongHashMap_createKey(LongHashMap *,int32_t hash,int64_t key,MCObject *value,int32_t index);

LongHashMapEntryArray *LongHashMapEntryArray_nativeNew(MCObjectHeap *,int32_t length);
bool LongHashMapEntryArray_isInstance(const MCObject *);
LongHashMapEntry *LongHashMapEntry_new(MCObjectHeap *,int32_t hash,int64_t key,
                                    MCObject *value,LongHashMapEntry *next);
bool LongHashMapEntry_isInstance(const MCObject *);
int64_t LongHashMapEntry_getKey(LongHashMapEntry *);
MCObject *LongHashMapEntry_getValue(LongHashMapEntry *);
int32_t LongHashMapEntry_hashCode(LongHashMapEntry *);
/* Entry.equals/toString require arbitrary Object virtual methods, boxed Long,
   and JDK text conversion; those dependencies are not declared as implemented. */
#endif
