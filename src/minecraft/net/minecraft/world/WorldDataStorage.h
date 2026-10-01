#ifndef C919_SOURCE_WORLD_DATA_STORAGE_H
#define C919_SOURCE_WORLD_DATA_STORAGE_H
#include "world/World.h"
/* Original World.getUniqueDataId body on the single Source World receiver. */
bool World_getUniqueDataId(World *,NBTString *key,int32_t *output);
/* Legacy C919 NEXT-map projection/import, separated from the original method.
   Only the provider's LAST Short entries own counter state. */
bool World_nativeMapNextProjection(const World *,int32_t *output);
bool World_nativeImportMapNextProjection(World *,int32_t next);
#endif
