#ifndef C919_SOURCE_WORLD_TYPE_H
#define C919_SOURCE_WORLD_TYPE_H
#include "nbt/NBTString.h"
typedef struct WorldType {
    MCObject object;
    int32_t worldTypeId;
    NBTString *worldType;
    int32_t generatorVersion;
    bool canBeCreated,isWorldTypeVersioned,hasNotificationData;
} WorldType;
typedef struct WorldTypeArray {
    MCObject object;
    WorldType *items[16];
} WorldTypeArray;
typedef struct WorldTypeStatics {
    MCObject object;
    WorldTypeArray *worldTypes;
    WorldType *DEFAULT,*FLAT,*LARGE_BIOMES,*AMPLIFIED,*CUSTOMIZED,*DEBUG_WORLD,*DEFAULT_1_1;
} WorldTypeStatics;
/* Per-heap class-static roots preserve the actual mutable array and identity
   aliases during whole-graph snapshots. No process cache of managed pointers. */
WorldTypeStatics *WorldType_getStatics(MCObjectHeap *);
bool WorldType_isInstance(const MCObject *);
WorldType *WorldType_nativeAllocate(MCObjectHeap *);
bool WorldType_construct(WorldType *,int32_t id,NBTString *name,int32_t version);
NBTString *WorldType_getWorldTypeName(WorldType *);
NBTString *WorldType_getTranslateName(WorldType *);
NBTString *WorldType_getTranslatedInfo(WorldType *);
WorldType *WorldType_parseWorldType(MCObjectHeap *,NBTString *);
int32_t WorldType_getGeneratorVersion(WorldType *);
WorldType *WorldType_getWorldTypeForGeneratorVersion(WorldType *,int32_t);
bool WorldType_getCanBeCreated(WorldType *);
bool WorldType_isVersioned(WorldType *);
int32_t WorldType_getWorldTypeID(WorldType *);
bool WorldType_showWorldInfoNotice(WorldType *);
/* String operations use the explicit native Java8 UTF-16 adapter. */
#endif
