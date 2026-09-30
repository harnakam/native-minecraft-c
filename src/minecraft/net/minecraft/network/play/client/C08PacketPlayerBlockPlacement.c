#include "C08PacketPlayerBlockPlacement.h"
#include <math.h>
#include <string.h>

static const C08BlockPos field_179726_a = {-1,-1,-1};
void C08PacketPlayerBlockPlacement_init(C08PacketPlayerBlockPlacement *packet) {
    memset(packet,0,sizeof(*packet)); mc_slot_init(&packet->stack);
}
void C08PacketPlayerBlockPlacement_free(C08PacketPlayerBlockPlacement *packet) {
    mc_slot_free(&packet->stack); C08PacketPlayerBlockPlacement_init(packet);
}
bool C08PacketPlayerBlockPlacement_construct(C08PacketPlayerBlockPlacement *packet,
    const C08BlockPos *positionIn,int placedBlockDirectionIn,const mc_slot *stackIn,
    float facingXIn,float facingYIn,float facingZIn) {
    C08PacketPlayerBlockPlacement next; C08PacketPlayerBlockPlacement_init(&next);
    if (positionIn) { next.position=*positionIn; next.hasPosition=true; }
    next.placedBlockDirection=placedBlockDirectionIn;
    if (stackIn && !mc_slot_copy(&next.stack,stackIn)) return false;
    next.facingX=facingXIn; next.facingY=facingYIn; next.facingZ=facingZIn;
    C08PacketPlayerBlockPlacement_free(packet); *packet=next; return true;
}
bool C08PacketPlayerBlockPlacement_constructUseItem(C08PacketPlayerBlockPlacement *packet,const mc_slot *stackIn) {
    return C08PacketPlayerBlockPlacement_construct(packet,&field_179726_a,255,stackIn,0,0,0);
}
bool C08PacketPlayerBlockPlacement_readPacketData(C08PacketPlayerBlockPlacement *packet,mc_buf *buffer) {
    mc_buf input=*buffer; C08PacketPlayerBlockPlacement next; C08PacketPlayerBlockPlacement_init(&next);
    mc_get_position(&input,&next.position.x,&next.position.y,&next.position.z);
    /* The old position codec exposes unsigned Y; original BlockPos.fromLong
       sign-extends all three axes. Adapt locally without changing other users. */
    if (next.position.y>=2048) next.position.y-=4096;
    next.hasPosition=true;
    next.placedBlockDirection=mc_get_u8(&input);
    bool ok=mc_slot_read(&input,&next.stack);
    next.facingX=(float)mc_get_u8(&input)/16.0f;
    next.facingY=(float)mc_get_u8(&input)/16.0f;
    next.facingZ=(float)mc_get_u8(&input)/16.0f;
    if (!ok || input.failed) { C08PacketPlayerBlockPlacement_free(&next); buffer->failed=true; return false; }
    C08PacketPlayerBlockPlacement_free(packet); *packet=next; buffer->pos=input.pos; return true;
}
static uint8_t java_float_to_byte(float value) {
    /* Java narrows float to saturating int first, then int to eight bits. */
    int32_t integer=isnan(value) ? 0 : value>=2147483648.0f ? INT32_MAX : value<=-2147483648.0f ? INT32_MIN : (int32_t)value;
    return (uint8_t)(uint32_t)integer;
}
bool C08PacketPlayerBlockPlacement_writePacketData(const C08PacketPlayerBlockPlacement *packet,mc_buf *buffer) {
    if (!packet->hasPosition) { buffer->failed=true; return false; }
    mc_buf output; mc_buf_init(&output);
    mc_put_position(&output,packet->position.x,packet->position.y,packet->position.z);
    mc_put_u8(&output,(uint8_t)packet->placedBlockDirection);
    bool ok=mc_slot_write(&output,&packet->stack);
    mc_put_u8(&output,java_float_to_byte(packet->facingX*16.0f));
    mc_put_u8(&output,java_float_to_byte(packet->facingY*16.0f));
    mc_put_u8(&output,java_float_to_byte(packet->facingZ*16.0f));
    if (ok && !output.failed) mc_put_bytes(buffer,output.data,output.len);
    else buffer->failed=true;
    mc_buf_free(&output); return !buffer->failed;
}
void C08PacketPlayerBlockPlacement_processPacket(const C08PacketPlayerBlockPlacement *packet,
    void *handler,C08ProcessPlayerBlockPlacement processPlayerBlockPlacement) {
    processPlayerBlockPlacement(handler,packet);
}
const C08BlockPos *C08PacketPlayerBlockPlacement_getPosition(const C08PacketPlayerBlockPlacement *packet) { return packet->hasPosition ? &packet->position : NULL; }
int C08PacketPlayerBlockPlacement_getPlacedBlockDirection(const C08PacketPlayerBlockPlacement *packet) { return packet->placedBlockDirection; }
const mc_slot *C08PacketPlayerBlockPlacement_getStack(const C08PacketPlayerBlockPlacement *packet) { return packet->stack.item_id<0 ? NULL : &packet->stack; }
float C08PacketPlayerBlockPlacement_getPlacedBlockOffsetX(const C08PacketPlayerBlockPlacement *packet) { return packet->facingX; }
float C08PacketPlayerBlockPlacement_getPlacedBlockOffsetY(const C08PacketPlayerBlockPlacement *packet) { return packet->facingY; }
float C08PacketPlayerBlockPlacement_getPlacedBlockOffsetZ(const C08PacketPlayerBlockPlacement *packet) { return packet->facingZ; }
