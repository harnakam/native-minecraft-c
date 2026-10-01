#ifndef C919_NATIVE_HASH_SET_H
#define C919_NATIVE_HASH_SET_H
#include "util/NativeHashMap.h"
/* Managed JDK HashSet dependency with actual map/PRESENT ownership. The map's
   documented hash-list/tree-bin boundary also applies here. */
typedef struct {MCObject object;NativeHashMap *map;MCObject *present;} NativeHashSet;
NativeHashSet *NativeHashSet_newWithKeys(MCObjectHeap *,const NativeHashKeyMethods *,MCObject *);
bool NativeHashSet_isInstance(const MCObject *);
int32_t NativeHashSet_size(NativeHashSet *);
bool NativeHashSet_contains(NativeHashSet *,MCObject *);
bool NativeHashSet_add(NativeHashSet *,MCObject *,bool *changed);
bool NativeHashSet_remove(NativeHashSet *,MCObject *,bool *changed);
bool NativeHashSet_clear(NativeHashSet *);
NativeIterator *NativeHashSet_iterator(NativeHashSet *);
#endif
