#ifndef C919_SOURCE_MATERIAL_LIQUID_H
#define C919_SOURCE_MATERIAL_LIQUID_H
#include "block/material/Material.h"

/* Original subclass declares no additional instance fields. */
typedef struct MaterialLiquid {
    Material material;
} MaterialLiquid;
extern const NativeJavaClassDescriptor MaterialLiquid_Class;
const MCObjectClass *MaterialLiquid_nativeClass(void);
bool MaterialLiquid_isInstance(const MCObject *);
bool MaterialLiquid_isRuntimeClass(const MCObject *);
MaterialLiquid *MaterialLiquid_nativeAllocate(MCObjectHeap *);
NativeArrayResult MaterialLiquid_construct(MaterialLiquid *, MapColor *nullableColor);
NativeArrayResult MaterialLiquid_new(MCObjectHeap *, MapColor *nullableColor, MaterialLiquid **out);
NativeArrayResult MaterialLiquid_isLiquid(MaterialLiquid *, bool *out);
NativeArrayResult MaterialLiquid_blocksMovement(MaterialLiquid *, bool *out);
NativeArrayResult MaterialLiquid_isSolid(MaterialLiquid *, bool *out);
#endif
