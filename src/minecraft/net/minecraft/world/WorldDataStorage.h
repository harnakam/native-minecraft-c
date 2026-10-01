#ifndef C919_SOURCE_WORLD_DATA_STORAGE_H
#define C919_SOURCE_WORLD_DATA_STORAGE_H
#include "util/MCGameplayWorld.h"
/* Original World.getUniqueDataId body over the explicit native World receiver.
   This segment does not translate the whole World/WorldClient constructors. */
bool World_getUniqueDataId(MCGameplayWorld *,NBTString *key,int32_t *output);
/* Legacy C919 NEXT-map projection/import, separated from the original method.
   Only the provider's LAST Short entries own counter state. */
bool World_nativeMapNextProjection(const MCGameplayWorld *,int32_t *output);
bool World_nativeImportMapNextProjection(MCGameplayWorld *,int32_t next);
#endif
