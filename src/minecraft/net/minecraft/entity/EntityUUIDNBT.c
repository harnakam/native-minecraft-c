#include "entity/EntityUUIDNBT.h"
#include "entity/player/EntityPlayer.h"
#include "entity/item/EntityItem.h"
#include "util/MCGameplayPlayer.h"

static bool fail(MCObjectHeap *heap) { MCObjectHeap_fail(heap); return false; }
static bool uuid_ref(MCObjectHeap *heap, NativeJavaUUID *uuid) {
    return uuid && uuid->object.heap==heap && NativeJavaUUID_isInstance((MCObject *)uuid) &&
        !MCObjectHeap_failed(heap) ? true : fail(heap);
}
NativeJavaUUID *EntityUUIDNBT_nativeGetUniqueID(MCObject *entity) {
    NativeJavaUUID *uuid=NULL;
    if (MCGameplayPlayer_isInstance(entity)) uuid=((MCGameplayPlayer *)entity)->living.entity.entityUniqueID;
    else if (EntityItem_isInstance(entity)) uuid=((EntityItem *)entity)->entity.entityUniqueID;
    else { fail(entity?entity->heap:NULL); return NULL; }
    /* The original field getter may return NULL; the following dereference in
       the writer is what requires a UUID. Cross-heap references are invalid. */
    if (uuid && !uuid_ref(entity->heap,uuid)) return NULL;
    return uuid;
}
static bool native_set(MCObject *entity, NativeJavaUUID *uuid) {
    MCObjectHeap *heap=entity?entity->heap:NULL;
    if (!uuid_ref(heap,uuid)) return false;
    if (MCGameplayPlayer_isInstance(entity)) ((MCGameplayPlayer *)entity)->living.entity.entityUniqueID=uuid;
    else if (EntityItem_isInstance(entity)) ((EntityItem *)entity)->entity.entityUniqueID=uuid;
    else return fail(heap);
    MCObjectHeap_touch(heap); return true;
}
static const EntityUUIDNBTDispatch native_dispatch={EntityUUIDNBT_nativeGetUniqueID,native_set};
const EntityUUIDNBTDispatch *EntityUUIDNBT_nativeOwnerDispatch(void) { return &native_dispatch; }
static bool begin(MCObject *entity, NBTTagCompound *tag, MCObjectRootScope *scope) {
    MCObjectHeap *heap=entity?entity->heap:NULL;
    if (!entity || !tag || ((MCObject *)tag)->heap!=heap || NBTBase_getId((NBTBase *)tag)!=10 ||
        !MCObjectRootScope_begin(scope,heap)) return fail(heap);
    return MCObjectRootScope_pin(scope,entity) && MCObjectRootScope_pin(scope,(MCObject *)tag);
}
bool Entity_writeUUIDToNBTSegment(MCObject *entity, NBTTagCompound *tag, const EntityUUIDNBTDispatch *d) {
    MCObjectHeap *heap=entity?entity->heap:NULL; MCObjectRootScope scope={0};
    bool ok=d && d->getUniqueID && begin(entity,tag,&scope);
    NativeJavaUUID *uuid=ok?d->getUniqueID(entity):NULL;
    if (ok) ok=uuid_ref(heap,uuid) && MCObjectRootScope_pin(&scope,(MCObject *)uuid) &&
        NBTTagCompound_setLong_ascii(tag,"UUIDMost",uuid->mostSignificantBits);
    uuid=ok?d->getUniqueID(entity):NULL;
    if (ok) ok=uuid_ref(heap,uuid) && MCObjectRootScope_pin(&scope,(MCObject *)uuid) &&
        NBTTagCompound_setLong_ascii(tag,"UUIDLeast",uuid->leastSignificantBits);
    if (!ok) fail(heap);
    MCObjectRootScope_end(&scope); return ok && !MCObjectHeap_failed(heap);
}
bool Entity_readUUIDFromNBTSegment(MCObject *entity, NBTTagCompound *tag, const EntityUUIDNBTDispatch *d) {
    MCObjectHeap *heap=entity?entity->heap:NULL; MCObjectRootScope scope={0};
    bool ok=d && d->setUniqueID && begin(entity,tag,&scope); NativeJavaUUID *uuid=NULL;
    if (ok && NBTTagCompound_hasKeyType_ascii(tag,"UUIDMost",4) &&
        NBTTagCompound_hasKeyType_ascii(tag,"UUIDLeast",4)) {
        int64_t most=NBTTagCompound_getLong_ascii(tag,"UUIDMost");
        ok=!MCObjectHeap_failed(heap);
        int64_t least=ok?NBTTagCompound_getLong_ascii(tag,"UUIDLeast"):0;
        if (ok) ok=!MCObjectHeap_failed(heap);
        uuid=ok?NativeJavaUUID_new(heap,most,least):NULL;
        ok=uuid!=NULL;
    } else if (ok && NBTTagCompound_hasKeyType_ascii(tag,"UUID",8)) {
        uuid=NativeJavaUUID_fromString(heap,NBTTagCompound_getString_ascii(tag,"UUID"));
        ok=uuid!=NULL;
    }
    if (ok && uuid) ok=uuid_ref(heap,uuid) && MCObjectRootScope_pin(&scope,(MCObject *)uuid) &&
        d->setUniqueID(entity,uuid) && !MCObjectHeap_failed(heap);
    if (!ok) fail(heap);
    MCObjectRootScope_end(&scope); return ok && !MCObjectHeap_failed(heap);
}
bool EntityPlayer_restoreProfileUUIDSegment(MCGameplayPlayer *player) {
    MCObjectHeap *heap=player?player->living.entity.object.heap:NULL; MCObjectRootScope scope={0};
    bool ok=MCGameplayPlayer_isInstance((MCObject *)player) && MCObjectRootScope_begin(&scope,heap);
    if (ok) ok=MCObjectRootScope_pin(&scope,(MCObject *)player);
    NativeJavaUUID *uuid=ok?EntityPlayer_getUUID(player->gameProfile):NULL;
    if (ok) ok=uuid_ref(heap,uuid);
    if (ok) { player->living.entity.entityUniqueID=uuid; MCObjectHeap_touch(heap); }
    else fail(heap);
    MCObjectRootScope_end(&scope); return ok && !MCObjectHeap_failed(heap);
}
