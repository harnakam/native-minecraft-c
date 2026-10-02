#ifndef C919_SOURCE_WORLD_SAVED_DATA_H
#define C919_SOURCE_WORLD_SAVED_DATA_H
#include "nbt/NBTTagCompound.h"

typedef struct WorldSavedData WorldSavedData;
/* Immutable ancestry fact for genuine translated saved-data subclasses. */
extern const NativeJavaClassDescriptor WorldSavedData_Class;
/* Native exception adapter. EXCEPTION preserves Source partial effects for a
   surrounding catch; FAILURE is sticky unsupported/ownership/allocation error. */
typedef enum {
    WORLD_SAVED_DATA_OK,
    WORLD_SAVED_DATA_EXCEPTION,
    WORLD_SAVED_DATA_FAILURE
} WorldSavedDataResult;
typedef struct WorldSavedDataVirtualMethods {
    WorldSavedDataResult (*readFromNBT)(MCObject *, WorldSavedData *, NBTTagCompound *);
    WorldSavedDataResult (*writeToNBT)(MCObject *, WorldSavedData *, NBTTagCompound *);
    WorldSavedDataResult (*markDirty)(MCObject *, WorldSavedData *);
    WorldSavedDataResult (*setDirty)(MCObject *, WorldSavedData *, bool);
    WorldSavedDataResult (*isDirty)(MCObject *, WorldSavedData *, bool *);
} WorldSavedDataVirtualMethods;
/* Immutable, process-lifetime native class facts. A per-heap rooted registry
   retains these facts through clone/adopt; exact class+size are checked before
   reading base fields. This is not Java reflection/class-initialization. A
   subclass's actual tracer must call WorldSavedData_traceFields exactly once. */
typedef struct WorldSavedDataNativeType {
    const MCObjectClass *objectClass;
    size_t minimumSize;
    const WorldSavedDataVirtualMethods *virtualMethods;
} WorldSavedDataNativeType;
struct WorldSavedData {
    MCObject object;
    NBTString *mapName; /* Source final reference, nullable. */
    bool dirty;         /* Allocation default, not a constructor assignment. */
    MCObject *nativeContext;
};
bool WorldSavedData_nativeRegisterType(MCObjectHeap *, const WorldSavedDataNativeType *);
WorldSavedData *WorldSavedData_nativeAllocate(MCObjectHeap *, const WorldSavedDataNativeType *,
                                              MCObject *nativeContext);
bool WorldSavedData_isInstance(const MCObject *);
void WorldSavedData_traceFields(WorldSavedData *, MCObjectVisitor, void *);
/* Construct the base of a real most-derived allocation; there is deliberately
   no concrete WorldSavedData_new and no successful abstract read/write body. */
bool WorldSavedData_construct(WorldSavedData *, NBTString *nullableName);
WorldSavedDataResult WorldSavedData_readFromNBT(WorldSavedData *, NBTTagCompound *);
WorldSavedDataResult WorldSavedData_writeToNBT(WorldSavedData *, NBTTagCompound *);
WorldSavedDataResult WorldSavedData_markDirty(WorldSavedData *);
WorldSavedDataResult WorldSavedData_markDirty_base(WorldSavedData *);
WorldSavedDataResult WorldSavedData_setDirty(WorldSavedData *, bool);
WorldSavedDataResult WorldSavedData_setDirty_base(WorldSavedData *, bool);
WorldSavedDataResult WorldSavedData_isDirty(WorldSavedData *, bool *);
WorldSavedDataResult WorldSavedData_isDirty_base(WorldSavedData *, bool *);
#endif
