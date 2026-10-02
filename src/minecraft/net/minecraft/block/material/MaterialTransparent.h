#ifndef C919_SOURCE_MATERIAL_TRANSPARENT_H
#define C919_SOURCE_MATERIAL_TRANSPARENT_H
#include "block/material/Material.h"

/* Original subclass declares no additional instance fields. */
typedef struct MaterialTransparent {
    Material material;
} MaterialTransparent;
extern const NativeJavaClassDescriptor MaterialTransparent_Class;
const MCObjectClass *MaterialTransparent_nativeClass(void);
bool MaterialTransparent_isInstance(const MCObject *);
bool MaterialTransparent_isRuntimeClass(const MCObject *);
MaterialTransparent *MaterialTransparent_nativeAllocate(MCObjectHeap *);
NativeArrayResult MaterialTransparent_construct(MaterialTransparent *, MapColor *nullableColor);
NativeArrayResult MaterialTransparent_new(MCObjectHeap *, MapColor *nullableColor,
                                          MaterialTransparent **out);
NativeArrayResult MaterialTransparent_isSolid(MaterialTransparent *, bool *out);
NativeArrayResult MaterialTransparent_blocksLight(MaterialTransparent *, bool *out);
NativeArrayResult MaterialTransparent_blocksMovement(MaterialTransparent *, bool *out);
#endif
