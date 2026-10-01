#include "client/entity/AbstractClientPlayer.h"
#include "client/entity/EntityPlayerSP.h"
#include "util/MCGameplayPlayer.h"

bool AbstractClientPlayer_isInstance(const MCObject *object) {
    return EntityPlayerSP_isInstance(object) &&
        MCObjectHeap_objectSize(object)>=sizeof(AbstractClientPlayer);
}
void AbstractClientPlayer_traceFields(AbstractClientPlayer *self,MCObjectVisitor visit,void *context) {
    MCGameplayPlayer_traceFields(&self->player,visit,context);
    self->playerInfo=visit(self->playerInfo,context);
}
bool AbstractClientPlayer_construct(AbstractClientPlayer *self,MCObject *world,NativeGameProfile *profile,
    const EntityPlayerDependencies *deps,const mc_crafting_dispatch *crafting,MCObject *context,
    NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids) {
    if(!AbstractClientPlayer_isInstance((MCObject *)self)) {
        MCObjectHeap_fail(self?((MCObject *)self)->heap:NULL);
        return false;
    }
    /* Original constructor only delegates to EntityPlayer. In particular its
       default playerInfo field is not reset after superclass virtual calls. */
    return EntityPlayer_construct(&self->player,world,profile,deps,crafting,context,random,ids);
}
