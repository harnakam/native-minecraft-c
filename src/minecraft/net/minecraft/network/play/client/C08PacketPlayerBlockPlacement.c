#include "network/play/client/C08PacketPlayerBlockPlacement.h"
#include "network/play/client/native_packet.h"
#include <math.h>

/* Native class-static storage is itself a heap root, so complete graph
   snapshots retain the static reference and its aliases. It has no external
   heap-pointer table and cannot point back into a discarded parent graph. */
typedef struct {
    MCObject object;
    DataWatcherBlockPos *field_179726_a;
} PlacementStatics;
static void statics_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    PlacementStatics *fields=(PlacementStatics *)object;
    fields->field_179726_a=(DataWatcherBlockPos *)visitor((MCObject *)fields->field_179726_a,context);
}
static const MCObjectClass statics_class={"native.C08PacketPlayerBlockPlacement.statics",MCObjectHeap_plainClone,statics_trace,NULL};
static bool any_static(const MCObject *object,void *context) { (void)object; (void)context; return true; }
static PlacementStatics *class_statics(MCObjectHeap *heap) {
    PlacementStatics *fields=(PlacementStatics *)MCObjectHeap_findObject(heap,&statics_class,any_static,NULL);
    if (fields) return fields;
    fields=(PlacementStatics *)MCObjectHeap_alloc(heap,sizeof(*fields),&statics_class);
    if (!fields) return NULL;
    fields->field_179726_a=DataWatcher_blockPos(heap,-1,-1,-1);
    if (!fields->field_179726_a) return NULL;
    /* The private class root intentionally lives until this heap is freed. */
    MCObjectRoot root={0};
    return MCObjectRoot_init(&root,heap,(MCObject *)fields) ? fields : NULL;
}
static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    C08PacketPlayerBlockPlacement *packet=(C08PacketPlayerBlockPlacement *)object;
    packet->position=(DataWatcherBlockPos *)visitor((MCObject *)packet->position,context);
    packet->stack=(ItemStack *)visitor((MCObject *)packet->stack,context);
}
static const MCObjectClass packet_class={"net.minecraft.network.play.client.C08PacketPlayerBlockPlacement",MCObjectHeap_plainClone,trace,NULL};
bool C08PacketPlayerBlockPlacement_isInstance(const MCObject *object) { return object&&object->klass==&packet_class; }
C08PacketPlayerBlockPlacement *C08PacketPlayerBlockPlacement_new_empty(MCObjectHeap *heap) {
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,heap)) return NULL;
    C08PacketPlayerBlockPlacement *packet=class_statics(heap) ?
        (C08PacketPlayerBlockPlacement *)MCObjectHeap_alloc(heap,sizeof(*packet),&packet_class) : NULL;
    MCObjectRootScope_end(&scope); return packet;
}
C08PacketPlayerBlockPlacement *C08PacketPlayerBlockPlacement_new(MCObjectHeap *heap,
    DataWatcherBlockPos *positionIn,int32_t direction,ItemStack *stackIn,float x,float y,float z) {
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,heap)) return NULL;
    PlacementStatics *fields=class_statics(heap);
    bool valid=fields&&MCObjectRootScope_pin(&scope,(MCObject *)positionIn)&&
        MCObjectRootScope_pin(&scope,(MCObject *)stackIn);
    if (valid && ((positionIn&&!BlockPos_isInstance((MCObject *)positionIn))||
                  (stackIn&&!ItemStack_isInstance((MCObject *)stackIn)))) {
        MCObjectHeap_fail(heap); valid=false;
    }
    C08PacketPlayerBlockPlacement *packet=valid ? C08PacketPlayerBlockPlacement_new_empty(heap) : NULL;
    if (packet) {
        packet->position=positionIn; packet->placedBlockDirection=direction;
        packet->stack=stackIn ? ItemStack_copy(heap,stackIn) : NULL;
        if (stackIn&&!packet->stack) packet=NULL;
        if (packet) { packet->facingX=x; packet->facingY=y; packet->facingZ=z; }
    }
    MCObjectRootScope_end(&scope); return packet;
}
C08PacketPlayerBlockPlacement *C08PacketPlayerBlockPlacement_new_useItem(MCObjectHeap *heap,ItemStack *stackIn) {
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,heap)) return NULL;
    PlacementStatics *fields=class_statics(heap);
    C08PacketPlayerBlockPlacement *packet=fields ?
        C08PacketPlayerBlockPlacement_new(heap,fields->field_179726_a,255,stackIn,0,0,0) : NULL;
    MCObjectRootScope_end(&scope); return packet;
}
bool C08PacketPlayerBlockPlacement_readPacketData(C08PacketPlayerBlockPlacement *packet,PacketBuffer *buffer) {
    MCObjectRootScope scope={0}; if (!mc_packet_begin((MCObject *)packet,buffer,&scope)) return false;
    int64_t packed=mc_get_i64(buffer->buffer); if (buffer->buffer->failed) goto done;
    DataWatcherBlockPos *position=BlockPos_fromLong(buffer->heap,packed); if (!position) goto done;
    packet->position=position;
    int32_t value=mc_get_u8(buffer->buffer); if (buffer->buffer->failed) goto done;
    packet->placedBlockDirection=value;
    if (!PacketBuffer_readItemStackFromBuffer(buffer,&packet->stack)) goto done;
    value=mc_get_u8(buffer->buffer); if (buffer->buffer->failed) goto done; packet->facingX=(float)value/16.0f;
    value=mc_get_u8(buffer->buffer); if (buffer->buffer->failed) goto done; packet->facingY=(float)value/16.0f;
    value=mc_get_u8(buffer->buffer); if (buffer->buffer->failed) goto done; packet->facingZ=(float)value/16.0f;
done:
    return mc_packet_finish((MCObject *)packet,buffer,&scope,!buffer->buffer->failed);
}
static uint8_t java_float_to_byte(float value) {
    int32_t integer=isnan(value) ? 0 : value>=2147483648.0f ? INT32_MAX :
        value<=-2147483648.0f ? INT32_MIN : (int32_t)value;
    return (uint8_t)(uint32_t)integer;
}
bool C08PacketPlayerBlockPlacement_writePacketData(C08PacketPlayerBlockPlacement *packet,PacketBuffer *buffer) {
    MCObjectRootScope scope={0}; if (!mc_packet_begin((MCObject *)packet,buffer,&scope)) return false;
    bool ok=packet->position&&MCObjectRootScope_pin(&scope,(MCObject *)packet->position)&&
        BlockPos_isInstance((MCObject *)packet->position);
    if (!ok) { MCObjectHeap_fail(packet->object.heap); goto done; }
    int64_t packed;
    ok=BlockPos_toLong(packet->position,&packed)==NATIVE_ARRAY_OK;
    if (!ok) { MCObjectHeap_fail(packet->object.heap); goto done; }
    mc_put_i64(buffer->buffer,packed);
    if (buffer->buffer->failed) goto done;
    mc_put_u8(buffer->buffer,(uint8_t)packet->placedBlockDirection);
    if (buffer->buffer->failed) goto done;
    ok=PacketBuffer_writeItemStackToBuffer(buffer,packet->stack); if (!ok) goto done;
    mc_put_u8(buffer->buffer,java_float_to_byte(packet->facingX*16.0f));
    if (buffer->buffer->failed) goto done;
    mc_put_u8(buffer->buffer,java_float_to_byte(packet->facingY*16.0f));
    if (buffer->buffer->failed) goto done;
    mc_put_u8(buffer->buffer,java_float_to_byte(packet->facingZ*16.0f));
done:
    return mc_packet_finish((MCObject *)packet,buffer,&scope,ok);
}
bool C08PacketPlayerBlockPlacement_processPacket(C08PacketPlayerBlockPlacement *packet,INetHandlerPlayServer handler) {
    MCObjectRootScope scope={0}; if (!mc_packet_handler_begin((MCObject *)packet,handler,&scope)) return false;
    bool ok=handler.methods->processPlayerBlockPlacement&&handler.methods->processPlayerBlockPlacement(handler.instance,packet);
    return mc_packet_handler_finish((MCObject *)packet,&scope,ok);
}
DataWatcherBlockPos *C08PacketPlayerBlockPlacement_getPosition(const C08PacketPlayerBlockPlacement *packet) { return packet->position; }
int32_t C08PacketPlayerBlockPlacement_getPlacedBlockDirection(const C08PacketPlayerBlockPlacement *packet) { return packet->placedBlockDirection; }
ItemStack *C08PacketPlayerBlockPlacement_getStack(const C08PacketPlayerBlockPlacement *packet) { return packet->stack; }
float C08PacketPlayerBlockPlacement_getPlacedBlockOffsetX(const C08PacketPlayerBlockPlacement *packet) { return packet->facingX; }
float C08PacketPlayerBlockPlacement_getPlacedBlockOffsetY(const C08PacketPlayerBlockPlacement *packet) { return packet->facingY; }
float C08PacketPlayerBlockPlacement_getPlacedBlockOffsetZ(const C08PacketPlayerBlockPlacement *packet) { return packet->facingZ; }
