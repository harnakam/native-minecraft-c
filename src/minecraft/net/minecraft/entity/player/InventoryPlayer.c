#include "entity/player/InventoryPlayer.h"
#include "nbt/NBTTagCompound.h"
#include <string.h>
static int32_t add(int32_t a,int32_t b) { uint32_t bits=(uint32_t)a+(uint32_t)b; int32_t out; memcpy(&out,&bits,sizeof(out)); return out; }
static int32_t sub(int32_t a,int32_t b) { uint32_t bits=(uint32_t)a-(uint32_t)b; int32_t out; memcpy(&out,&bits,sizeof(out)); return out; }
static void trace(MCObject *o,MCObjectVisitor visit,void *ctx) {
    InventoryPlayer *p=(InventoryPlayer *)o;
    p->mainInventory=(ItemStackArray *)visit((MCObject *)p->mainInventory,ctx);
    p->armorInventory=(ItemStackArray *)visit((MCObject *)p->armorInventory,ctx);
    p->player=visit(p->player,ctx); p->itemStack=(ItemStack *)visit((MCObject *)p->itemStack,ctx);
}
static const MCObjectClass klass={"InventoryPlayer",MCObjectHeap_plainClone,trace,NULL};
InventoryPlayer *InventoryPlayer_new(MCObjectHeap *heap,MCObject *actor,InventoryPlayerCreative callback) {
    if (actor && actor->heap!=heap) { MCObjectHeap_fail(heap); return NULL; }
    InventoryPlayer *p=(InventoryPlayer *)MCObjectHeap_alloc(heap,sizeof(*p),&klass);
    if (!p) return NULL;
    p->mainInventory=ItemStackArray_new(heap,36); p->armorInventory=ItemStackArray_new(heap,4);
    if (!p->mainInventory || !p->armorInventory) return NULL;
    p->player=actor; p->isCreativeMode=callback; return p;
}
ItemStack *InventoryPlayer_getCurrentItem(const InventoryPlayer *p) { return p->currentItem<9 && p->currentItem>=0?p->mainInventory->items[p->currentItem]:NULL; }
int32_t InventoryPlayer_getHotbarSize(void) { return 9; }
int32_t InventoryPlayer_getFirstEmptyStack(const InventoryPlayer *p) { for (int32_t i=0;i<p->mainInventory->length;i++) if (!p->mainInventory->items[i]) return i; return -1; }
void InventoryPlayer_changeCurrentItem(InventoryPlayer *p,int32_t direction) {
    if (direction>0) direction=1;
    if (direction<0) direction=-1;
    p->currentItem=sub(p->currentItem,direction);
    while (p->currentItem<0) p->currentItem=add(p->currentItem,9);
    while (p->currentItem>=9) p->currentItem=sub(p->currentItem,9);
    MCObjectHeap_touch(p->object.heap);
}
static int32_t contain_item(const InventoryPlayer *p,const Item *item) { for (int32_t i=0;i<p->mainInventory->length;i++) if (p->mainInventory->items[i] && p->mainInventory->items[i]->item==item) return i; return -1; }
bool InventoryPlayer_hasItem(const InventoryPlayer *p,const Item *item) { return contain_item(p,item)>=0; }
bool InventoryPlayer_consumeInventoryItem(InventoryPlayer *p,const Item *item) {
    int32_t i=contain_item(p,item); if (i<0) return false;
    ItemStack *s=p->mainInventory->items[i]; s->stackSize=sub(s->stackSize,1);
    if (s->stackSize<=0) p->mainInventory->items[i]=NULL;
    MCObjectHeap_touch(p->object.heap); return true;
}
static int32_t store_item(const InventoryPlayer *p,const ItemStack *input) {
    for (int32_t i=0;i<p->mainInventory->length;i++) {
        ItemStack *s=p->mainInventory->items[i];
        if (s && s->item==input->item && ItemStack_isStackable(s) && s->stackSize<ItemStack_getMaxStackSize(s) && s->stackSize<InventoryPlayer_getInventoryStackLimit(p) && (!ItemStack_getHasSubtypes(s)||s->itemDamage==input->itemDamage) && ItemStack_areItemStackTagsEqual(s,input)) return i;
    }
    return -1;
}
static int32_t store_partial(InventoryPlayer *p,ItemStack *input) {
    const Item *item=input->item; int32_t count=input->stackSize,slot=store_item(p,input);
    if (slot<0) slot=InventoryPlayer_getFirstEmptyStack(p);
    if (slot<0) return count;
    if (!p->mainInventory->items[slot]) {
        ItemStack *s=ItemStack_new(p->object.heap,item,0,input->itemDamage);
        if (!s) return count;
        p->mainInventory->items[slot]=s; MCObjectHeap_touch(p->object.heap);
        if (ItemStack_hasTagCompound(input)) {
            NBTTagCompound *tag=(NBTTagCompound *)NBTBase_copy(p->object.heap,(NBTBase *)input->stackTagCompound);
            if (!tag || !ItemStack_setTagCompound(s,tag)) return count;
        }
    }
    ItemStack *s=p->mainInventory->items[slot]; int32_t moved=count;
    int32_t capacity=sub(ItemStack_getMaxStackSize(s),s->stackSize);
    if (count>capacity) moved=capacity;
    capacity=sub(InventoryPlayer_getInventoryStackLimit(p),s->stackSize);
    if (moved>capacity) moved=capacity;
    if (moved==0) return count;
    count=sub(count,moved); s->stackSize=add(s->stackSize,moved); s->animationsToGo=5; MCObjectHeap_touch(p->object.heap); return count;
}
static bool creative(InventoryPlayer *p) {
    if (!p->player || !p->isCreativeMode) { MCObjectHeap_fail(p->object.heap); return false; }
    return p->isCreativeMode(p->player);
}
bool InventoryPlayer_addItemStackToInventory(InventoryPlayer *p,ItemStack *input) {
    if (!input || input->stackSize==0 || !input->item) return false;
    if (input->object.heap!=p->object.heap) { MCObjectHeap_fail(p->object.heap); return false; }
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,p->object.heap)) return false;
    if (!MCObjectRootScope_pin(&scope,(MCObject *)p)||!MCObjectRootScope_pin(&scope,(MCObject *)input)) { MCObjectRootScope_end(&scope); return false; }
    bool result=false;
    if (ItemStack_isItemDamaged(input)) {
        int32_t empty=InventoryPlayer_getFirstEmptyStack(p);
        if (empty>=0) {
            ItemStack *copied=ItemStack_copyItemStack(p->object.heap,input);
            if (copied) { p->mainInventory->items[empty]=copied; copied->animationsToGo=5; input->stackSize=0; result=true; MCObjectHeap_touch(p->object.heap); }
        } else if (creative(p)) { input->stackSize=0; result=true; MCObjectHeap_touch(p->object.heap); }
    } else {
        int32_t before;
        for (;;) {
            before=input->stackSize; input->stackSize=store_partial(p,input); MCObjectHeap_touch(p->object.heap);
            if (MCObjectHeap_failed(p->object.heap) || input->stackSize<=0 || input->stackSize>=before) break;
        }
        if (!MCObjectHeap_failed(p->object.heap)) {
            if (input->stackSize==before && creative(p)) { input->stackSize=0; result=true; MCObjectHeap_touch(p->object.heap); }
            else result=input->stackSize<before;
        }
    }
    MCObjectRootScope_end(&scope); return result && !MCObjectHeap_failed(p->object.heap);
}
static ItemStack **slot(InventoryPlayer *p,int32_t index) {
    ItemStackArray *array=p->mainInventory;
    if (index>=array->length) { index=sub(index,array->length); array=p->armorInventory; }
    if (index<0 || index>=array->length) { MCObjectHeap_fail(p->object.heap); return NULL; }
    return &array->items[index];
}
ItemStack *InventoryPlayer_getStackInSlot(InventoryPlayer *p,int32_t index) { ItemStack **s=slot(p,index); return s?*s:NULL; }
ItemStack *InventoryPlayer_armorItemInSlot(InventoryPlayer *p,int32_t index) { if (index<0 || index>=p->armorInventory->length) { MCObjectHeap_fail(p->object.heap); return NULL; } return p->armorInventory->items[index]; }
ItemStack *InventoryPlayer_removeStackFromSlot(InventoryPlayer *p,int32_t index) { ItemStack **s=slot(p,index); if (!s || !*s) return NULL; ItemStack *out=*s; *s=NULL; MCObjectHeap_touch(p->object.heap); return out; }
ItemStack *InventoryPlayer_decrStackSize(InventoryPlayer *p,int32_t index,int32_t count) {
    ItemStack **s=slot(p,index); if (!s || !*s) return NULL;
    ItemStack *out;
    if ((*s)->stackSize<=count) { out=*s; *s=NULL; }
    else { out=ItemStack_splitStack(p->object.heap,*s,count); if (!out) return NULL; if ((*s)->stackSize==0) *s=NULL; }
    MCObjectHeap_touch(p->object.heap); return out;
}
bool InventoryPlayer_setInventorySlotContents(InventoryPlayer *p,int32_t index,ItemStack *input) {
    ItemStack **s=slot(p,index); if (!s) return false;
    if (input && input->object.heap!=p->object.heap) { MCObjectHeap_fail(p->object.heap); return false; }
    *s=input; MCObjectHeap_touch(p->object.heap); return true;
}
int32_t InventoryPlayer_getSizeInventory(const InventoryPlayer *p) { return p->mainInventory->length+4; }
int32_t InventoryPlayer_getInventoryStackLimit(const InventoryPlayer *p) { (void)p; return 64; }
const char *InventoryPlayer_getName(const InventoryPlayer *p) { (void)p; return "container.inventory"; }
bool InventoryPlayer_hasCustomName(const InventoryPlayer *p) { (void)p; return false; }
InventoryDisplayName InventoryPlayer_getDisplayName(const InventoryPlayer *p) { InventoryDisplayName d={InventoryPlayer_getName(p),!InventoryPlayer_hasCustomName(p)}; return d; }
void InventoryPlayer_markDirty(InventoryPlayer *p) { p->inventoryChanged=true; MCObjectHeap_touch(p->object.heap); }
bool InventoryPlayer_setItemStack(InventoryPlayer *p,ItemStack *input) { if (input && input->object.heap!=p->object.heap) { MCObjectHeap_fail(p->object.heap); return false; } p->itemStack=input; MCObjectHeap_touch(p->object.heap); return true; }
ItemStack *InventoryPlayer_getItemStack(const InventoryPlayer *p) { return p->itemStack; }
bool InventoryPlayer_hasItemStack(const InventoryPlayer *p,const ItemStack *s) {
    for (int32_t i=0;i<p->armorInventory->length;i++) if (p->armorInventory->items[i] && ItemStack_isItemEqual(p->armorInventory->items[i],s)) return true;
    for (int32_t i=0;i<p->mainInventory->length;i++) if (p->mainInventory->items[i] && ItemStack_isItemEqual(p->mainInventory->items[i],s)) return true;
    return false;
}
NBTTagList *InventoryPlayer_writeToNBT(InventoryPlayer *p,NBTTagList *out) {
    if (!out || ((MCObject *)out)->heap!=p->object.heap) { MCObjectHeap_fail(p->object.heap); return NULL; }
    for (int32_t group=0;group<2;group++) {
        ItemStackArray *a=group?p->armorInventory:p->mainInventory;
        for (int32_t i=0;i<a->length;i++) if (a->items[i]) {
            NBTTagCompound *tag=NBTTagCompound_new(p->object.heap);
            if (!tag || !NBTTagCompound_setByte_ascii(tag,"Slot",(int8_t)(i+(group?100:0))) || !ItemStack_writeToNBT(a->items[i],tag) || !NBTTagList_appendTag(out,(NBTBase *)tag)) return NULL;
        }
    }
    return out;
}
ItemStackNBTResult InventoryPlayer_readFromNBT(InventoryPlayer *p,NBTTagList *input) {
    if (!input || ((MCObject *)input)->heap!=p->object.heap) { MCObjectHeap_fail(p->object.heap); return ITEMSTACK_NBT_FAILURE; }
    p->mainInventory=ItemStackArray_new(p->object.heap,36); if (!p->mainInventory) return ITEMSTACK_NBT_FAILURE;
    p->armorInventory=ItemStackArray_new(p->object.heap,4); if (!p->armorInventory) return ITEMSTACK_NBT_FAILURE;
    MCObjectHeap_touch(p->object.heap);
    for (int32_t i=0;i<NBTTagList_tagCount(input);i++) {
        NBTTagCompound *tag=NBTTagList_getCompoundTagAt(input,i); if (!tag) return ITEMSTACK_NBT_FAILURE;
        int32_t index=(uint8_t)NBTTagCompound_getByte_ascii(tag,"Slot"); ItemStackNBTResult status;
        ItemStack *s=ItemStack_loadItemStackFromNBT(p->object.heap,tag,&status); if (status!=ITEMSTACK_NBT_OK) return status;
        if (s) {
            if (index<p->mainInventory->length) p->mainInventory->items[index]=s;
            if (index>=100 && index<p->armorInventory->length+100) p->armorInventory->items[index-100]=s;
        }
    }
    MCObjectHeap_touch(p->object.heap); return MCObjectHeap_failed(p->object.heap)?ITEMSTACK_NBT_FAILURE:ITEMSTACK_NBT_OK;
}
bool InventoryPlayer_copyInventory(InventoryPlayer *p,const InventoryPlayer *other) {
    for (int32_t i=0;i<p->mainInventory->length;i++) { p->mainInventory->items[i]=ItemStack_copyItemStack(p->object.heap,other->mainInventory->items[i]); if (MCObjectHeap_failed(p->object.heap)) return false; }
    for (int32_t i=0;i<p->armorInventory->length;i++) { p->armorInventory->items[i]=ItemStack_copyItemStack(p->object.heap,other->armorInventory->items[i]); if (MCObjectHeap_failed(p->object.heap)) return false; }
    p->currentItem=other->currentItem; MCObjectHeap_touch(p->object.heap); return true;
}
void InventoryPlayer_clear(InventoryPlayer *p) { for (int32_t i=0;i<p->mainInventory->length;i++) p->mainInventory->items[i]=NULL; for (int32_t i=0;i<p->armorInventory->length;i++) p->armorInventory->items[i]=NULL; MCObjectHeap_touch(p->object.heap); }
/* Original methods intentionally empty or constant. */
void InventoryPlayer_openInventory(InventoryPlayer *p,MCObject *actor) { (void)p; (void)actor; }
void InventoryPlayer_closeInventory(InventoryPlayer *p,MCObject *actor) { (void)p; (void)actor; }
bool InventoryPlayer_isItemValidForSlot(const InventoryPlayer *p,int32_t index,const ItemStack *s) { (void)p; (void)index; (void)s; return true; }
int32_t InventoryPlayer_getField(const InventoryPlayer *p,int32_t id) { (void)p; (void)id; return 0; }
void InventoryPlayer_setField(InventoryPlayer *p,int32_t id,int32_t value) { (void)p; (void)id; (void)value; }
int32_t InventoryPlayer_getFieldCount(const InventoryPlayer *p) { (void)p; return 0; }
