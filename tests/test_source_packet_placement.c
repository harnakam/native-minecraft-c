#include "entity/DataWatcher.h"
#include "network/play/client/C08PacketPlayerBlockPlacement.h"
#include "nbt/NBTTagCompound.h"
#include "nbt/NBTTagInt.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"source C08: %s at %d\n",#x,__LINE__); exit(1); } } while (0)
static MCObjectHeap *heap_new(void) {
    MCObjectHeap *heap=MCObjectHeap_new(4*1024*1024); CHECK(heap); return heap;
}
static void view(PacketBuffer *packet,MCObjectHeap *heap,mc_buf *buffer) {
    CHECK(PacketBuffer_init(packet,heap,buffer));
}
static void bytes_equal(const mc_buf *buffer,const char *hex) {
    CHECK(!buffer->failed&&buffer->len*2==strlen(hex));
    for (size_t i=0;i<buffer->len;i++) {
        unsigned byte=0; CHECK(sscanf(hex+i*2,"%2x",&byte)==1); CHECK(buffer->data[i]==byte);
    }
}

/* Source constructors preserve position identity and copy each ItemStack/NBT
   edge. Count zero and negative counts are objects, rather than empty slots. */
static void constructor_reference_edges(void) {
    MCObjectHeap *heap=heap_new();
    DataWatcherBlockPos *position=DataWatcher_blockPos(heap,7,-9,11); CHECK(position);
    ItemStack *source=ItemStack_new(heap,ItemStack_registryItem(387),0,7); CHECK(source);
    NBTTagCompound *tag=NBTTagCompound_new(heap),*child=NBTTagCompound_new(heap); CHECK(tag&&child);
    CHECK(NBTTagCompound_setInteger_ascii(child,"value",42));
    CHECK(NBTTagCompound_setTag_ascii(tag,"a",(NBTBase *)child));
    CHECK(NBTTagCompound_setTag_ascii(tag,"b",(NBTBase *)child));
    CHECK(ItemStack_setTagCompound(source,tag));
    source->animationsToGo=8;
    C08PacketPlayerBlockPlacement *packet=C08PacketPlayerBlockPlacement_new(heap,position,999,source,.25f,.5f,.75f);
    CHECK(packet&&C08PacketPlayerBlockPlacement_isInstance((MCObject *)packet));
    CHECK(!C08PacketPlayerBlockPlacement_isInstance((MCObject *)source));
    CHECK(C08PacketPlayerBlockPlacement_getPosition(packet)==position);
    CHECK(C08PacketPlayerBlockPlacement_getStack(packet)!=source);
    CHECK(packet->stack->stackSize==0&&packet->stack->itemDamage==7&&packet->stack->animationsToGo==0);
    NBTTagCompound *copy=packet->stack->stackTagCompound;
    NBTBase *a=NBTTagCompound_getTag_ascii(copy,"a"),*b=NBTTagCompound_getTag_ascii(copy,"b");
    CHECK(copy!=tag&&a!=b&&a!=(NBTBase *)child&&b!=(NBTBase *)child);
    CHECK(NBTTagCompound_getInteger_ascii((NBTTagCompound *)a,"value")==42);
    CHECK(NBTTagCompound_setInteger_ascii(child,"value",-1));
    CHECK(NBTTagCompound_getInteger_ascii((NBTTagCompound *)a,"value")==42);
    source->stackSize=-128; CHECK(packet->stack->stackSize==0);
    CHECK(C08PacketPlayerBlockPlacement_getPlacedBlockDirection(packet)==999);
    CHECK(C08PacketPlayerBlockPlacement_getPlacedBlockOffsetX(packet)==.25f);
    CHECK(C08PacketPlayerBlockPlacement_getPlacedBlockOffsetY(packet)==.5f);
    CHECK(C08PacketPlayerBlockPlacement_getPlacedBlockOffsetZ(packet)==.75f);
    C08PacketPlayerBlockPlacement *second=C08PacketPlayerBlockPlacement_new(heap,position,0,source,0,0,0);
    CHECK(second&&second->position==packet->position&&second->stack!=packet->stack&&second->stack->stackSize==-128);
    packet->stack->stackSize=-1; CHECK(C08PacketPlayerBlockPlacement_getStack(packet)->stackSize==-1);
    C08PacketPlayerBlockPlacement *empty=C08PacketPlayerBlockPlacement_new_empty(heap); CHECK(empty);
    CHECK(!empty->position&&!empty->stack&&empty->placedBlockDirection==0);
    CHECK(empty->facingX==0&&empty->facingY==0&&empty->facingZ==0);
    CHECK(!C08PacketPlayerBlockPlacement_isInstance(NULL));
    MCObjectHeap_free(heap);
}

