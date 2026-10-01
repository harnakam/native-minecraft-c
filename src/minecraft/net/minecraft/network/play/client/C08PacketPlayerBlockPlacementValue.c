#include "C08PacketPlayerBlockPlacementValue.h"
#include <math.h>
#include <string.h>

static const C08ValueBlockPos field_179726_a = {-1,-1,-1};
void C08PacketPlayerBlockPlacementValue_init(C08PacketPlayerBlockPlacementValue *packet) {
    memset(packet,0,sizeof(*packet)); mc_slot_init(&packet->stack);
}
void C08PacketPlayerBlockPlacementValue_free(C08PacketPlayerBlockPlacementValue *packet) {
    mc_slot_free(&packet->stack); C08PacketPlayerBlockPlacementValue_init(packet);
}
bool C08PacketPlayerBlockPlacementValue_construct(C08PacketPlayerBlockPlacementValue *packet,
    const C08ValueBlockPos *positionIn,int placedBlockDirectionIn,const mc_slot *stackIn,
    float facingXIn,float facingYIn,float facingZIn) {
    C08PacketPlayerBlockPlacementValue next; C08PacketPlayerBlockPlacementValue_init(&next);
    if (positionIn) { next.position=*positionIn; next.hasPosition=true; }
    next.placedBlockDirection=placedBlockDirectionIn;
    if (stackIn && !mc_slot_copy(&next.stack,stackIn)) return false;
    next.facingX=facingXIn; next.facingY=facingYIn; next.facingZ=facingZIn;
    C08PacketPlayerBlockPlacementValue_free(packet); *packet=next; return true;
}
bool C08PacketPlayerBlockPlacementValue_constructUseItem(C08PacketPlayerBlockPlacementValue *packet,const mc_slot *stackIn) {
    return C08PacketPlayerBlockPlacementValue_construct(packet,&field_179726_a,255,stackIn,0,0,0);
}
bool C08PacketPlayerBlockPlacementValue_readPacketData(C08PacketPlayerBlockPlacementValue *packet,mc_buf *buffer) {
    mc_buf input=*buffer; C08PacketPlayerBlockPlacementValue next; C08PacketPlayerBlockPlacementValue_init(&next);
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
    if (!ok || input.failed) { C08PacketPlayerBlockPlacementValue_free(&next); buffer->failed=true; return false; }
    C08PacketPlayerBlockPlacementValue_free(packet); *packet=next; buffer->pos=input.pos; return true;
}
static uint8_t java_float_to_byte(float value) {
    /* Java narrows float to saturating int first, then int to eight bits. */
    int32_t integer=isnan(value) ? 0 : value>=2147483648.0f ? INT32_MAX : value<=-2147483648.0f ? INT32_MIN : (int32_t)value;
    return (uint8_t)(uint32_t)integer;
}
bool C08PacketPlayerBlockPlacementValue_writePacketData(const C08PacketPlayerBlockPlacementValue *packet,mc_buf *buffer) {
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
void C08PacketPlayerBlockPlacementValue_processPacket(const C08PacketPlayerBlockPlacementValue *packet,
    void *handler,C08ValueProcessPlayerBlockPlacement processPlayerBlockPlacement) {
    processPlayerBlockPlacement(handler,packet);
}
const C08ValueBlockPos *C08PacketPlayerBlockPlacementValue_getPosition(const C08PacketPlayerBlockPlacementValue *packet) { return packet->hasPosition ? &packet->position : NULL; }
int C08PacketPlayerBlockPlacementValue_getPlacedBlockDirection(const C08PacketPlayerBlockPlacementValue *packet) { return packet->placedBlockDirection; }
const mc_slot *C08PacketPlayerBlockPlacementValue_getStack(const C08PacketPlayerBlockPlacementValue *packet) { return packet->stack.item_id<0 ? NULL : &packet->stack; }
float C08PacketPlayerBlockPlacementValue_getPlacedBlockOffsetX(const C08PacketPlayerBlockPlacementValue *packet) { return packet->facingX; }
float C08PacketPlayerBlockPlacementValue_getPlacedBlockOffsetY(const C08PacketPlayerBlockPlacementValue *packet) { return packet->facingY; }
float C08PacketPlayerBlockPlacementValue_getPlacedBlockOffsetZ(const C08PacketPlayerBlockPlacementValue *packet) { return packet->facingZ; }
