#ifndef C919_SOURCE_ENTITY_HANGING_H
#define C919_SOURCE_ENTITY_HANGING_H
#include "entity/Entity.h"
#include "util/BlockPos.h"
#include "util/NativeJavaClass.h"
typedef struct World World;
typedef struct EntityHanging EntityHanging;
typedef enum { ENTITY_FRAME_OK, ENTITY_FRAME_EXCEPTION, ENTITY_FRAME_FAILURE } EntityFrameResult;
/* Immutable native facts for the reached EnumFacing leaves, not a complete
   EnumFacing/Enum/Vec3i class translation or another managed entity owner. */
typedef enum { NATIVE_HANGING_AXIS_X, NATIVE_HANGING_AXIS_Y, NATIVE_HANGING_AXIS_Z } NativeHangingAxis;
typedef struct NativeHangingFacing {
    int32_t index, horizontalIndex, offsetX, offsetY, offsetZ;
    NativeHangingAxis axis;
} NativeHangingFacing;
extern const NativeHangingFacing NativeHangingFacing_DOWN, NativeHangingFacing_UP;
extern const NativeHangingFacing NativeHangingFacing_NORTH, NativeHangingFacing_SOUTH;
extern const NativeHangingFacing NativeHangingFacing_WEST, NativeHangingFacing_EAST;
bool NativeHangingFacing_isKnown(const NativeHangingFacing *);
bool NativeHangingFacing_getHorizontalIndex(const NativeHangingFacing *, int32_t *);
const NativeHangingFacing *NativeHangingFacing_getHorizontal(int32_t);
const NativeHangingFacing *NativeHangingFacing_rotateYCCW(const NativeHangingFacing *);
typedef struct EntityHangingDependencies {
    bool (*getWidthPixels)(MCObject *, EntityHanging *, int32_t *);
    bool (*getHeightPixels)(MCObject *, EntityHanging *, int32_t *);
} EntityHangingDependencies;
struct EntityHanging {
    Entity entity;
    int32_t tickCounter1;
    BlockPos *hangingPosition;
    const NativeHangingFacing *facingDirection;
    const EntityHangingDependencies *nativeDependencies;
    MCObject *nativeContext;
};
bool EntityHanging_isInstance(const MCObject *);
void EntityHanging_traceFields(EntityHanging *, MCObjectVisitor, void *);
extern const NativeJavaClassDescriptor EntityHanging_Class;
NativeJavaClass *EntityHanging_nativeClass(MCObjectHeap *);
/* Original abstract superclass constructors on an actual allocated subtype.
   Entity virtual callbacks must invoke that subtype's real methods. */
EntityFrameResult EntityHanging_construct(EntityHanging *, World *, const EntityDependencies *,
    MCObject *, NativeJavaRandomRuntime *, NativeEntityIDRuntime *);
EntityFrameResult EntityHanging_constructPosition(EntityHanging *, World *, BlockPos *,
    const EntityDependencies *, MCObject *, NativeJavaRandomRuntime *, NativeEntityIDRuntime *);
/* Original inherited empty entityInit; Frame supplies its actual override. */
bool EntityHanging_entityInit(EntityHanging *);
EntityFrameResult EntityHanging_updateFacingWithBoundingBox(EntityHanging *, const NativeHangingFacing *);
EntityFrameResult EntityHanging_setPosition(EntityHanging *, double, double, double);
BlockPos *EntityHanging_getHangingPosition(EntityHanging *);
const NativeHangingFacing *EntityHanging_getHorizontalFacing(EntityHanging *);
EntityFrameResult EntityHanging_writeEntityToNBT(EntityHanging *, NBTTagCompound *);
EntityFrameResult EntityHanging_readEntityFromNBT(EntityHanging *, NBTTagCompound *);
/* Physics, validity, damage, onUpdate/onBroken and full EnumFacing remain
   unported; none has a fabricated successful substitute here. */
#endif