/* A class static is shared even after collection and graph adoption, without
   equating a separately constructed position with equal coordinates. */
static void static_reference_lifetime(void) {
    MCObjectHeap *heap=heap_new();
    C08PacketPlayerBlockPlacement *a=C08PacketPlayerBlockPlacement_new_useItem(heap,NULL);
    C08PacketPlayerBlockPlacement *b=C08PacketPlayerBlockPlacement_new_useItem(heap,NULL); CHECK(a&&b);
    CHECK(a->position==b->position&&a->position->vec3i.x==-1&&a->position->vec3i.y==-1&&a->position->vec3i.z==-1);
    DataWatcherBlockPos *equal=DataWatcher_blockPos(heap,-1,-1,-1); CHECK(equal);
    C08PacketPlayerBlockPlacement *full=C08PacketPlayerBlockPlacement_new(heap,equal,255,NULL,0,0,0); CHECK(full);
    CHECK(full->position==equal&&full->position!=a->position);
    MCObjectRoot root={0}; CHECK(MCObjectRoot_init(&root,heap,(MCObject *)a));
    CHECK(MCObjectHeap_collect(heap));
    a=(C08PacketPlayerBlockPlacement *)MCObjectRoot_get(&root);
    b=C08PacketPlayerBlockPlacement_new_useItem(heap,NULL); CHECK(b&&b->position==a->position);
    MCObjectHeap *working=MCObjectHeap_clone(heap); CHECK(working);
    MCObjectRoot copy={0}; CHECK(MCObjectRoot_rebind(&copy,working,&root));
    C08PacketPlayerBlockPlacement *wa=(C08PacketPlayerBlockPlacement *)MCObjectRoot_get(&copy);
    C08PacketPlayerBlockPlacement *wb=C08PacketPlayerBlockPlacement_new_useItem(working,NULL); CHECK(wb);
    CHECK(wa->position==wb->position&&wa->position!=a->position);
    CHECK(MCObjectHeap_adopt(heap,working)); MCObjectHeap_free(working);
    a=(C08PacketPlayerBlockPlacement *)MCObjectRoot_get(&root);
    b=C08PacketPlayerBlockPlacement_new_useItem(heap,NULL); CHECK(b&&b->position==a->position);
    DataWatcherBlockPos *static_position=b->position;
    MCObjectRoot_drop(&root); CHECK(MCObjectHeap_collect(heap));
    b=C08PacketPlayerBlockPlacement_new_useItem(heap,NULL); CHECK(b&&b->position==static_position);
    MCObjectHeap_free(heap);
}

/* Literal numerical vectors were checked against actual 1.8.9 classes. This
   catches signed coordinate packing, Java narrowing, and float saturation. */
