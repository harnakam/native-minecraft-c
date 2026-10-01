#include "network/play/server/S39PacketPlayerAbilities.h"
#include "network/play/server/native_packet.h"
static const MCObjectClass klass={"net.minecraft.network.play.server.S39PacketPlayerAbilities",MCObjectHeap_plainClone,NULL,NULL};
bool S39PacketPlayerAbilities_isInstance(const MCObject *o) {return o && o->klass==&klass && MCObjectHeap_objectSize(o)>=sizeof(S39PacketPlayerAbilities);}
static bool valid(S39PacketPlayerAbilities *p) {
    if (!p) return false;
    if (!S39PacketPlayerAbilities_isInstance((MCObject *)p) || MCObjectHeap_objectSize((MCObject *)p)<sizeof(*p) || MCObjectHeap_failed(p->object.heap)) {MCObjectHeap_fail(p->object.heap);return false;}
    return true;
}
S39PacketPlayerAbilities *S39PacketPlayerAbilities_new_empty(MCObjectHeap *heap) {return (S39PacketPlayerAbilities *)MCObjectHeap_alloc(heap,sizeof(S39PacketPlayerAbilities),&klass);}
S39PacketPlayerAbilities *S39PacketPlayerAbilities_new(MCObjectHeap *heap,PlayerCapabilities *capabilities) {
    MCObjectRootScope scope={0};
    if (!capabilities || capabilities->object.heap!=heap || !PlayerCapabilities_isInstance((MCObject *)capabilities) || !MCObjectRootScope_begin(&scope,heap)) {MCObjectHeap_fail(heap);return NULL;}
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)capabilities);
    S39PacketPlayerAbilities *p=ok?S39PacketPlayerAbilities_new_empty(heap):NULL;
    if (p) {
        ok=S39PacketPlayerAbilities_setInvulnerable(p,capabilities->disableDamage) &&
           S39PacketPlayerAbilities_setFlying(p,capabilities->isFlying) &&
           S39PacketPlayerAbilities_setAllowFlying(p,capabilities->allowFlying) &&
           S39PacketPlayerAbilities_setCreativeMode(p,capabilities->isCreativeMode) &&
           S39PacketPlayerAbilities_setFlySpeed(p,PlayerCapabilities_getFlySpeed(capabilities)) &&
           S39PacketPlayerAbilities_setWalkSpeed(p,PlayerCapabilities_getWalkSpeed(capabilities));
    }
    MCObjectRootScope_end(&scope);return ok?p:NULL;
}
bool S39PacketPlayerAbilities_readPacketData(S39PacketPlayerAbilities *p,PacketBuffer *b) {
    MCObjectRootScope scope={0};
    if (!valid(p) || !mc_packet_begin((MCObject *)p,b,&scope)) return false;
    uint8_t flags=mc_get_u8(b->buffer);bool ok=!b->buffer->failed;
    if (ok) ok=S39PacketPlayerAbilities_setInvulnerable(p,(flags&1)>0) && S39PacketPlayerAbilities_setFlying(p,(flags&2)>0) &&
               S39PacketPlayerAbilities_setAllowFlying(p,(flags&4)>0) && S39PacketPlayerAbilities_setCreativeMode(p,(flags&8)>0);
    float speed=ok?mc_get_f32(b->buffer):0;
    if (ok) ok=!b->buffer->failed && S39PacketPlayerAbilities_setFlySpeed(p,speed);
    speed=ok?mc_get_f32(b->buffer):0;
    if (ok) ok=!b->buffer->failed && S39PacketPlayerAbilities_setWalkSpeed(p,speed);
    return mc_packet_finish((MCObject *)p,b,&scope,ok);
}
bool S39PacketPlayerAbilities_writePacketData(S39PacketPlayerAbilities *p,PacketBuffer *b) {
    MCObjectRootScope scope={0};
    if (!valid(p) || !mc_packet_begin((MCObject *)p,b,&scope)) return false;
    uint8_t flags=0;
    if (S39PacketPlayerAbilities_isInvulnerable(p)) flags|=1;
    if (S39PacketPlayerAbilities_isFlying(p)) flags|=2;
    if (S39PacketPlayerAbilities_isAllowFlying(p)) flags|=4;
    if (S39PacketPlayerAbilities_isCreativeMode(p)) flags|=8;
    mc_put_u8(b->buffer,flags);mc_put_f32(b->buffer,p->flySpeed);mc_put_f32(b->buffer,p->walkSpeed);
    return mc_packet_finish((MCObject *)p,b,&scope,!b->buffer->failed);
}
bool S39PacketPlayerAbilities_processPacket(S39PacketPlayerAbilities *p,INetHandlerPlayClient h) {
    MCObjectRootScope scope={0};
    if (!valid(p) || !mc_client_handler_begin((MCObject *)p,h,&scope)) return false;
    bool ok=h.methods->handlePlayerAbilities && h.methods->handlePlayerAbilities(h.instance,p);
    return mc_packet_handler_finish((MCObject *)p,&scope,ok);
}
bool S39PacketPlayerAbilities_isInvulnerable(const S39PacketPlayerAbilities *p) {return p->invulnerable;}
bool S39PacketPlayerAbilities_setInvulnerable(S39PacketPlayerAbilities *p,bool value) {if (!valid(p)) return false;p->invulnerable=value;MCObjectHeap_touch(p->object.heap);return true;}
bool S39PacketPlayerAbilities_isFlying(const S39PacketPlayerAbilities *p) {return p->flying;}
bool S39PacketPlayerAbilities_setFlying(S39PacketPlayerAbilities *p,bool value) {if (!valid(p)) return false;p->flying=value;MCObjectHeap_touch(p->object.heap);return true;}
bool S39PacketPlayerAbilities_isAllowFlying(const S39PacketPlayerAbilities *p) {return p->allowFlying;}
bool S39PacketPlayerAbilities_setAllowFlying(S39PacketPlayerAbilities *p,bool value) {if (!valid(p)) return false;p->allowFlying=value;MCObjectHeap_touch(p->object.heap);return true;}
bool S39PacketPlayerAbilities_isCreativeMode(const S39PacketPlayerAbilities *p) {return p->creativeMode;}
bool S39PacketPlayerAbilities_setCreativeMode(S39PacketPlayerAbilities *p,bool value) {if (!valid(p)) return false;p->creativeMode=value;MCObjectHeap_touch(p->object.heap);return true;}
bool S39PacketPlayerAbilities_setFlySpeed(S39PacketPlayerAbilities *p,float value) {if (!valid(p)) return false;p->flySpeed=value;MCObjectHeap_touch(p->object.heap);return true;}
float S39PacketPlayerAbilities_getFlySpeed(const S39PacketPlayerAbilities *p) {return p->flySpeed;}
bool S39PacketPlayerAbilities_setWalkSpeed(S39PacketPlayerAbilities *p,float value) {if (!valid(p)) return false;p->walkSpeed=value;MCObjectHeap_touch(p->object.heap);return true;}
float S39PacketPlayerAbilities_getWalkSpeed(const S39PacketPlayerAbilities *p) {return p->walkSpeed;}
