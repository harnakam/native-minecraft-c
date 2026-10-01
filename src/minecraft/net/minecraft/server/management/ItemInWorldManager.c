#include "server/management/ItemInWorldManager.h"

void ItemInWorldManager_traceFields(ItemInWorldManager *self,MCObjectVisitor visit,void *context) {
    self->theWorld=visit(self->theWorld,context);
    self->thisPlayerMP=(EntityPlayerMP *)visit((MCObject *)self->thisPlayerMP,context);
    self->field_180240_f=(BlockPos *)visit((MCObject *)self->field_180240_f,context);
    self->field_180241_i=(BlockPos *)visit((MCObject *)self->field_180241_i,context);
}
static void trace(MCObject *object,MCObjectVisitor visit,void *context) {
    ItemInWorldManager_traceFields((ItemInWorldManager *)object,visit,context);
}
static const MCObjectClass klass={"net.minecraft.server.management.ItemInWorldManager",MCObjectHeap_plainClone,trace,NULL};
bool ItemInWorldManager_isInstance(const MCObject *object) {
    return object&&object->klass==&klass&&MCObjectHeap_objectSize(object)>=sizeof(ItemInWorldManager);
}
ItemInWorldManager *ItemInWorldManager_nativeAllocate(MCObjectHeap *heap) {
    return (ItemInWorldManager *)MCObjectHeap_alloc(heap,sizeof(ItemInWorldManager),&klass);
}
static bool begin(ItemInWorldManager *self,MCObjectRootScope *scope) {
    MCObjectHeap *heap=self?self->object.heap:NULL;
    if(!ItemInWorldManager_isInstance((MCObject *)self)||!MCObjectRootScope_begin(scope,heap)||
        !MCObjectRootScope_pin(scope,(MCObject *)self)) {
        MCObjectHeap_fail(heap);MCObjectRootScope_end(scope);return false;
    }
    return true;
}
bool ItemInWorldManager_construct(ItemInWorldManager *self,MCObject *world) {
    MCObjectRootScope scope={0};if(!begin(self,&scope))return false;
    MCObjectHeap *heap=self->object.heap;bool ok=false;
    if(!MCObjectRootScope_pin(&scope,world))goto done;
    self->gameType=&WorldSettingsGameType_NOT_SET;MCObjectHeap_touch(heap);
    BlockPos *origin=NativeBlockPos_origin(heap);if(!origin)goto done;
    self->field_180240_f=origin;MCObjectHeap_touch(heap);
    origin=NativeBlockPos_origin(heap);if(!origin)goto done;
    self->field_180241_i=origin;self->durabilityRemainingOnBlock=-1;MCObjectHeap_touch(heap);
    self->theWorld=world;MCObjectHeap_touch(heap);ok=true;
done:
    if(!ok)MCObjectHeap_fail(heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(heap);
}
ItemInWorldManager *ItemInWorldManager_new(MCObjectHeap *heap,MCObject *world) {
    MCObjectRootScope scope={0};
    if(!MCObjectRootScope_begin(&scope,heap)){MCObjectHeap_fail(heap);return NULL;}
    ItemInWorldManager *self=MCObjectRootScope_pin(&scope,world)?ItemInWorldManager_nativeAllocate(heap):NULL;
    bool ok=self&&ItemInWorldManager_construct(self,world);
    MCObjectRootScope_end(&scope);return ok?self:NULL;
}
const WorldSettingsGameType *ItemInWorldManager_getGameType(ItemInWorldManager *self) {
    MCObjectRootScope scope={0};if(!begin(self,&scope))return NULL;
    const WorldSettingsGameType *result=self->gameType;
    MCObjectRootScope_end(&scope);return result;
}
static bool predicate(ItemInWorldManager *self,bool creative) {
    MCObjectRootScope scope={0};if(!begin(self,&scope))return false;
    const WorldSettingsGameType *mode=self->gameType;
    bool valid=WorldSettingsGameType_isCanonical(mode);
    bool result=valid&&(creative?WorldSettingsGameType_isCreative(mode):WorldSettingsGameType_isSurvivalOrAdventure(mode));
    if(!valid)MCObjectHeap_fail(self->object.heap);
    MCObjectRootScope_end(&scope);return result;
}
bool ItemInWorldManager_isCreative(ItemInWorldManager *self) {return predicate(self,true);}
bool ItemInWorldManager_survivalOrAdventure(ItemInWorldManager *self) {return predicate(self,false);}