static void actual_wire_vectors(void) {
    static const struct { int32_t x,y,z,direction; float a,b,c; const char *wire; int32_t rx,ry,rz; } cases[]={
        {-33554432,2047,33554431,259,-.1f,15.9375f,INFINITY,"8000001ffdffffff03ffffffffff",-33554432,2047,33554431},
        {0,-2048,0,255,NAN,-INFINITY,-256.5f,"0000002000000000ffffff0000f8",0,-2048,0},
        {INT32_MIN,INT32_MIN,INT32_MAX,-1,3.402823466e38f,-3.402823466e38f,-0.f,"0000000003ffffffffffffff0000",0,0,-1},
        {1,-1,-1,128,.03125f,.0625f,.999f,"0000007fffffffff80ffff00010f",1,-1,-1},
        {1,-1,-1,256,1,16,-16.0625f,"0000007fffffffff00ffff1000ff",1,-1,-1},
        {1,-1,-1,255,2147483520.f/16.f,-2147483520.f/16.f,0,"0000007fffffffffffffff808000",1,-1,-1}
    };
    MCObjectHeap *heap=heap_new(); mc_buf buffer={0}; PacketBuffer io; view(&io,heap,&buffer);
    for (size_t i=0;i<sizeof(cases)/sizeof(*cases);i++) {
        DataWatcherBlockPos *position=DataWatcher_blockPos(heap,cases[i].x,cases[i].y,cases[i].z); CHECK(position);
        C08PacketPlayerBlockPlacement *packet=C08PacketPlayerBlockPlacement_new(heap,position,cases[i].direction,NULL,cases[i].a,cases[i].b,cases[i].c); CHECK(packet);
        CHECK(packet->position==position&&packet->placedBlockDirection==cases[i].direction);
        CHECK(C08PacketPlayerBlockPlacement_writePacketData(packet,&io)); bytes_equal(&buffer,cases[i].wire);
        C08PacketPlayerBlockPlacement *read=C08PacketPlayerBlockPlacement_new_empty(heap); CHECK(read);
        CHECK(C08PacketPlayerBlockPlacement_readPacketData(read,&io)&&buffer.pos==buffer.len);
        CHECK(read->position!=position&&read->position->vec3i.x==cases[i].rx&&read->position->vec3i.y==cases[i].ry&&read->position->vec3i.z==cases[i].rz);
        CHECK(read->placedBlockDirection==(int32_t)(uint8_t)cases[i].direction&&!read->stack);
        CHECK(read->facingX==(float)buffer.data[11]/16&&read->facingY==(float)buffer.data[12]/16&&read->facingZ==(float)buffer.data[13]/16);
        mc_buf_clear(&buffer);
    }
    static const struct { int32_t count,damage; const char *wire; int32_t decoded_count,decoded_damage; } items[]={
        {0,7,"ffffffffffffffffff018b00000700000000",0,7},
        {-1,7,"ffffffffffffffffff018bff000700000000",-1,7},
        {-128,7,"ffffffffffffffffff018b80000700000000",-128,7},
        {128,7,"ffffffffffffffffff018b80000700000000",-128,7},
        {INT32_MIN,65535,"ffffffffffffffffff018b00ffff00000000",0,0}
    };
    for (size_t i=0;i<sizeof(items)/sizeof(*items);i++) {
        ItemStack *stack=ItemStack_new(heap,ItemStack_registryItem(395),items[i].count,items[i].damage); CHECK(stack);
        C08PacketPlayerBlockPlacement *packet=C08PacketPlayerBlockPlacement_new_useItem(heap,stack); CHECK(packet);
        CHECK(packet->stack!=stack&&packet->stack->stackSize==items[i].count);
        CHECK(C08PacketPlayerBlockPlacement_writePacketData(packet,&io)); bytes_equal(&buffer,items[i].wire);
        C08PacketPlayerBlockPlacement *read=C08PacketPlayerBlockPlacement_new_empty(heap); CHECK(read&&C08PacketPlayerBlockPlacement_readPacketData(read,&io));
        CHECK(read->stack&&read->stack->stackSize==items[i].decoded_count&&read->stack->itemDamage==items[i].decoded_damage);
        mc_buf_clear(&buffer);
    }
    mc_buf_free(&buffer); MCObjectHeap_free(heap);
}

/* Failed reads preserve each completed source assignment, including a newly
   decoded nullable stack, rather than rolling the whole packet back. */
static void partial_read_assignment(void) {
    static const uint8_t bytes[]={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x01,0x8b,0x80,0x00,0x07,0x00,0x10,0x20,0x30};
    for (size_t length=0;length<sizeof(bytes);length++) {
        MCObjectHeap *heap=heap_new(); DataWatcherBlockPos *oldpos=DataWatcher_blockPos(heap,1,2,3); CHECK(oldpos);
        ItemStack *old=ItemStack_new(heap,ItemStack_registryItem(1),4,0); CHECK(old);
        C08PacketPlayerBlockPlacement *packet=C08PacketPlayerBlockPlacement_new(heap,oldpos,77,old,4,5,6); CHECK(packet);
        ItemStack *oldcopy=packet->stack;
        mc_buf buffer={(uint8_t *)bytes,length,length,0,false}; PacketBuffer io; view(&io,heap,&buffer);
        CHECK(!C08PacketPlayerBlockPlacement_readPacketData(packet,&io)&&buffer.failed);
        CHECK(!MCObjectHeap_failed(heap)&&!MCObjectHeap_hasBorrowers(heap));
        CHECK(length<8 ? packet->position==oldpos : packet->position!=oldpos&&packet->position->vec3i.x==-1&&packet->position->vec3i.y==-1&&packet->position->vec3i.z==-1);
        CHECK(packet->placedBlockDirection==(length>=9 ? 255 : 77));
        CHECK(length<15 ? packet->stack==oldcopy : packet->stack!=oldcopy&&packet->stack->stackSize==-128&&packet->stack->itemDamage==7);
        CHECK(packet->facingX==(length>=16 ? 1.f : 4.f));
        CHECK(packet->facingY==(length>=17 ? 2.f : 5.f)); CHECK(packet->facingZ==6.f);
        CHECK(buffer.pos<=buffer.len); MCObjectHeap_free(heap);
    }
    static const uint8_t empty[]={0,0,0,0,0,0,0,0,0x81,0xff,0xff,1,2,3};
    MCObjectHeap *heap=heap_new(); ItemStack *source=ItemStack_new(heap,ItemStack_registryItem(1),0,0); CHECK(source);
    C08PacketPlayerBlockPlacement *packet=C08PacketPlayerBlockPlacement_new_useItem(heap,source); CHECK(packet);
    mc_buf buffer={(uint8_t *)empty,11,11,0,false}; PacketBuffer io; view(&io,heap,&buffer);
    CHECK(!C08PacketPlayerBlockPlacement_readPacketData(packet,&io));
    CHECK(!packet->stack&&packet->placedBlockDirection==129&&packet->position->vec3i.x==0);
    MCObjectHeap_free(heap);
}

