#ifndef C919_SOURCE_TILE_ENTITY_BANNER_H
#define C919_SOURCE_TILE_ENTITY_BANNER_H
#include "item/ItemStack.h"
/* The two original static ItemStack helpers used by RecipesBanners.
   TileEntity instance state, packets and banner texture rendering remain
   separate class dependencies, rather than empty methods in this subset. */
int32_t TileEntityBanner_getBaseColor(ItemStack *);
int32_t TileEntityBanner_getPatterns(ItemStack *);
#endif
