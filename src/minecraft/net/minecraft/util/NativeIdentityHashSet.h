#ifndef C919_NATIVE_IDENTITY_HASH_SET_H
#define C919_NATIVE_IDENTITY_HASH_SET_H
#include "util/NativeIdentityHashMap.h"
/* Native dependency for Sets.newIdentityHashSet/newSetFromMap. PRESENT and
   NULL-key sentinel are per-heap rooted identities remapped by graph clones. */
typedef struct NativeIdentityHashSet {
    MCObject object;
    NativeIdentityHashMap *map;
    MCObject *present;
} NativeIdentityHashSet;
NativeIdentityHashSet *NativeIdentityHashSet_new(MCObjectHeap *);
bool NativeIdentityHashSet_isInstance(const MCObject *);
int32_t NativeIdentityHashSet_size(NativeIdentityHashSet *);
bool NativeIdentityHashSet_contains(NativeIdentityHashSet *,MCObject *);
bool NativeIdentityHashSet_add(NativeIdentityHashSet *,MCObject *,bool *changed);
bool NativeIdentityHashSet_remove(NativeIdentityHashSet *,MCObject *,bool *changed);
bool NativeIdentityHashSet_clear(NativeIdentityHashSet *);
NativeIdentityIterator *NativeIdentityHashSet_iterator(NativeIdentityHashSet *);
#endif
