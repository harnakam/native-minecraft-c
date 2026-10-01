#include "network/play/client/C03PacketPlayer.h"
#include "network/play/client/native_packet.h"
static const MCObjectClass baseClass={"net.minecraft.network.play.client.C03PacketPlayer",MCObjectHeap_plainClone,NULL,NULL};
static const MCObjectClass positionClass={"net.minecraft.network.play.client.C03PacketPlayer$C04PacketPlayerPosition",MCObjectHeap_plainClone,NULL,NULL};
static const MCObjectClass lookClass={"net.minecraft.network.play.client.C03PacketPlayer$C05PacketPlayerLook",MCObjectHeap_plainClone,NULL,NULL};
static const MCObjectClass positionLookClass={"net.minecraft.network.play.client.C03PacketPlayer$C06PacketPlayerPosLook",MCObjectHeap_plainClone,NULL,NULL};
static bool exact(const MCObject *o,const MCObjectClass *klass) {
    return o&&o->klass==klass&&MCObjectHeap_objectSize(o)>=sizeof(C03PacketPlayer);
}
bool C03PacketPlayer_nativeBaseIsInstance(const MCObject *o) {return exact(o,&baseClass);}
bool C04PacketPlayerPosition_isInstance(const MCObject *o) {return exact(o,&positionClass);}
bool C05PacketPlayerLook_isInstance(const MCObject *o) {return exact(o,&lookClass);}
bool C06PacketPlayerPosLook_isInstance(const MCObject *o) {return exact(o,&positionLookClass);}
bool C03PacketPlayer_isInstance(const MCObject *o) {
    return C03PacketPlayer_nativeBaseIsInstance(o)||C04PacketPlayerPosition_isInstance(o)||
           C05PacketPlayerLook_isInstance(o)||C06PacketPlayerPosLook_isInstance(o);
}
static bool valid(C03PacketPlayer *p) {
    if(!C03PacketPlayer_isInstance((MCObject *)p)||MCObjectHeap_failed(p->object.heap)) {
        MCObjectHeap_fail(p?p->object.heap:NULL);return false;
    }
    return true;
}
static C03PacketPlayer *allocate(MCObjectHeap *heap,const MCObjectClass *klass) {
    return (C03PacketPlayer *)MCObjectHeap_alloc(heap,sizeof(C03PacketPlayer),klass);
}
C03PacketPlayer *C03PacketPlayer_new_empty(MCObjectHeap *heap) {return allocate(heap,&baseClass);}
C03PacketPlayer *C03PacketPlayer_new(MCObjectHeap *heap,bool ground) {
    C03PacketPlayer *p=C03PacketPlayer_new_empty(heap);if(p)p->onGround=ground;return p;
}
C04PacketPlayerPosition *C04PacketPlayerPosition_new_empty(MCObjectHeap *heap) {
    C03PacketPlayer *p=allocate(heap,&positionClass);if(p)p->moving=true;return p;
}
C04PacketPlayerPosition *C04PacketPlayerPosition_new(MCObjectHeap *heap,double x,double y,double z,bool ground) {
    C03PacketPlayer *p=allocate(heap,&positionClass);
    if(p){p->x=x;p->y=y;p->z=z;p->onGround=ground;p->moving=true;}return p;
}
C05PacketPlayerLook *C05PacketPlayerLook_new_empty(MCObjectHeap *heap) {
    C03PacketPlayer *p=allocate(heap,&lookClass);if(p)p->rotating=true;return p;
}
C05PacketPlayerLook *C05PacketPlayerLook_new(MCObjectHeap *heap,float yaw,float pitch,bool ground) {
    C03PacketPlayer *p=allocate(heap,&lookClass);
    if(p){p->yaw=yaw;p->pitch=pitch;p->onGround=ground;p->rotating=true;}return p;
}
C06PacketPlayerPosLook *C06PacketPlayerPosLook_new_empty(MCObjectHeap *heap) {
    C03PacketPlayer *p=allocate(heap,&positionLookClass);if(p){p->moving=true;p->rotating=true;}return p;
}
C06PacketPlayerPosLook *C06PacketPlayerPosLook_new(MCObjectHeap *heap,double x,double y,double z,float yaw,float pitch,bool ground) {
    C03PacketPlayer *p=allocate(heap,&positionLookClass);
    if(p){p->x=x;p->y=y;p->z=z;p->yaw=yaw;p->pitch=pitch;p->onGround=ground;p->rotating=true;p->moving=true;}return p;
}
static bool readDouble(PacketBuffer *b,double *field) {
    double value=mc_get_f64(b->buffer);if(b->buffer->failed)return false;*field=value;MCObjectHeap_touch(b->heap);return true;
}
static bool readFloat(PacketBuffer *b,float *field) {
    float value=mc_get_f32(b->buffer);if(b->buffer->failed)return false;*field=value;MCObjectHeap_touch(b->heap);return true;
}
static bool baseRead(C03PacketPlayer *p,PacketBuffer *b) {
    uint8_t value=mc_get_u8(b->buffer);if(b->buffer->failed)return false;
    p->onGround=value!=0;MCObjectHeap_touch(p->object.heap);return true;
}
static bool baseWrite(C03PacketPlayer *p,PacketBuffer *b) {mc_put_u8(b->buffer,p->onGround?1:0);return !b->buffer->failed;}
static bool readBody(C03PacketPlayer *p,PacketBuffer *b,bool position,bool look) {
    MCObjectRootScope scope={0};if(!valid(p)||!mc_packet_begin((MCObject *)p,b,&scope))return false;
    bool ok=!position||(readDouble(b,&p->x)&&readDouble(b,&p->y)&&readDouble(b,&p->z));
    if(ok&&look)ok=readFloat(b,&p->yaw)&&readFloat(b,&p->pitch);
    if(ok)ok=baseRead(p,b);
    return mc_packet_finish((MCObject *)p,b,&scope,ok);
}
static bool writeBody(C03PacketPlayer *p,PacketBuffer *b,bool position,bool look) {
    MCObjectRootScope scope={0};if(!valid(p)||!mc_packet_begin((MCObject *)p,b,&scope))return false;
    if(position){mc_put_f64(b->buffer,p->x);mc_put_f64(b->buffer,p->y);mc_put_f64(b->buffer,p->z);}
    if(look){mc_put_f32(b->buffer,p->yaw);mc_put_f32(b->buffer,p->pitch);}
    bool ok=!b->buffer->failed&&baseWrite(p,b);
    return mc_packet_finish((MCObject *)p,b,&scope,ok);
}
static bool subtype(C03PacketPlayer *p,const MCObjectClass *klass) {
    if(exact((MCObject *)p,klass))return valid(p);
    MCObjectHeap_fail(p?p->object.heap:NULL);return false;
}
bool C04PacketPlayerPosition_readPacketData(C04PacketPlayerPosition *p,PacketBuffer *b) {return subtype(p,&positionClass)&&readBody(p,b,true,false);}
bool C04PacketPlayerPosition_writePacketData(C04PacketPlayerPosition *p,PacketBuffer *b) {return subtype(p,&positionClass)&&writeBody(p,b,true,false);}
bool C05PacketPlayerLook_readPacketData(C05PacketPlayerLook *p,PacketBuffer *b) {return subtype(p,&lookClass)&&readBody(p,b,false,true);}
bool C05PacketPlayerLook_writePacketData(C05PacketPlayerLook *p,PacketBuffer *b) {return subtype(p,&lookClass)&&writeBody(p,b,false,true);}
bool C06PacketPlayerPosLook_readPacketData(C06PacketPlayerPosLook *p,PacketBuffer *b) {return subtype(p,&positionLookClass)&&readBody(p,b,true,true);}
bool C06PacketPlayerPosLook_writePacketData(C06PacketPlayerPosLook *p,PacketBuffer *b) {return subtype(p,&positionLookClass)&&writeBody(p,b,true,true);}
bool C03PacketPlayer_readPacketData(C03PacketPlayer *p,PacketBuffer *b) {
    if(C04PacketPlayerPosition_isInstance((MCObject *)p))return C04PacketPlayerPosition_readPacketData(p,b);
    if(C05PacketPlayerLook_isInstance((MCObject *)p))return C05PacketPlayerLook_readPacketData(p,b);
    if(C06PacketPlayerPosLook_isInstance((MCObject *)p))return C06PacketPlayerPosLook_readPacketData(p,b);
    return readBody(p,b,false,false);
}
bool C03PacketPlayer_writePacketData(C03PacketPlayer *p,PacketBuffer *b) {
    if(C04PacketPlayerPosition_isInstance((MCObject *)p))return C04PacketPlayerPosition_writePacketData(p,b);
    if(C05PacketPlayerLook_isInstance((MCObject *)p))return C05PacketPlayerLook_writePacketData(p,b);
    if(C06PacketPlayerPosLook_isInstance((MCObject *)p))return C06PacketPlayerPosLook_writePacketData(p,b);
    return writeBody(p,b,false,false);
}
bool C03PacketPlayer_processPacket(C03PacketPlayer *p,INetHandlerPlayServer handler) {
    MCObjectRootScope scope={0};if(!valid(p)||!mc_packet_handler_begin((MCObject *)p,handler,&scope))return false;
    bool ok=handler.methods->processPlayer&&handler.methods->processPlayer(handler.instance,p);
    return mc_packet_handler_finish((MCObject *)p,&scope,ok);
}
double C03PacketPlayer_getPositionX(const C03PacketPlayer *p) {return p->x;}
double C03PacketPlayer_getPositionY(const C03PacketPlayer *p) {return p->y;}
double C03PacketPlayer_getPositionZ(const C03PacketPlayer *p) {return p->z;}
float C03PacketPlayer_getYaw(const C03PacketPlayer *p) {return p->yaw;}
float C03PacketPlayer_getPitch(const C03PacketPlayer *p) {return p->pitch;}
bool C03PacketPlayer_isOnGround(const C03PacketPlayer *p) {return p->onGround;}
bool C03PacketPlayer_isMoving(const C03PacketPlayer *p) {return p->moving;}
bool C03PacketPlayer_getRotating(const C03PacketPlayer *p) {return p->rotating;}
bool C03PacketPlayer_setMoving(C03PacketPlayer *p,bool moving) {
    if(!valid(p))return false;
    p->moving=moving;MCObjectHeap_touch(p->object.heap);return true;
}
