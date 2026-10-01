#ifndef C919_NATIVE_SORTED_STRING_MAP_H
#define C919_NATIVE_SORTED_STRING_MAP_H
#include "nbt/NBTString.h"
typedef struct NativeSortedStringEntry {
    MCObject object;
    NBTString *key;
    MCObject *value;
    struct NativeSortedStringEntry *left,*right,*parent;
    bool red;
} NativeSortedStringEntry;
typedef struct NativeSortedStringMap {
    MCObject object;
    NativeSortedStringEntry *root;
    int32_t size;
    uint32_t modCount;
} NativeSortedStringMap;
/* Managed natural UTF-16 ordered map boundary, not java.util.TreeMap class
   completion. Real entries retain key/value refs; equal-key replacement keeps
   the original key. Index reads use current sorted contents. */
NativeSortedStringMap *NativeSortedStringMap_new(MCObjectHeap *);
bool NativeSortedStringMap_isInstance(const MCObject *);
MCObject *NativeSortedStringMap_get(NativeSortedStringMap *,const NBTString *);
bool NativeSortedStringMap_containsKey(NativeSortedStringMap *,const NBTString *);
bool NativeSortedStringMap_put(NativeSortedStringMap *,NBTString *,MCObject *);
bool NativeSortedStringMap_entryAt(NativeSortedStringMap *,int32_t,NBTString **,MCObject **);
/* Borrowed native cursor, consumed only within a caller RootScope. Structural
   change is rejected on next; equal-key replacement is not structural. */
typedef struct {NativeSortedStringMap *map;NativeSortedStringEntry *next;uint32_t expectedModCount;} NativeSortedStringCursor;
bool NativeSortedStringMap_beginCursor(NativeSortedStringMap *,NativeSortedStringCursor *);
bool NativeSortedStringMap_next(NativeSortedStringCursor *,NBTString **,MCObject **);
#endif
