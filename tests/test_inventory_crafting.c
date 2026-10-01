#include "inventory/InventoryCrafting.h"
#include "inventory/InventoryCraftResult.h"
#include "item/crafting/RecipeBookCloning.h"
#include "nbt/NBTTagCompound.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
typedef struct { MCObject object; unsigned calls; int32_t seen[16]; bool reject; InventoryCraftResult *result; } Observer;
static void trace(MCObject *o,MCObjectVisitor visit,void *ctx) { Observer *s=(Observer *)o; s->result=(InventoryCraftResult *)visit((MCObject *)s->result,ctx); }
static const MCObjectClass klass={"GridTestObserver",MCObjectHeap_plainClone,trace,NULL};
static Observer *observer(MCObjectHeap *h) { Observer *o=(Observer *)MCObjectHeap_alloc(h,sizeof(*o),&klass); CHECK(o); return o; }
static bool notify(MCObject *handler,InventoryCrafting *g) {
    Observer *o=(Observer *)handler; ItemStack *stack=InventoryCrafting_getStackInSlot(g,0);
    CHECK(o->calls<16); o->seen[o->calls++]=stack?stack->stackSize:INT32_MIN;
    CHECK(!MCObjectHeap_collect(g->object.heap)); /* Java-local scope blocks GC. */
    return !o->reject;
}
static ItemStack *stack(MCObjectHeap *h,int id,int32_t count) { ItemStack *s=ItemStack_new(h,ItemStack_registryItem(id),count,0); CHECK(s); return s; }
static void matrix_methods(void) {
    MCObjectHeap *h=MCObjectHeap_new(1024*1024); CHECK(h); Observer *state=observer(h);
    InventoryCrafting *g=InventoryCrafting_new(h,(MCObject *)state,notify,3,3); CHECK(g);
    CHECK(InventoryCrafting_getSizeInventory(g)==9&&InventoryCrafting_getWidth(g)==3&&InventoryCrafting_getHeight(g)==3);
    CHECK(!state->calls&&!InventoryCrafting_getStackInSlot(g,0));
    CHECK(!strcmp(InventoryCrafting_getName(g),"container.crafting")&&!InventoryCrafting_hasCustomName(g));
    InventoryDisplayName d=InventoryCrafting_getDisplayName(g); CHECK(d.translated&&!strcmp(d.key,"container.crafting"));
    CHECK(InventoryCrafting_getInventoryStackLimit(g)==64&&InventoryCrafting_isUseableByPlayer(g,NULL));
    CHECK(InventoryCrafting_isItemValidForSlot(g,-99,NULL));
    InventoryCrafting_markDirty(g); InventoryCrafting_openInventory(g,NULL); InventoryCrafting_closeInventory(g,NULL); InventoryCrafting_setField(g,1,12);
    CHECK(!InventoryCrafting_getField(g,1)&&!InventoryCrafting_getFieldCount(g)&&!state->calls);
    ItemStack *s=stack(h,17,5),*out;
    CHECK(InventoryCrafting_setInventorySlotContents(g,0,s)); CHECK(state->calls==1&&state->seen[0]==5);
    CHECK(InventoryCrafting_getStackInRowAndColumn(g,0,0)==s);
    CHECK(!InventoryCrafting_getStackInRowAndColumn(g,3,0)&&!InventoryCrafting_getStackInRowAndColumn(g,0,3));
    CHECK(!InventoryCrafting_getStackInRowAndColumn(g,-1,0)&&!MCObjectHeap_failed(h));
    out=InventoryCrafting_decrStackSize(g,0,2); CHECK(out&&out!=s&&out->stackSize==2&&s->stackSize==3&&state->calls==2&&state->seen[1]==3);
    out=InventoryCrafting_decrStackSize(g,0,0); CHECK(out&&out->item==s->item&&out->stackSize==0&&state->calls==3&&state->seen[2]==3);
    out=InventoryCrafting_decrStackSize(g,0,4); CHECK(out==s&&s->stackSize==3&&!InventoryCrafting_getStackInSlot(g,0)&&state->calls==4&&state->seen[3]==INT32_MIN);
    CHECK(!InventoryCrafting_decrStackSize(g,0,1)&&state->calls==4);
    CHECK(InventoryCrafting_setInventorySlotContents(g,0,NULL)); CHECK(state->calls==5&&state->seen[4]==INT32_MIN);
    CHECK(InventoryCrafting_setInventorySlotContents(g,0,s)); CHECK(InventoryCrafting_removeStackFromSlot(g,0)==s&&state->calls==6);
    CHECK(InventoryCrafting_setInventorySlotContents(g,0,s)); InventoryCrafting_clear(g); CHECK(state->calls==7&&!InventoryCrafting_getStackInSlot(g,0));
    state->reject=true; CHECK(!InventoryCrafting_setInventorySlotContents(g,0,s)); CHECK(MCObjectHeap_failed(h)&&state->calls==8&&g->stackList->items[0]==s);
    MCObjectHeap_free(h);
    h=MCObjectHeap_new(1024*1024); g=InventoryCrafting_new(h,NULL,NULL,0,3); CHECK(g&&!InventoryCrafting_getSizeInventory(g));
    CHECK(!InventoryCrafting_new(h,NULL,NULL,INT32_MAX,2)&&MCObjectHeap_failed(h)); MCObjectHeap_free(h);
    h=MCObjectHeap_new(1024*1024); g=InventoryCrafting_new(h,NULL,NULL,2,2); CHECK(g); CHECK(!InventoryCrafting_getStackInSlot(g,-1)&&MCObjectHeap_failed(h)); MCObjectHeap_free(h);
}
static void result_methods(void) {
    MCObjectHeap *h=MCObjectHeap_new(1024*1024); InventoryCraftResult *r=InventoryCraftResult_new(h); CHECK(r);
    CHECK(InventoryCraftResult_getSizeInventory(r)==1&&!InventoryCraftResult_getStackInSlot(r,99));
    CHECK(!strcmp(InventoryCraftResult_getName(r),"Result")&&!InventoryCraftResult_hasCustomName(r));
    InventoryDisplayName d=InventoryCraftResult_getDisplayName(r); CHECK(d.translated&&!strcmp(d.key,"Result"));
    ItemStack *s=stack(h,276,2); CHECK(InventoryCraftResult_setInventorySlotContents(r,-99,s)); CHECK(InventoryCraftResult_getStackInSlot(r,99)==s);
    CHECK(InventoryCraftResult_decrStackSize(r,99,1)==s&&!InventoryCraftResult_getStackInSlot(r,0));
    CHECK(InventoryCraftResult_setInventorySlotContents(r,0,s)); CHECK(InventoryCraftResult_removeStackFromSlot(r,99)==s);
    CHECK(InventoryCraftResult_getInventoryStackLimit(r)==64&&InventoryCraftResult_isUseableByPlayer(r,NULL)&&InventoryCraftResult_isItemValidForSlot(r,0,s));
    InventoryCraftResult_markDirty(r); InventoryCraftResult_openInventory(r,NULL); InventoryCraftResult_closeInventory(r,NULL); InventoryCraftResult_setField(r,99,12);
    CHECK(!InventoryCraftResult_getField(r,99)&&!InventoryCraftResult_getFieldCount(r));
    CHECK(InventoryCraftResult_setInventorySlotContents(r,0,s)); InventoryCraftResult_clear(r); CHECK(!InventoryCraftResult_getStackInSlot(r,0));
    CHECK(!MCObjectHeap_failed(h)); MCObjectHeap_free(h);
}
static bool update(MCObject *handler,InventoryCrafting *g) {
    Observer *o=(Observer *)handler; ItemStack *result=RecipeBookCloning_getCraftingResult(g,NULL,NULL);
    return !MCObjectHeap_failed(g->object.heap)&&InventoryCraftResult_setInventorySlotContents(o->result,0,result);
}
static void immediate_notifications(void) {
    MCObjectHeap *h=MCObjectHeap_new(1024*1024); Observer *o=observer(h); o->result=InventoryCraftResult_new(h); CHECK(o->result);
    InventoryCrafting *g=InventoryCrafting_new(h,(MCObject *)o,update,2,2); CHECK(g);
    ItemStack *s=stack(h,387,2); NBTTagCompound *tag=NBTTagCompound_new(h); CHECK(tag&&NBTTagCompound_setInteger_ascii(tag,"generation",0)&&ItemStack_setTagCompound(s,tag));
    CHECK(InventoryCrafting_setInventorySlotContents(g,0,s)); CHECK(!InventoryCraftResult_getStackInSlot(o->result,0));
    CHECK(InventoryCrafting_setInventorySlotContents(g,1,stack(h,386,2))); CHECK(InventoryCraftResult_getStackInSlot(o->result,0)->stackSize==1);
    CHECK(InventoryCrafting_decrStackSize(g,0,1)); CHECK(s->stackSize==1&&InventoryCraftResult_getStackInSlot(o->result,0)->stackSize==1);
    CHECK(InventoryCrafting_decrStackSize(g,0,1)==s); CHECK(!InventoryCraftResult_getStackInSlot(o->result,0));
    CHECK(InventoryCrafting_setInventorySlotContents(g,0,s)); ItemStack *preview=InventoryCraftResult_getStackInSlot(o->result,0); CHECK(preview);
    CHECK(InventoryCrafting_removeStackFromSlot(g,0)==s); CHECK(InventoryCraftResult_getStackInSlot(o->result,0)==preview);
    CHECK(!MCObjectHeap_failed(h)); MCObjectHeap_free(h);
}
int main(void) { matrix_methods(); result_methods(); immediate_notifications(); printf("InventoryCrafting reference source port: %u checks passed\n",checks); return 0; }
