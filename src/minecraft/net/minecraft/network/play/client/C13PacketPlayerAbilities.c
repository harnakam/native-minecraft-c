#include "network/play/client/C13PacketPlayerAbilities.h"
#include "network/play/client/native_packet.h"
static const MCObjectClass klass={"net.minecraft.network.play.client.C13PacketPlayerAbilities",MCObjectHeap_plainClone,NULL,NULL};
bool C13PacketPlayerAbilities_isInstance(const MCObject *o) {return o && o->klass==&klass && MCObjectHeap_objectSize(o)>=sizeof(C13PacketPlayerAbilities);}
static bool valid(C13PacketPlayerAbilities *p) {
    if (!p) return false;
    if (!C13PacketPlayerAbilities_isInstance((MCObject *)p) || MCObjectHeap_objectSize((MCObject *)p)<sizeof(*p) || MCObjectHeap_failed(p->object.heap)) {MCObjectHeap_fail(p->object.heap);return false;}
    return true;
}
C13PacketPlayerAbilities *C13PacketPlayerAbilities_new_empty(MCObjectHeap *heap) {return (C13PacketPlayerAbilities *)MCObjectHeap_alloc(heap,sizeof(C13PacketPlayerAbilities),&klass);}
C13PacketPlayerAbilities *C13PacketPlayerAbilities_new(MCObjectHeap *heap,PlayerCapabilities *capabilities) {
    MCObjectRootScope scope={0};
    if (!capabilities || capabilities->object.heap!=heap || !PlayerCapabilities_isInstance((MCObject *)capabilities) || !MCObjectRootScope_begin(&scope,heap)) {MCObjectHeap_fail(heap);return NULL;}
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)capabilities);
    C13PacketPlayerAbilities *p=ok?C13PacketPlayerAbilities_new_empty(heap):NULL;
    if (p) {
        ok=C13PacketPlayerAbilities_setInvulnerable(p,capabilities->disableDamage) &&
           C13PacketPlayerAbilities_setFlying(p,capabilities->isFlying) &&
           C13PacketPlayerAbilities_setAllowFlying(p,capabilities->allowFlying) &&
           C13PacketPlayerAbilities_setCreativeMode(p,capabilities->isCreativeMode) &&
           C13PacketPlayerAbilities_setFlySpeed(p,PlayerCapabilities_getFlySpeed(capabilities)) &&
           C13PacketPlayerAbilities_setWalkSpeed(p,PlayerCapabilities_getWalkSpeed(capabilities));
    }
    MCObjectRootScope_end(&scope);return ok?p:NULL;
}
bool C13PacketPlayerAbilities_readPacketData(C13PacketPlayerAbilities *p,PacketBuffer *b) {
    MCObjectRootScope scope={0};
    if (!valid(p) || !mc_packet_begin((MCObject *)p,b,&scope)) return false;
    uint8_t flags=mc_get_u8(b->buffer);bool ok=!b->buffer->failed;
    if (ok) ok=C13PacketPlayerAbilities_setInvulnerable(p,(flags&1)>0) && C13PacketPlayerAbilities_setFlying(p,(flags&2)>0) &&
               C13PacketPlayerAbilities_setAllowFlying(p,(flags&4)>0) && C13PacketPlayerAbilities_setCreativeMode(p,(flags&8)>0);
    float speed=ok?mc_get_f32(b->buffer):0;
    if (ok) ok=!b->buffer->failed && C13PacketPlayerAbilities_setFlySpeed(p,speed);
    speed=ok?mc_get_f32(b->buffer):0;
    if (ok) ok=!b->buffer->failed && C13PacketPlayerAbilities_setWalkSpeed(p,speed);
    return mc_packet_finish((MCObject *)p,b,&scope,ok);
}
bool C13PacketPlayerAbilities_writePacketData(C13PacketPlayerAbilities *p,PacketBuffer *b) {
    MCObjectRootScope scope={0};
    if (!valid(p) || !mc_packet_begin((MCObject *)p,b,&scope)) return false;
    uint8_t flags=0;
    if (C13PacketPlayerAbilities_isInvulnerable(p)) flags|=1;
    if (C13PacketPlayerAbilities_isFlying(p)) flags|=2;
    if (C13PacketPlayerAbilities_isAllowFlying(p)) flags|=4;
    if (C13PacketPlayerAbilities_isCreativeMode(p)) flags|=8;
    mc_put_u8(b->buffer,flags);mc_put_f32(b->buffer,p->flySpeed);mc_put_f32(b->buffer,p->walkSpeed);
    return mc_packet_finish((MCObject *)p,b,&scope,!b->buffer->failed);
}
bool C13PacketPlayerAbilities_processPacket(C13PacketPlayerAbilities *p,INetHandlerPlayServer h) {
    MCObjectRootScope scope={0};
    if (!valid(p) || !mc_packet_handler_begin((MCObject *)p,h,&scope)) return false;
    bool ok=h.methods->processPlayerAbilities && h.methods->processPlayerAbilities(h.instance,p);
    return mc_packet_handler_finish((MCObject *)p,&scope,ok);
}
bool C13PacketPlayerAbilities_isInvulnerable(const C13PacketPlayerAbilities *p) {return p->invulnerable;}
bool C13PacketPlayerAbilities_setInvulnerable(C13PacketPlayerAbilities *p,bool value) {if (!valid(p)) return false;p->invulnerable=value;MCObjectHeap_touch(p->object.heap);return true;}
bool C13PacketPlayerAbilities_isFlying(const C13PacketPlayerAbilities *p) {return p->flying;}
bool C13PacketPlayerAbilities_setFlying(C13PacketPlayerAbilities *p,bool value) {if (!valid(p)) return false;p->flying=value;MCObjectHeap_touch(p->object.heap);return true;}
bool C13PacketPlayerAbilities_isAllowFlying(const C13PacketPlayerAbilities *p) {return p->allowFlying;}
bool C13PacketPlayerAbilities_setAllowFlying(C13PacketPlayerAbilities *p,bool value) {if (!valid(p)) return false;p->allowFlying=value;MCObjectHeap_touch(p->object.heap);return true;}
bool C13PacketPlayerAbilities_isCreativeMode(const C13PacketPlayerAbilities *p) {return p->creativeMode;}
bool C13PacketPlayerAbilities_setCreativeMode(C13PacketPlayerAbilities *p,bool value) {if (!valid(p)) return false;p->creativeMode=value;MCObjectHeap_touch(p->object.heap);return true;}
bool C13PacketPlayerAbilities_setFlySpeed(C13PacketPlayerAbilities *p,float value) {if (!valid(p)) return false;p->flySpeed=value;MCObjectHeap_touch(p->object.heap);return true;}
bool C13PacketPlayerAbilities_setWalkSpeed(C13PacketPlayerAbilities *p,float value) {if (!valid(p)) return false;p->walkSpeed=value;MCObjectHeap_touch(p->object.heap);return true;}
