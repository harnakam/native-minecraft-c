#include "network/play/server/S2EPacketCloseWindow.h"
#include "network/play/server/S2FPacketSetSlot.h"
#include "network/play/server/S30PacketWindowItems.h"
#include "network/play/server/S32PacketConfirmTransaction.h"
#include "inventory/inventory_dispatch.h"
#include "nbt/NBTTagCompound.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"server inventory packets check %u at %d: %s\n",checks,__LINE__,#x);exit(1);}} while(0)
static void view(PacketBuffer *io,MCObjectHeap *heap,mc_buf *buffer) {CHECK(PacketBuffer_init(io,heap,buffer));}
static void bytes(const mc_buf *buffer,const char *hex) {
    CHECK(!buffer->failed&&buffer->len*2==strlen(hex));
    for(size_t i=0;i<buffer->len;i++){unsigned value;CHECK(sscanf(hex+i*2,"%2x",&value)==1);CHECK(buffer->data[i]==value);}
}
typedef struct {MCObject object;InventoryPlayer *inventory;bool creative;} FixturePlayer;
static void player_trace(MCObject *o,MCObjectVisitor visitor,void *context) {FixturePlayer *p=(FixturePlayer *)o;p->inventory=(InventoryPlayer *)visitor((MCObject *)p->inventory,context);}
static const MCObjectClass player_class={"fixture.packet.list-player",MCObjectHeap_plainClone,player_trace,NULL};
static bool creative(const MCObject *o) {return ((const FixturePlayer *)o)->creative;}
static void container_trace(MCObject *o,MCObjectVisitor visitor,void *context) {Container_trace((Container *)o,visitor,context);}
static const MCObjectClass container_class={"fixture.packet.list-container",MCObjectHeap_plainClone,container_trace,NULL};
/* Real source Container/Slot/InventoryPlayer methods create the List input.
   No raw-array constructor or disconnected mock List stands in for them. */