typedef struct { MCObject object; C08PacketPlayerBlockPlacement *last; int calls; bool refuse; } Handler;
static void handler_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    Handler *handler=(Handler *)object; handler->last=(C08PacketPlayerBlockPlacement *)visitor((MCObject *)handler->last,context);
}
static const MCObjectClass handler_class={"test.PlacementHandler",MCObjectHeap_plainClone,handler_trace,NULL};
static bool placed(MCObject *object,C08PacketPlayerBlockPlacement *packet) {
    Handler *handler=(Handler *)object;
    CHECK(MCObjectHeap_hasBorrowers(object->heap)&&!MCObjectHeap_collect(object->heap));
    handler->last=packet; ++handler->calls; return !handler->refuse;
}
static void process_lifetime_and_errors(void) {
    static const INetHandlerPlayServerMethods methods={.processPlayerBlockPlacement=placed};
    MCObjectHeap *heap=heap_new(); Handler *handler=(Handler *)MCObjectHeap_alloc(heap,sizeof(*handler),&handler_class); CHECK(handler);
    C08PacketPlayerBlockPlacement *packet=C08PacketPlayerBlockPlacement_new_useItem(heap,NULL); CHECK(packet);
    CHECK(C08PacketPlayerBlockPlacement_processPacket(packet,(INetHandlerPlayServer){(MCObject *)handler,&methods}));
    CHECK(handler->last==packet&&handler->calls==1&&!MCObjectHeap_hasBorrowers(heap));
    MCObjectRoot root={0}; CHECK(MCObjectRoot_init(&root,heap,(MCObject *)handler)); CHECK(MCObjectHeap_collect(heap));
    CHECK(handler->last==packet&&packet->position->vec3i.x==-1);
    handler->refuse=true; CHECK(!C08PacketPlayerBlockPlacement_processPacket(packet,(INetHandlerPlayServer){(MCObject *)handler,&methods}));
    CHECK(MCObjectHeap_failed(heap)&&!MCObjectHeap_hasBorrowers(heap)); MCObjectHeap_free(heap);
    heap=heap_new(); packet=C08PacketPlayerBlockPlacement_new_empty(heap); CHECK(packet);
    CHECK(!C08PacketPlayerBlockPlacement_processPacket(packet,(INetHandlerPlayServer){0})&&MCObjectHeap_failed(heap)); MCObjectHeap_free(heap);
    heap=heap_new(); handler=(Handler *)MCObjectHeap_alloc(heap,sizeof(*handler),&handler_class); CHECK(handler);
    packet=C08PacketPlayerBlockPlacement_new_empty(heap); CHECK(packet);
    static const INetHandlerPlayServerMethods missing={0};
    CHECK(!C08PacketPlayerBlockPlacement_processPacket(packet,(INetHandlerPlayServer){(MCObject *)handler,&missing})&&MCObjectHeap_failed(heap)); MCObjectHeap_free(heap);
    heap=heap_new(); MCObjectHeap *foreign=heap_new();
    handler=(Handler *)MCObjectHeap_alloc(foreign,sizeof(*handler),&handler_class); packet=C08PacketPlayerBlockPlacement_new_empty(heap); CHECK(handler&&packet);
    CHECK(!C08PacketPlayerBlockPlacement_processPacket(packet,(INetHandlerPlayServer){(MCObject *)handler,&methods})&&MCObjectHeap_failed(heap));
    CHECK(!MCObjectHeap_failed(foreign)); MCObjectHeap_free(heap); MCObjectHeap_free(foreign);
}

/* Null position fails before its bytes; an invalid Item/NBT write retains the
   already emitted position/direction/item prefix, with no offset bytes. */
