#ifndef C919_SOURCE_ENTITY_ITEM_FRAME_H
#define C919_SOURCE_ENTITY_ITEM_FRAME_H
#include "entity/EntityHanging.h"
#include "item/ItemStack.h"
#include "util/NativeBlockStateRuntime.h"
typedef struct EntityItemFrameDependencies {
    EntityFrameResult (*updateComparatorOutputLevel)(MCObject *, World *, BlockPos *, NativeBlock *air);
} EntityItemFrameDependencies;
struct EntityItemFrame {
    EntityHanging hanging;
    float itemDropChance;
    const EntityItemFrameDependencies *nativeDependencies;
    MCObject *nativeContext;
};
bool EntityItemFrame_isInstance(const MCObject *);
extern const NativeJavaClassDescriptor EntityItemFrame_Class;
NativeJavaClass *EntityItemFrame_nativeClass(MCObjectHeap *);
EntityItemFrame *EntityItemFrame_nativeAllocate(MCObjectHeap *, const EntityHangingDependencies *,
    MCObject *hangingContext, const EntityItemFrameDependencies *, MCObject *frameContext);
/* Named native dispatch binding for this concrete subtype. Base constructor
   still executes original virtual Hanging.setPosition and Frame.entityInit.
   NULL world is legal; movement/physics are not supplied by empty callbacks. */
const EntityDependencies *EntityItemFrame_nativeEntityDependencies(void);
EntityFrameResult EntityItemFrame_construct(EntityItemFrame *, World *, const EntityDependencies *,
    MCObject *, NativeJavaRandomRuntime *, NativeEntityIDRuntime *);
EntityFrameResult EntityItemFrame_constructPosition(EntityItemFrame *, World *, BlockPos *,
    const NativeHangingFacing *, const EntityDependencies *, MCObject *,
    NativeJavaRandomRuntime *, NativeEntityIDRuntime *);
bool EntityItemFrame_entityInit(EntityItemFrame *);
int32_t EntityItemFrame_getWidthPixels(EntityItemFrame *);
int32_t EntityItemFrame_getHeightPixels(EntityItemFrame *);
float EntityItemFrame_getCollisionBorderSize(EntityItemFrame *);
EntityFrameResult EntityItemFrame_getDisplayedItem(EntityItemFrame *, ItemStack **out);
EntityFrameResult EntityItemFrame_setDisplayedItem(EntityItemFrame *, ItemStack *nullableItem);
EntityFrameResult EntityItemFrame_getRotation(EntityItemFrame *, int32_t *out);
EntityFrameResult EntityItemFrame_setItemRotation(EntityItemFrame *, int32_t);
EntityFrameResult EntityItemFrame_writeEntityToNBT(EntityItemFrame *, NBTTagCompound *);
EntityFrameResult EntityItemFrame_readEntityFromNBT(EntityItemFrame *, NBTTagCompound *);
/* Only the declared reached constructor/watcher/bounds/item/NBT methods are
   translated. Drop, combat, interaction, world comparator and full tick/render
   dependencies are explicit and never replaced with successful placeholders.
   EXCEPTION is a healthy reached Source NULL/Validate/checkcast boundary;
   unsupported dispatch/foreign graph/OOM is sticky native FAILURE. */
#endif