static ContainerList *list(MCObjectHeap *heap,ItemStack *one,ItemStack *two) {
    FixturePlayer *player=(FixturePlayer *)MCObjectHeap_alloc(heap,sizeof(*player),&player_class);CHECK(player);
    player->inventory=InventoryPlayer_new(heap,(MCObject *)player,creative);CHECK(player->inventory);
    CHECK(InventoryPlayer_setInventorySlotContents(player->inventory,1,one));CHECK(InventoryPlayer_setInventorySlotContents(player->inventory,2,one));CHECK(InventoryPlayer_setInventorySlotContents(player->inventory,3,two));
    Container *container=(Container *)MCObjectHeap_alloc(heap,sizeof(*container),&container_class);CHECK(container);CHECK(Container_construct(container,NULL,NULL));
    for(int32_t i=0;i<4;i++){Slot *slot=Slot_new(heap,mc_IInventory_player(player->inventory),i,0,0);CHECK(slot&&Container_addSlotToContainer(container,slot));}
    ContainerList *result=Container_getInventory(container);CHECK(result&&ContainerList_size(result)==4);return result;
}
static int32_t signed_byte(int32_t value) {uint8_t bits=(uint8_t)value;int8_t out;memcpy(&out,&bits,sizeof(out));return out;}
static int32_t signed_short(int32_t value) {uint16_t bits=(uint16_t)value;int16_t out;memcpy(&out,&bits,sizeof(out));return out;}
static void defaults_and_literal_vectors(void) {
    MCObjectHeap *heap=MCObjectHeap_new(4*1024*1024);CHECK(heap);mc_buf buffer={0};PacketBuffer io;view(&io,heap,&buffer);
    S2EPacketCloseWindow *close=S2EPacketCloseWindow_new_empty(heap);CHECK(close&&close->windowId==0&&S2EPacketCloseWindow_writePacketData(close,&io));bytes(&buffer,"00");mc_buf_clear(&buffer);
    S2FPacketSetSlot *set=S2FPacketSetSlot_new_empty(heap);CHECK(set&&set->windowId==0&&set->slot==0&&!set->item&&S2FPacketSetSlot_writePacketData(set,&io));bytes(&buffer,"000000ffff");mc_buf_clear(&buffer);
    S32PacketConfirmTransaction *confirm=S32PacketConfirmTransaction_new_empty(heap);CHECK(confirm&&confirm->windowId==0&&confirm->actionNumber==0&&!confirm->field_148893_c&&S32PacketConfirmTransaction_writePacketData(confirm,&io));bytes(&buffer,"00000000");mc_buf_clear(&buffer);
    ItemStack *zero=ItemStack_new(heap,ItemStack_registryItem(1),0,7),*negative=ItemStack_new(heap,ItemStack_registryItem(1),-128,7);CHECK(zero&&negative);
    set=S2FPacketSetSlot_new(heap,129,70000,zero);CHECK(set&&set->item!=zero&&set->item->stackSize==0&&set->windowId==129&&set->slot==70000);CHECK(S2FPacketSetSlot_writePacketData(set,&io));bytes(&buffer,"811170000100000700");
    S2FPacketSetSlot *decoded=S2FPacketSetSlot_new_empty(heap);CHECK(decoded&&S2FPacketSetSlot_readPacketData(decoded,&io));CHECK(decoded->windowId==-127&&decoded->slot==4464&&decoded->item&&decoded->item->stackSize==0&&decoded->item->itemDamage==7&&buffer.pos==buffer.len);mc_buf_clear(&buffer);
    S30PacketWindowItems *items=S30PacketWindowItems_new(heap,129,list(heap,zero,negative));CHECK(items);CHECK(S30PacketWindowItems_writePacketData(items,&io));bytes(&buffer,"810004ffff000100000700000100000700000180000700");
    S30PacketWindowItems *received=S30PacketWindowItems_new_empty(heap);CHECK(received&&S30PacketWindowItems_readPacketData(received,&io));CHECK(received->windowId==129&&received->itemStacks->length==4&&!received->itemStacks->items[0]);CHECK(received->itemStacks->items[1]&&received->itemStacks->items[1]->stackSize==0&&received->itemStacks->items[2]->stackSize==0&&received->itemStacks->items[3]->stackSize==-128);CHECK(received->itemStacks->items[1]!=received->itemStacks->items[2]&&buffer.pos==buffer.len);mc_buf_clear(&buffer);
    confirm=S32PacketConfirmTransaction_new(heap,255,INT16_MIN,true);CHECK(confirm&&S32PacketConfirmTransaction_writePacketData(confirm,&io));bytes(&buffer,"ff800001");buffer.data[3]=255;
    confirm=S32PacketConfirmTransaction_new_empty(heap);CHECK(confirm&&S32PacketConfirmTransaction_readPacketData(confirm,&io));CHECK(confirm->windowId==255&&confirm->actionNumber==INT16_MIN&&confirm->field_148893_c);mc_buf_clear(&buffer);
    close=S2EPacketCloseWindow_new(heap,-1);CHECK(close&&S2EPacketCloseWindow_writePacketData(close,&io));bytes(&buffer,"ff");close=S2EPacketCloseWindow_new_empty(heap);CHECK(close&&S2EPacketCloseWindow_readPacketData(close,&io)&&close->windowId==255);
    mc_buf_free(&buffer);CHECK(!MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(1024*1024);CHECK(heap);view(&io,heap,&buffer);items=S30PacketWindowItems_new_empty(heap);CHECK(items&&items->windowId==0&&!S30PacketWindowItems_getItemStacks(items));CHECK(!S30PacketWindowItems_writePacketData(items,&io));CHECK(buffer.failed&&buffer.len==1&&buffer.data[0]==0&&MCObjectHeap_failed(heap)&&!MCObjectHeap_hasBorrowers(heap));mc_buf_free(&buffer);MCObjectHeap_free(heap);
}
static void signed_roundtrips(void) {
    const int32_t windows[]={-1,0,127,128,255,256,70000,INT32_MIN,INT32_MAX};
    const int32_t counts[]={-128,-1,0,127,128,INT32_MIN,INT32_MAX};
    MCObjectHeap *heap=MCObjectHeap_new(8*1024*1024);CHECK(heap);mc_buf buffer={0};PacketBuffer io;view(&io,heap,&buffer);
    for(unsigned w=0;w<sizeof(windows)/sizeof(windows[0]);w++) {
        S2EPacketCloseWindow *close=S2EPacketCloseWindow_new(heap,windows[w]);CHECK(close&&close->windowId==windows[w]&&S2EPacketCloseWindow_writePacketData(close,&io));CHECK(buffer.len==1&&buffer.data[0]==(uint8_t)windows[w]);S2EPacketCloseWindow *readClose=S2EPacketCloseWindow_new_empty(heap);CHECK(readClose&&S2EPacketCloseWindow_readPacketData(readClose,&io)&&readClose->windowId==(uint8_t)windows[w]);mc_buf_clear(&buffer);
        S32PacketConfirmTransaction *confirm=S32PacketConfirmTransaction_new(heap,windows[w],INT16_MAX,false);CHECK(confirm&&S32PacketConfirmTransaction_getWindowId(confirm)==windows[w]&&S32PacketConfirmTransaction_getActionNumber(confirm)==INT16_MAX&&!S32PacketConfirmTransaction_func_148888_e(confirm)&&S32PacketConfirmTransaction_writePacketData(confirm,&io));S32PacketConfirmTransaction *readConfirm=S32PacketConfirmTransaction_new_empty(heap);CHECK(readConfirm&&S32PacketConfirmTransaction_readPacketData(readConfirm,&io)&&readConfirm->windowId==(uint8_t)windows[w]&&readConfirm->actionNumber==INT16_MAX&&!readConfirm->field_148893_c);mc_buf_clear(&buffer);
        for(unsigned c=0;c<sizeof(counts)/sizeof(counts[0]);c++) {
            ItemStack *source=ItemStack_new(heap,ItemStack_registryItem(1),counts[c],70000);CHECK(source);S2FPacketSetSlot *set=S2FPacketSetSlot_new(heap,windows[w],INT32_MIN,source);CHECK(set&&S2FPacketSetSlot_func_149175_c(set)==windows[w]&&S2FPacketSetSlot_func_149173_d(set)==INT32_MIN&&S2FPacketSetSlot_func_149174_e(set)!=source&&set->item->stackSize==counts[c]);CHECK(S2FPacketSetSlot_writePacketData(set,&io));S2FPacketSetSlot *received=S2FPacketSetSlot_new_empty(heap);CHECK(received&&S2FPacketSetSlot_readPacketData(received,&io));CHECK(received->windowId==signed_byte(windows[w])&&received->slot==0&&received->item&&received->item->stackSize==signed_byte(counts[c])&&received->item->itemDamage==signed_short(70000));mc_buf_clear(&buffer);
        }
    }
    mc_buf_free(&buffer);CHECK(!MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
}
static void copies_getters_and_graph_lifetime(void) {
    MCObjectHeap *heap=MCObjectHeap_new(8*1024*1024);CHECK(heap);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,heap));
    ItemStack *source=ItemStack_new(heap,ItemStack_registryItem(387),-1,7);CHECK(source);source->animationsToGo=19;
    NBTTagCompound *tag=NBTTagCompound_new(heap),*child=NBTTagCompound_new(heap);CHECK(tag&&child);CHECK(NBTTagCompound_setInteger_ascii(child,"v",1));CHECK(NBTTagCompound_setTag_ascii(tag,"left",(NBTBase *)child));CHECK(NBTTagCompound_setTag_ascii(tag,"right",(NBTBase *)child));CHECK(NBTTagCompound_setString_ascii(tag,"title",NBTString_fromUTF8(heap,"長い本の題名")));CHECK(ItemStack_setTagCompound(source,tag));
    S30PacketWindowItems *items=S30PacketWindowItems_new(heap,19,list(heap,source,NULL));CHECK(items&&S30PacketWindowItems_func_148911_c(items)==19);ItemStackArray *array=S30PacketWindowItems_getItemStacks(items);CHECK(array==items->itemStacks&&array->length==4&&!array->items[0]&&!array->items[3]);CHECK(array->items[1]!=source&&array->items[2]!=source&&array->items[1]!=array->items[2]);
    CHECK(array->items[1]->stackSize==-1&&array->items[1]->itemDamage==7&&array->items[1]->animationsToGo==0&&array->items[1]->stackTagCompound!=tag&&array->items[1]->stackTagCompound!=array->items[2]->stackTagCompound);
    for(int i=1;i<=2;i++) {NBTBase *left=NBTTagCompound_getTag_ascii(array->items[i]->stackTagCompound,"left"),*right=NBTTagCompound_getTag_ascii(array->items[i]->stackTagCompound,"right");CHECK(left&&right&&left!=right&&left!=(NBTBase *)child&&NBTBase_equals(left,right));CHECK(NBTString_equals(NBTTagCompound_getString_ascii(array->items[i]->stackTagCompound,"title"),NBTTagCompound_getString_ascii(tag,"title")));}
    S2FPacketSetSlot *set=S2FPacketSetSlot_new(heap,-1,-1,source);CHECK(set&&set->item!=source&&set->item->stackTagCompound!=tag&&set->item->animationsToGo==0);
    /* The source array getter is mutable, and source stack getters are direct. */
    array->items[0]=S2FPacketSetSlot_func_149174_e(set);array->items[3]=array->items[0];array->items[0]->stackSize=0;CHECK(set->item->stackSize==0&&source->stackSize==-1);
    MCObjectRoot a={0},b={0};CHECK(MCObjectRoot_init(&a,heap,(MCObject *)items));CHECK(MCObjectRoot_init(&b,heap,(MCObject *)set));MCObjectRootScope_end(&scope);CHECK(MCObjectHeap_collect(heap));MCObjectHeap *working=MCObjectHeap_clone(heap);CHECK(working);MCObjectRoot wa={0},wb={0};CHECK(MCObjectRoot_rebind(&wa,working,&a));CHECK(MCObjectRoot_rebind(&wb,working,&b));CHECK(MCObjectRootScope_begin(&scope,working));
    S30PacketWindowItems *copy=(S30PacketWindowItems *)MCObjectRoot_get(&wa);S2FPacketSetSlot *copySet=(S2FPacketSetSlot *)MCObjectRoot_get(&wb);CHECK(copy->itemStacks!=array&&copy->itemStacks->items[0]==copySet->item&&copy->itemStacks->items[3]==copySet->item&&copySet->item!=set->item);CHECK(copy->itemStacks->items[1]!=copy->itemStacks->items[2]);
    CHECK(NBTTagCompound_setInteger_ascii((NBTTagCompound *)NBTTagCompound_getTag_ascii(copySet->item->stackTagCompound,"left"),"v",99));CHECK(NBTTagCompound_getInteger_ascii((NBTTagCompound *)NBTTagCompound_getTag_ascii(set->item->stackTagCompound,"left"),"v")==1);
    MCObjectRootScope_end(&scope);CHECK(MCObjectHeap_adopt(heap,working));MCObjectHeap_free(working);items=(S30PacketWindowItems *)MCObjectRoot_get(&a);set=(S2FPacketSetSlot *)MCObjectRoot_get(&b);CHECK(items->itemStacks->items[0]==set->item&&items->itemStacks->items[3]==set->item);CHECK(MCObjectHeap_collect(heap));MCObjectHeap_free(heap);
}
static void partial_reads(void) {
    const uint8_t setBytes[]={0x81,0x11,0x70,0x00,0x01,0x00,0x00,0x07,0x00};
    for(size_t length=0;length<sizeof(setBytes);length++) {
        MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);S2FPacketSetSlot *p=S2FPacketSetSlot_new_empty(heap);CHECK(p);ItemStack *old=ItemStack_new_item(heap,ItemStack_registryItem(7));CHECK(old);p->windowId=123;p->slot=124;p->item=old;mc_buf input={(uint8_t *)setBytes,length,length,0,false};PacketBuffer io;view(&io,heap,&input);CHECK(!S2FPacketSetSlot_readPacketData(p,&io)&&input.failed);CHECK(p->windowId==(length>=1?-127:123)&&p->slot==(length>=3?4464:124)&&p->item==old);CHECK(!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
    }
    const uint8_t listBytes[]={0x81,0x00,0x04,0xff,0xff,0x00,0x01,0x00,0x00,0x07,0x00,0x00,0x01,0x00,0x00,0x07,0x00,0x00,0x01,0x80,0x00,0x07,0x00};
    for(size_t length=0;length<sizeof(listBytes);length++) {
        MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);S30PacketWindowItems *p=S30PacketWindowItems_new_empty(heap);CHECK(p);ItemStackArray *old=ItemStackArray_new(heap,1);CHECK(old);p->windowId=123;p->itemStacks=old;mc_buf input={(uint8_t *)listBytes,length,length,0,false};PacketBuffer io;view(&io,heap,&input);CHECK(!S30PacketWindowItems_readPacketData(p,&io)&&input.failed);CHECK(p->windowId==(length>=1?129:123));
        if(length<3)CHECK(p->itemStacks==old);else {CHECK(p->itemStacks!=old&&p->itemStacks->length==4&&!p->itemStacks->items[0]&&!p->itemStacks->items[3]);CHECK((p->itemStacks->items[1]!=NULL)==(length>=11));CHECK((p->itemStacks->items[2]!=NULL)==(length>=17));if(length>=11)CHECK(p->itemStacks->items[1]->stackSize==0&&p->itemStacks->items[1]->itemDamage==7);if(length>=17)CHECK(p->itemStacks->items[1]!=p->itemStacks->items[2]);}
        CHECK(!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
    }
    const uint8_t confirmBytes[]={0x81,0x80,0x00,0x00};
    for(size_t length=0;length<=sizeof(confirmBytes);length++) {
        MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);S32PacketConfirmTransaction *p=S32PacketConfirmTransaction_new(heap,123,INT16_MAX,true);CHECK(p);mc_buf input={(uint8_t *)confirmBytes,length,length,0,false};PacketBuffer io;view(&io,heap,&input);CHECK(S32PacketConfirmTransaction_readPacketData(p,&io)==(length==4));CHECK(p->windowId==(length>=1?129:123)&&p->actionNumber==(length>=3?INT16_MIN:INT16_MAX)&&p->field_148893_c==(length<4));CHECK(!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
    }
    for(unsigned n=0;n<3;n++) {
        const uint8_t headers[][3]={{0x81,0xff,0xff},{0x81,0x80,0x00},{0x81,0x7f,0xff}};MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);S30PacketWindowItems *p=S30PacketWindowItems_new_empty(heap);CHECK(p);ItemStackArray *old=ItemStackArray_new(heap,1);CHECK(old);p->itemStacks=old;mc_buf input={(uint8_t *)headers[n],3,3,0,false};PacketBuffer io;view(&io,heap,&input);CHECK(!S30PacketWindowItems_readPacketData(p,&io)&&input.failed&&p->windowId==129&&input.pos==3);CHECK(n<2?(p->itemStacks==old&&MCObjectHeap_failed(heap)):(p->itemStacks!=old&&p->itemStacks->length==INT16_MAX&&!MCObjectHeap_failed(heap)));CHECK(!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
    }
    for(unsigned n=0;n<2;n++) {
        uint8_t inputBytes[]={0xff,0xff,0xff,0x03,0xe7,0xff,0x00,0x07,0x00};if(n==0){inputBytes[3]=0;inputBytes[4]=0;}MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);S2FPacketSetSlot *p=S2FPacketSetSlot_new_empty(heap);CHECK(p);mc_buf input={inputBytes,sizeof(inputBytes),sizeof(inputBytes),0,false};PacketBuffer io;view(&io,heap,&input);CHECK(S2FPacketSetSlot_readPacketData(p,&io));CHECK(p->windowId==-1&&p->slot==-1&&p->item&&!p->item->item&&p->item->stackSize==-1&&p->item->itemDamage==7);MCObjectHeap_free(heap);
    }
}
typedef struct {MCObject object;MCObject *last;unsigned calls[4];bool refuse,poison;} Handler;
static void handler_trace(MCObject *o,MCObjectVisitor visitor,void *context) {Handler *h=(Handler *)o;h->last=visitor(h->last,context);}
static const MCObjectClass handler_class={"fixture.server-packet.handler",MCObjectHeap_plainClone,handler_trace,NULL};
static bool record(MCObject *o,MCObject *packet,unsigned index) {Handler *h=(Handler *)o;CHECK(MCObjectHeap_hasBorrowers(o->heap)&&!MCObjectHeap_collect(o->heap));h->last=packet;++h->calls[index];if(h->poison)MCObjectHeap_fail(o->heap);return !h->refuse;}
static bool closed(MCObject *o,S2EPacketCloseWindow *p) {return record(o,(MCObject *)p,0);}
static bool set(MCObject *o,S2FPacketSetSlot *p) {return record(o,(MCObject *)p,1);}
static bool updated(MCObject *o,S30PacketWindowItems *p) {return record(o,(MCObject *)p,2);}
static bool confirmed(MCObject *o,S32PacketConfirmTransaction *p) {return record(o,(MCObject *)p,3);}
static const INetHandlerPlayClientMethods handler_methods={
    .handleCloseWindow=closed,.handleSetSlot=set,.handleWindowItems=updated,.handleConfirmTransaction=confirmed
};
static bool process(unsigned kind,MCObject *packet,INetHandlerPlayClient h) {
    switch(kind){case 0:return S2EPacketCloseWindow_processPacket((S2EPacketCloseWindow *)packet,h);case 1:return S2FPacketSetSlot_processPacket((S2FPacketSetSlot *)packet,h);case 2:return S30PacketWindowItems_processPacket((S30PacketWindowItems *)packet,h);default:return S32PacketConfirmTransaction_processPacket((S32PacketConfirmTransaction *)packet,h);}
}
static MCObject *new_packet(MCObjectHeap *heap,unsigned kind) {
    switch(kind){case 0:return (MCObject *)S2EPacketCloseWindow_new_empty(heap);case 1:return (MCObject *)S2FPacketSetSlot_new_empty(heap);case 2:return (MCObject *)S30PacketWindowItems_new_empty(heap);default:return (MCObject *)S32PacketConfirmTransaction_new_empty(heap);}
}
static void handlers_and_boundaries(void) {
    for(unsigned kind=0;kind<4;kind++)for(unsigned scenario=0;scenario<6;scenario++) {
        MCObjectHeap *heap=MCObjectHeap_new(1024*1024),*foreign=MCObjectHeap_new(1024*1024);CHECK(heap&&foreign);Handler *h=(Handler *)MCObjectHeap_alloc(scenario==3?foreign:heap,sizeof(*h),&handler_class);CHECK(h);MCObject *packet=new_packet(heap,kind);CHECK(packet);INetHandlerPlayClientMethods methods=handler_methods;
        if(scenario==4){switch(kind){case 0:methods.handleCloseWindow=NULL;break;case 1:methods.handleSetSlot=NULL;break;case 2:methods.handleWindowItems=NULL;break;default:methods.handleConfirmTransaction=NULL;break;}}
        h->refuse=scenario==1;h->poison=scenario==5;INetHandlerPlayClient target={(MCObject *)h,&methods};if(scenario==2)target=(INetHandlerPlayClient){0};CHECK(process(kind,packet,target)==(scenario==0));CHECK(MCObjectHeap_failed(heap)==(scenario!=0)&&!MCObjectHeap_failed(foreign));CHECK(h->calls[kind]==((scenario==0||scenario==1||scenario==5)?1u:0u));if(h->calls[kind])CHECK(h->last==packet);CHECK(!MCObjectHeap_hasBorrowers(heap)&&!MCObjectHeap_hasBorrowers(foreign));MCObjectHeap_free(heap);MCObjectHeap_free(foreign);
    }
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024),*foreign=MCObjectHeap_new(1024*1024);CHECK(heap&&foreign);ItemStack *source=ItemStack_new_item(foreign,ItemStack_registryItem(1));CHECK(source);CHECK(!S2FPacketSetSlot_new(heap,1,1,source));CHECK(MCObjectHeap_failed(heap)&&!MCObjectHeap_failed(foreign)&&source->stackSize==1&&!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);MCObjectHeap_free(foreign);
    heap=MCObjectHeap_new(1024*1024);foreign=MCObjectHeap_new(1024*1024);CHECK(heap&&foreign);S30PacketWindowItems *packet=S30PacketWindowItems_new_empty(heap);CHECK(packet);mc_buf b={0};PacketBuffer io;view(&io,foreign,&b);CHECK(!S30PacketWindowItems_writePacketData(packet,&io)&&b.failed&&b.len==0&&MCObjectHeap_failed(heap)&&!MCObjectHeap_failed(foreign));CHECK(!MCObjectHeap_hasBorrowers(heap)&&!MCObjectHeap_hasBorrowers(foreign));mc_buf_free(&b);MCObjectHeap_free(heap);MCObjectHeap_free(foreign);
}
static void allocation_failures(void) {
    MCObjectHeap *heap=MCObjectHeap_new(8192);CHECK(heap);S30PacketWindowItems *packet=S30PacketWindowItems_new_empty(heap);CHECK(packet);ItemStackArray *old=ItemStackArray_new(heap,1);CHECK(old);packet->itemStacks=old;uint8_t header[]={129,0x7f,0xff};mc_buf input={header,sizeof(header),sizeof(header),0,false};PacketBuffer io;view(&io,heap,&input);
    CHECK(!S30PacketWindowItems_readPacketData(packet,&io)&&input.failed&&MCObjectHeap_failed(heap));CHECK(packet->windowId==129&&packet->itemStacks==old&&input.pos==3&&!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(16384);CHECK(heap);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,heap));ItemStack *source=ItemStack_new_item(heap,ItemStack_registryItem(387));CHECK(source);NBTTagCompound *tag=NBTTagCompound_new(heap);CHECK(tag);int8_t payload[8192];memset(payload,7,sizeof(payload));NBTByteArrayStorage *storage=NBTByteArrayStorage_new(heap,payload,(int32_t)sizeof(payload));CHECK(storage);NBTTagByteArray *array=NBTTagByteArray_new(heap,storage);CHECK(array);CHECK(NBTTagCompound_setTag_ascii(tag,"payload",(NBTBase *)array)&&ItemStack_setTagCompound(source,tag));ContainerList *stacks=list(heap,source,NULL);CHECK(!MCObjectHeap_failed(heap));
    CHECK(!S30PacketWindowItems_new(heap,0,stacks)&&MCObjectHeap_failed(heap));CHECK(source->stackTagCompound==tag&&source->stackSize==1&&NBTTagCompound_getTag_ascii(tag,"payload")== (NBTBase *)array&&NBTTagByteArray_getByteArray(array)==storage&&NBTByteArrayStorage_data(storage)[8191]==7);MCObjectRootScope_end(&scope);CHECK(!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
}
int main(void) {defaults_and_literal_vectors();signed_roundtrips();copies_getters_and_graph_lifetime();partial_reads();handlers_and_boundaries();allocation_failures();printf("server inventory packet source ports: %u checks passed\n",checks);return 0;}