static void writer_failure_and_heap_guards(void) {
    MCObjectHeap *heap=heap_new(); mc_buf buffer={0}; PacketBuffer io; view(&io,heap,&buffer);
    C08PacketPlayerBlockPlacement *packet=C08PacketPlayerBlockPlacement_new_empty(heap); CHECK(packet);
    mc_put_u8(&buffer,0x55);
    CHECK(!C08PacketPlayerBlockPlacement_writePacketData(packet,&io)&&buffer.len==1&&buffer.data[0]==0x55);
    CHECK(!MCObjectHeap_hasBorrowers(heap)); mc_buf_free(&buffer); MCObjectHeap_free(heap);
    heap=heap_new(); view(&io,heap,&buffer);
    packet=C08PacketPlayerBlockPlacement_new(heap,NULL,6,NULL,1,2,3); CHECK(packet&&!packet->position&&packet->placedBlockDirection==6);
    CHECK(!C08PacketPlayerBlockPlacement_writePacketData(packet,&io)&&buffer.len==0); mc_buf_free(&buffer); MCObjectHeap_free(heap);
    heap=heap_new(); view(&io,heap,&buffer); ItemStack *stack=ItemStack_new(heap,NULL,-1,7); CHECK(stack);
    packet=C08PacketPlayerBlockPlacement_new_useItem(heap,stack); CHECK(packet);
    CHECK(!C08PacketPlayerBlockPlacement_writePacketData(packet,&io)&&buffer.len==14&&MCObjectHeap_failed(heap));
    CHECK(buffer.data[8]==255&&buffer.data[11]==255&&buffer.data[12]==0&&buffer.data[13]==7);
    mc_buf_free(&buffer); MCObjectHeap_free(heap);
    heap=heap_new(); view(&io,heap,&buffer);
    stack=ItemStack_new(heap,ItemStack_registryItem(395),0,0); CHECK(stack);
    packet=C08PacketPlayerBlockPlacement_new_useItem(heap,stack); CHECK(packet);
    NBTTagCompound *cycle=NBTTagCompound_new(heap); CHECK(cycle&&NBTTagCompound_setTag_ascii(cycle,"self",(NBTBase *)cycle));
    CHECK(ItemStack_setTagCompound(packet->stack,cycle));
    CHECK(!C08PacketPlayerBlockPlacement_writePacketData(packet,&io)&&buffer.len>14&&buffer.failed);
    CHECK(!MCObjectHeap_hasBorrowers(heap)); mc_buf_free(&buffer); MCObjectHeap_free(heap);
    heap=heap_new(); MCObjectHeap *foreign=heap_new(); packet=C08PacketPlayerBlockPlacement_new_useItem(heap,NULL); CHECK(packet);
    view(&io,foreign,&buffer); CHECK(!C08PacketPlayerBlockPlacement_writePacketData(packet,&io)&&buffer.failed&&buffer.len==0&&MCObjectHeap_failed(heap));
    mc_buf_free(&buffer); MCObjectHeap_free(heap); MCObjectHeap_free(foreign);
    heap=heap_new(); foreign=heap_new(); DataWatcherBlockPos *pos=DataWatcher_blockPos(foreign,1,2,3); CHECK(pos);
    CHECK(!C08PacketPlayerBlockPlacement_new(heap,pos,1,NULL,0,0,0)&&MCObjectHeap_failed(heap));
    CHECK(pos->vec3i.x==1&&!MCObjectHeap_failed(foreign)); MCObjectHeap_free(heap); MCObjectHeap_free(foreign);
    heap=heap_new(); foreign=heap_new(); stack=ItemStack_new(foreign,ItemStack_registryItem(1),4,0); CHECK(stack);
    CHECK(!C08PacketPlayerBlockPlacement_new_useItem(heap,stack)&&MCObjectHeap_failed(heap));
    CHECK(stack->stackSize==4&&!MCObjectHeap_failed(foreign)); MCObjectHeap_free(heap); MCObjectHeap_free(foreign);
    heap=MCObjectHeap_new(sizeof(C08PacketPlayerBlockPlacement)); CHECK(heap);
    CHECK(!C08PacketPlayerBlockPlacement_new_empty(heap)&&MCObjectHeap_failed(heap)); CHECK(!MCObjectHeap_hasBorrowers(heap)); MCObjectHeap_free(heap);
}

int main(void) {
    constructor_reference_edges(); static_reference_lifetime(); actual_wire_vectors();
    partial_read_assignment(); process_lifetime_and_errors(); writer_failure_and_heap_guards();
    printf("source C08 placement: %u checks passed\n",checks); return 0;
}
