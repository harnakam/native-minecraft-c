#ifndef C919_NATIVE_ITEM_MOTION_H
#define C919_NATIVE_ITEM_MOTION_H
#include "entity/item/EntityItem.h"
#include "world/NativeWorld.h"
/* Explicit native terrain/collision kernel on canonical inherited scalar
   Entity fields. It never copies, frees or normalizes an ItemStack. Complete
   Entity.onUpdate/moveEntity and EntityItem.onUpdate are still separate ports. */
bool NativeItemMotion_tick(EntityItem *, const mc_world *);
/* Native collision/fixed-point transport envelope, not a Java Entity limit.
   Unsupported input fails explicitly before any scalar cast or mutation. */
bool NativeItemMotion_positionSupported(double x, double y, double z);
bool NativeItemMotion_validate(const EntityItem *);
#endif
