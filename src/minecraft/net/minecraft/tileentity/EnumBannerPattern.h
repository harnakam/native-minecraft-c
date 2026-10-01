#ifndef C919_SOURCE_ENUM_BANNER_PATTERN_H
#define C919_SOURCE_ENUM_BANNER_PATTERN_H
#include "item/ItemStack.h"
#include "nbt/NBTString.h"

/* Managed storage adapters for the original mutable String[] and the enum
   values() array. The enum fields retain their actual direct references. */
typedef struct { MCObject object; int32_t length; NBTString *items[]; } BannerStringArray;
typedef struct EnumBannerPattern {
    MCObject object;
    NBTString *patternName,*patternID;
    BannerStringArray *craftingLayers;
    ItemStack *patternCraftingStack;
} EnumBannerPattern;
typedef struct { MCObject object; int32_t length; EnumBannerPattern *items[]; } EnumBannerPatternArray;
typedef struct EnumBannerPatternRegistry EnumBannerPatternRegistry;
/* Native class-static initialization: one rooted registry in each heap. It is
   part of the whole-root transaction snapshot, never a process pointer cache. */
EnumBannerPatternRegistry *EnumBannerPattern_nativeRegistry(MCObjectHeap *);
EnumBannerPatternArray *EnumBannerPattern_values(MCObjectHeap *);
NBTString *EnumBannerPattern_getPatternName(EnumBannerPattern *);
NBTString *EnumBannerPattern_getPatternID(EnumBannerPattern *);
BannerStringArray *EnumBannerPattern_getCraftingLayers(EnumBannerPattern *);
bool EnumBannerPattern_hasValidCrafting(const EnumBannerPattern *);
bool EnumBannerPattern_hasCraftingStack(const EnumBannerPattern *);
ItemStack *EnumBannerPattern_getCraftingStack(EnumBannerPattern *);
EnumBannerPattern *EnumBannerPattern_getPatternByID(MCObjectHeap *,const NBTString *);
#endif
