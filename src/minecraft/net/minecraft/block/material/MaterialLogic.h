#ifndef C919_SOURCE_MATERIAL_LOGIC_H
#define C919_SOURCE_MATERIAL_LOGIC_H
#include "block/material/Material.h"

/* Original subclass declares no additional instance fields. */
typedef struct MaterialLogic {
    Material material;
} MaterialLogic;
extern const NativeJavaClassDescriptor MaterialLogic_Class;
const MCObjectClass *MaterialLogic_nativeClass(void);
bool MaterialLogic_isInstance(const MCObject *);
bool MaterialLogic_isRuntimeClass(const MCObject *);
MaterialLogic *MaterialLogic_nativeAllocate(MCObjectHeap *);
NativeArrayResult MaterialLogic_construct(MaterialLogic *, MapColor *nullableColor);
NativeArrayResult MaterialLogic_new(MCObjectHeap *, MapColor *nullableColor, MaterialLogic **out);
NativeArrayResult MaterialLogic_isSolid(MaterialLogic *, bool *out);
NativeArrayResult MaterialLogic_blocksLight(MaterialLogic *, bool *out);
NativeArrayResult MaterialLogic_blocksMovement(MaterialLogic *, bool *out);
#endif
