#ifndef C919_SOURCE_MATERIAL_PORTAL_H
#define C919_SOURCE_MATERIAL_PORTAL_H
#include "block/material/Material.h"

/* Original subclass declares no additional instance fields. */
typedef struct MaterialPortal {
    Material material;
} MaterialPortal;
extern const NativeJavaClassDescriptor MaterialPortal_Class;
const MCObjectClass *MaterialPortal_nativeClass(void);
bool MaterialPortal_isInstance(const MCObject *);
bool MaterialPortal_isRuntimeClass(const MCObject *);
MaterialPortal *MaterialPortal_nativeAllocate(MCObjectHeap *);
NativeArrayResult MaterialPortal_construct(MaterialPortal *, MapColor *nullableColor);
NativeArrayResult MaterialPortal_new(MCObjectHeap *, MapColor *nullableColor, MaterialPortal **out);
NativeArrayResult MaterialPortal_isSolid(MaterialPortal *, bool *out);
NativeArrayResult MaterialPortal_blocksLight(MaterialPortal *, bool *out);
NativeArrayResult MaterialPortal_blocksMovement(MaterialPortal *, bool *out);
#endif
