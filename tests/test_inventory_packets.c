#include "network/play/client/C0DPacketCloseWindow.h"
#include "network/play/client/C0EPacketClickWindow.h"
#include "network/play/client/C0FPacketConfirmTransaction.h"
#include "network/play/client/C10PacketCreativeInventoryAction.h"
#include "nbt/NBTTagCompound.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"inventory packets: %s at %d\n",#x,__LINE__); exit(1); } } while (0)
static void bytes_equal(const mc_buf *buffer,const char *hex) {
    CHECK(!buffer->failed&&buffer->len*2==strlen(hex));
    for (size_t i=0;i<buffer->len;i++) { unsigned byte=0; CHECK(sscanf(hex+i*2,"%2x",&byte)==1); CHECK(buffer->data[i]==byte); }
}
static void view(PacketBuffer *packet,MCObjectHeap *heap,mc_buf *buffer) { CHECK(PacketBuffer_init(packet,heap,buffer)); }
static void actual_game_vectors(void) {
    /* Numerical wire vectors independently executed with actual 1.8.9 packet
       classes. Read Byte is signed even for the window field; constructors
       retain their full int argument before the wire narrows it. */
    MCObjectHeap *heap=MCObjectHeap_new(4*1024*1024); CHECK(heap); mc_buf buffer={0}; PacketBuffer io; view(&io,heap,&buffer);
    ItemStack *source=ItemStack_new(heap,ItemStack_registryItem(1),0,0); CHECK(source);
    C0EPacketClickWindow *click=C0EPacketClickWindow_new(heap,129,70000,254,255,source,4660); CHECK(click);
    CHECK(C0EPacketClickWindow_getClickedItem(click)!=source); CHECK(C0EPacketClickWindow_getWindowId(click)==129);
    CHECK(C0EPacketClickWindow_getSlotId(click)==70000&&C0EPacketClickWindow_getUsedButton(click)==254);
    CHECK(C0EPacketClickWindow_getMode(click)==255&&C0EPacketClickWindow_getActionNumber(click)==4660);
    CHECK(C0EPacketClickWindow_writePacketData(click,&io)); bytes_equal(&buffer,"811170fe1234ff000100000000");
    C0EPacketClickWindow *read=C0EPacketClickWindow_new_empty(heap); CHECK(read); CHECK(C0EPacketClickWindow_readPacketData(read,&io));
    CHECK(read->windowId==-127&&read->slotId==4464&&read->usedButton==-2&&read->actionNumber==4660&&read->mode==-1);
    CHECK(read->clickedItem&&read->clickedItem->stackSize==0&&read->clickedItem->itemDamage==0&&read->clickedItem!=click->clickedItem);
    CHECK(buffer.pos==buffer.len); mc_buf_free(&buffer); view(&io,heap,&buffer);
    click=C0EPacketClickWindow_new(heap,255,-999,128,128,NULL,INT16_MIN); CHECK(click);
    CHECK(C0EPacketClickWindow_writePacketData(click,&io)); bytes_equal(&buffer,"fffc1980800080ffff");
    read=C0EPacketClickWindow_new_empty(heap); CHECK(read&&C0EPacketClickWindow_readPacketData(read,&io));
    CHECK(read->windowId==-1&&read->slotId==-999&&read->usedButton==-128&&read->mode==-128&&read->actionNumber==INT16_MIN&&!read->clickedItem);
    mc_buf_free(&buffer); view(&io,heap,&buffer);
    C0DPacketCloseWindow *close=C0DPacketCloseWindow_new(heap,255); CHECK(close&&C0DPacketCloseWindow_writePacketData(close,&io)); bytes_equal(&buffer,"ff");
    close=C0DPacketCloseWindow_new_empty(heap); CHECK(close&&C0DPacketCloseWindow_readPacketData(close,&io)&&close->windowId==-1);
    mc_buf_free(&buffer); view(&io,heap,&buffer);
    C0FPacketConfirmTransaction *confirm=C0FPacketConfirmTransaction_new(heap,129,INT16_MIN,true); CHECK(confirm&&C0FPacketConfirmTransaction_writePacketData(confirm,&io)); bytes_equal(&buffer,"81800001");
    buffer.data[3]=255; confirm=C0FPacketConfirmTransaction_new_empty(heap); CHECK(confirm&&C0FPacketConfirmTransaction_readPacketData(confirm,&io));
    CHECK(C0FPacketConfirmTransaction_getWindowId(confirm)==-127&&C0FPacketConfirmTransaction_getUid(confirm)==INT16_MIN&&confirm->accepted);
    mc_buf_free(&buffer); view(&io,heap,&buffer);
    source=ItemStack_new(heap,ItemStack_registryItem(62),-128,INT32_MAX); CHECK(source);
    C10PacketCreativeInventoryAction *creative=C10PacketCreativeInventoryAction_new(heap,70000,source); CHECK(creative);
    CHECK(C10PacketCreativeInventoryAction_getSlotId(creative)==70000&&C10PacketCreativeInventoryAction_getStack(creative)!=source);
    CHECK(C10PacketCreativeInventoryAction_writePacketData(creative,&io)); bytes_equal(&buffer,"1170003e80ffff00");
    creative=C10PacketCreativeInventoryAction_new_empty(heap); CHECK(creative&&C10PacketCreativeInventoryAction_readPacketData(creative,&io));
    CHECK(creative->slotId==4464&&ItemStack_registryId(creative->stack->item)==62&&creative->stack->stackSize==-128&&creative->stack->itemDamage==0);
    mc_buf_free(&buffer); view(&io,heap,&buffer);
    creative=C10PacketCreativeInventoryAction_new(heap,-1,NULL); CHECK(creative&&C10PacketCreativeInventoryAction_writePacketData(creative,&io)); bytes_equal(&buffer,"ffffffff");
    creative=C10PacketCreativeInventoryAction_new_empty(heap); CHECK(creative&&C10PacketCreativeInventoryAction_readPacketData(creative,&io)); CHECK(creative->slotId==-1&&!creative->stack);
    mc_buf_free(&buffer); CHECK(!MCObjectHeap_failed(heap)); MCObjectHeap_free(heap);
}
static void copy_and_graph_lifetime(void) {
    MCObjectHeap *heap=MCObjectHeap_new(4*1024*1024); CHECK(heap);
    ItemStack *source=ItemStack_new(heap,ItemStack_registryItem(1),-1,7); CHECK(source);
    NBTTagCompound *tag=NBTTagCompound_new(heap),*child=NBTTagCompound_new(heap); CHECK(tag&&child);
    CHECK(NBTTagCompound_setInteger_ascii(child,"v",1));
    CHECK(NBTTagCompound_setTag_ascii(tag,"left",(NBTBase *)child)); CHECK(NBTTagCompound_setTag_ascii(tag,"right",(NBTBase *)child));
    CHECK(ItemStack_setTagCompound(source,tag)); source->animationsToGo=6;
    C0EPacketClickWindow *click=C0EPacketClickWindow_new(heap,0,1,0,0,source,1); CHECK(click);
    CHECK(click->clickedItem!=source&&click->clickedItem->stackTagCompound!=tag&&click->clickedItem->animationsToGo==0);
    NBTBase *left=NBTTagCompound_getTag_ascii(click->clickedItem->stackTagCompound,"left"),*right=NBTTagCompound_getTag_ascii(click->clickedItem->stackTagCompound,"right");
    CHECK(left&&right&&left!=right&&left!=(NBTBase *)child&&NBTBase_equals(left,right));
    C10PacketCreativeInventoryAction *creative=C10PacketCreativeInventoryAction_new_empty(heap); CHECK(creative);
    /* References may subsequently be shared through original getters. */
    creative->stack=click->clickedItem;
    MCObjectRoot a={0},b={0}; CHECK(MCObjectRoot_init(&a,heap,(MCObject *)click)); CHECK(MCObjectRoot_init(&b,heap,(MCObject *)creative));
    CHECK(MCObjectHeap_collect(heap)); MCObjectHeap *working=MCObjectHeap_clone(heap); CHECK(working);
    MCObjectRoot wa={0},wb={0}; CHECK(MCObjectRoot_rebind(&wa,working,&a)); CHECK(MCObjectRoot_rebind(&wb,working,&b));
    C0EPacketClickWindow *wc=(C0EPacketClickWindow *)MCObjectRoot_get(&wa); C10PacketCreativeInventoryAction *wt=(C10PacketCreativeInventoryAction *)MCObjectRoot_get(&wb);
    CHECK(wc->clickedItem==wt->stack&&wc->clickedItem!=click->clickedItem);
    MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,working)); wc->clickedItem->stackSize=0; MCObjectRootScope_end(&scope);
    CHECK(click->clickedItem->stackSize==-1); CHECK(MCObjectHeap_adopt(heap,working)); MCObjectHeap_free(working);
    click=(C0EPacketClickWindow *)MCObjectRoot_get(&a); creative=(C10PacketCreativeInventoryAction *)MCObjectRoot_get(&b);
    CHECK(click->clickedItem==creative->stack&&creative->stack->stackSize==0); CHECK(MCObjectHeap_collect(heap));
    MCObjectHeap_free(heap);
}
typedef struct { MCObject object; MCObject *last; int calls[4]; bool refuse; } Handler;
static void handler_trace(MCObject *object,MCObjectVisitor visitor,void *context) { Handler *handler=(Handler *)object; handler->last=visitor(handler->last,context); }
static const MCObjectClass handler_class={"test.PacketHandler",MCObjectHeap_plainClone,handler_trace,NULL};
static bool record(MCObject *object,MCObject *packet,size_t index) {
    Handler *handler=(Handler *)object; CHECK(MCObjectHeap_hasBorrowers(object->heap)); CHECK(!MCObjectHeap_collect(object->heap));
    handler->last=packet; ++handler->calls[index]; return !handler->refuse;
}
static bool closed(MCObject *h,C0DPacketCloseWindow *p) { return record(h,(MCObject *)p,0); }
static bool clicked(MCObject *h,C0EPacketClickWindow *p) { return record(h,(MCObject *)p,1); }
static bool confirmed(MCObject *h,C0FPacketConfirmTransaction *p) { return record(h,(MCObject *)p,2); }
static bool created(MCObject *h,C10PacketCreativeInventoryAction *p) { return record(h,(MCObject *)p,3); }
static const INetHandlerPlayServerMethods methods={closed,clicked,confirmed,created};
static void handler_dispatch(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024); CHECK(heap);
    Handler *handler=(Handler *)MCObjectHeap_alloc(heap,sizeof(*handler),&handler_class); CHECK(handler);
    INetHandlerPlayServer dispatch={(MCObject *)handler,&methods};
    C0DPacketCloseWindow *close=C0DPacketCloseWindow_new(heap,12); C0EPacketClickWindow *click=C0EPacketClickWindow_new_empty(heap);
    C0FPacketConfirmTransaction *confirm=C0FPacketConfirmTransaction_new_empty(heap); C10PacketCreativeInventoryAction *creative=C10PacketCreativeInventoryAction_new_empty(heap); CHECK(close&&click&&confirm&&creative);
    CHECK(C0DPacketCloseWindow_processPacket(close,dispatch)&&handler->last==(MCObject *)close);
    CHECK(C0EPacketClickWindow_processPacket(click,dispatch)&&handler->last==(MCObject *)click);
    CHECK(C0FPacketConfirmTransaction_processPacket(confirm,dispatch)&&handler->last==(MCObject *)confirm);
    CHECK(C10PacketCreativeInventoryAction_processPacket(creative,dispatch)&&handler->last==(MCObject *)creative);
    for (size_t i=0;i<4;i++) CHECK(handler->calls[i]==1);
    handler->refuse=true; CHECK(!C0EPacketClickWindow_processPacket(click,dispatch)&&MCObjectHeap_failed(heap)); CHECK(!MCObjectHeap_hasBorrowers(heap)); MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(1024*1024); click=C0EPacketClickWindow_new_empty(heap); CHECK(click);
    CHECK(!C0EPacketClickWindow_processPacket(click,(INetHandlerPlayServer){0})&&MCObjectHeap_failed(heap)); CHECK(!MCObjectHeap_hasBorrowers(heap)); MCObjectHeap_free(heap);
}
static void truncated_read_order(void) {
    const uint8_t bytes[]={0x81,0x11,0x70,0xfe,0x12,0x34,0xff,0x00,0x01,0x00,0x00,0x00,0x00};
    for (size_t length=0;length<sizeof(bytes);length++) {
        MCObjectHeap *heap=MCObjectHeap_new(1024*1024); CHECK(heap);
        C0EPacketClickWindow *packet=C0EPacketClickWindow_new_empty(heap); CHECK(packet);
        ItemStack *sentinel=ItemStack_new_item(heap,ItemStack_registryItem(7)); CHECK(sentinel);
        packet->windowId=123; packet->slotId=124; packet->usedButton=125; packet->actionNumber=126; packet->mode=127; packet->clickedItem=sentinel;
        mc_buf input={(uint8_t *)bytes,length,length,0,false}; PacketBuffer io; view(&io,heap,&input);
        CHECK(!C0EPacketClickWindow_readPacketData(packet,&io)&&input.failed);
        CHECK(packet->windowId==(length>=1?-127:123)); CHECK(packet->slotId==(length>=3?4464:124)); CHECK(packet->usedButton==(length>=4?-2:125));
        CHECK(packet->actionNumber==(length>=6?4660:126)); CHECK(packet->mode==(length>=7?-1:127)); CHECK(packet->clickedItem==sentinel);
        CHECK(!MCObjectHeap_hasBorrowers(heap)); MCObjectHeap_free(heap);
    }
}
int main(void) {
    actual_game_vectors(); copy_and_graph_lifetime(); handler_dispatch(); truncated_read_order();
    printf("inventory packet source ports: %u checks passed\n",checks); return 0;
}
