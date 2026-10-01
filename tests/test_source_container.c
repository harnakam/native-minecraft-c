#include "inventory/Container.h"
#include "inventory/inventory_dispatch.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"source Container check %u failed at %d: %s\n",checks,__LINE__,#x); exit(1); } } while(0)
typedef struct { MCObject object; bool creative; ItemStack *dropped[8]; unsigned drops,updates,full; ItemStack *last; } Actor;
static void actor_trace(MCObject *o,MCObjectVisitor visit,void *ctx) {
    Actor *a=(Actor *)o; for (unsigned i=0;i<a->drops;i++) a->dropped[i]=(ItemStack *)visit((MCObject *)a->dropped[i],ctx);
    a->last=(ItemStack *)visit((MCObject *)a->last,ctx);
}
static const MCObjectClass actor_class={"test.Actor",MCObjectHeap_plainClone,actor_trace,NULL};
static bool creative(const MCObject *o) { return ((const Actor *)o)->creative; }
static bool drop(MCObject *o,ItemStack *stack,bool scatter) { Actor *a=(Actor *)o; (void)scatter; CHECK(a->drops<8); a->dropped[a->drops++]=stack; return true; }
static bool can_interact(Container *c,InventoryPlayer *p) { (void)c; (void)p; return true; }
static void container_trace(MCObject *o,MCObjectVisitor visit,void *ctx) { Container_trace((Container *)o,visit,ctx); }
static const MCObjectClass container_class={"test.ConcreteContainer",MCObjectHeap_plainClone,container_trace,NULL};
static const ContainerOverrides overrides={.canInteractWith=can_interact};
static bool full_update(MCObject *target,Container *c,ContainerList *list) {
    Actor *a=(Actor *)target; (void)c; ++a->full; a->last=(ItemStack *)ContainerList_get(list,0); return true;
}
static bool slot_update(MCObject *target,Container *c,int32_t slot,ItemStack *stack) {
    Actor *a=(Actor *)target; (void)c; (void)slot; ++a->updates; a->last=stack; return true;
}
static const ICraftingMethods listener_methods={full_update,slot_update};
typedef struct { MCObjectHeap *heap; Actor *actor; InventoryPlayer *inv; Container *container; MCObjectRoot root,inventory_root; } Fixture;
static Fixture fixture(unsigned slots) {
    Fixture f={0}; f.heap=MCObjectHeap_new(16*1024*1024); CHECK(f.heap!=NULL);
    f.actor=(Actor *)MCObjectHeap_alloc(f.heap,sizeof(Actor),&actor_class); CHECK(f.actor!=NULL); f.actor->creative=true;
    f.inv=InventoryPlayer_new(f.heap,(MCObject *)f.actor,creative); CHECK(f.inv!=NULL);
    f.container=(Container *)MCObjectHeap_alloc(f.heap,sizeof(Container),&container_class); CHECK(f.container!=NULL);
    CHECK(Container_construct(f.container,&overrides,drop)); CHECK(MCObjectRoot_init(&f.root,f.heap,(MCObject *)f.container));
    CHECK(MCObjectRoot_init(&f.inventory_root,f.heap,(MCObject *)f.inv));
    for (unsigned i=0;i<slots;i++) { Slot *s=Slot_new(f.heap,mc_IInventory_player(f.inv),(int32_t)i,0,0); CHECK(s!=NULL); CHECK(Container_addSlotToContainer(f.container,s)==s); CHECK(s->slotNumber==(int32_t)i); }
    return f;
}
static ItemStack *stack(Fixture *f,int id,int count) { ItemStack *s=ItemStack_new(f->heap,ItemStack_registryItem(id),count,0); CHECK(s!=NULL); return s; }
static void clear(Fixture *f) { InventoryPlayer_clear(f->inv); InventoryPlayer_setItemStack(f->inv,NULL); Container_resetDrag(f->container); f->actor->drops=0; }
static void normal_and_signed(void) {
    Fixture f=fixture(3); Container *c=f.container; InventoryPlayer *p=f.inv;
    CHECK(Container_getSlotFromInventory(c,mc_IInventory_player(p),1)==Container_getSlot(c,1));
    ItemStack *a=stack(&f,276,2); CHECK(InventoryPlayer_setItemStack(p,a));
    CHECK(Container_slotClick(c,0,0,0,p)==NULL); CHECK(InventoryPlayer_getItemStack(p)==NULL);
    CHECK(Slot_getStack(Container_getSlot(c,0))->stackSize==2 && a->stackSize==0);
    clear(&f); a=stack(&f,1,127); CHECK(InventoryPlayer_setInventorySlotContents(p,0,a));
    ItemStack *cursor=stack(&f,1,2); CHECK(InventoryPlayer_setItemStack(p,cursor));
    ItemStack *returned=Container_slotClick(c,0,0,0,p); CHECK(returned!=a && returned->stackSize==127);
    CHECK(a->stackSize==64 && cursor->stackSize==65 && InventoryPlayer_getItemStack(p)==cursor);
    clear(&f); a=stack(&f,1,INT32_MAX); CHECK(InventoryPlayer_setInventorySlotContents(p,0,a));
    returned=Container_slotClick(c,0,1,0,p); CHECK(returned->stackSize==INT32_MAX);
    CHECK(a->stackSize==-1073741825 && InventoryPlayer_getItemStack(p)->stackSize==-1073741824);
    clear(&f); cursor=stack(&f,1,0); CHECK(InventoryPlayer_setItemStack(p,cursor));
    CHECK(Container_slotClick(c,-999,1,0,p)==NULL); CHECK(f.actor->drops==1 && f.actor->dropped[0]->stackSize==1);
    CHECK(InventoryPlayer_getItemStack(p)==cursor && cursor->stackSize==-1);
    clear(&f); a=stack(&f,1,0); CHECK(InventoryPlayer_setInventorySlotContents(p,0,a));
    returned=Container_slotClick(c,0,0,0,p); CHECK(returned && returned!=a && returned->stackSize==0);
    CHECK(InventoryPlayer_getItemStack(p)==a && a->stackSize==0 && Slot_getStack(Container_getSlot(c,0))==NULL);
    clear(&f); a=stack(&f,1,0); CHECK(InventoryPlayer_setInventorySlotContents(p,0,a));
    CHECK(Container_slotClick(c,0,127,3,p)==NULL); CHECK(InventoryPlayer_getItemStack(p)->stackSize==64 && a->stackSize==0);
    clear(&f); a=stack(&f,1,5); CHECK(InventoryPlayer_setInventorySlotContents(p,0,a));
    CHECK(Container_slotClick(c,0,-7,4,p)==NULL); CHECK(f.actor->drops==1 && f.actor->dropped[0]==a && Slot_getStack(Container_getSlot(c,0))==NULL);
    clear(&f); a=stack(&f,276,127); CHECK(Container_mergeItemStack(c,a,0,3,false));
    CHECK(a->stackSize==0 && Slot_getStack(Container_getSlot(c,0))->stackSize==127);
    CHECK(!MCObjectHeap_failed(f.heap)); MCObjectHeap_free(f.heap);
}
static void drag_identity_and_cancel(void) {
    Fixture f=fixture(0); Container *c=f.container; InventoryPlayer *p=f.inv;
    Slot *same=Slot_new(f.heap,mc_IInventory_player(p),0,0,0),*different=Slot_new(f.heap,mc_IInventory_player(p),0,0,0);
    CHECK(same && different); CHECK(Container_addSlotToContainer(c,same)==same); CHECK(Container_addSlotToContainer(c,same)==same); CHECK(Container_addSlotToContainer(c,different)==different);
    CHECK(InventoryPlayer_setItemStack(p,stack(&f,1,3)));
    CHECK(Container_slotClick(c,-999,0,5,p)==NULL); CHECK(c->dragEvent==1);
    CHECK(Container_slotClick(c,0,1,5,p)==NULL); CHECK(Container_slotClick(c,1,1,5,p)==NULL); CHECK(ContainerIdentitySet_size(c->dragSlots)==1);
    CHECK(Container_slotClick(c,2,1,5,p)==NULL); CHECK(ContainerIdentitySet_size(c->dragSlots)==2);
    CHECK(Container_slotClick(c,-999,2,5,p)==NULL); CHECK(InventoryPlayer_getStackInSlot(p,0)->stackSize==2 && InventoryPlayer_getItemStack(p)->stackSize==1);
    clear(&f); ItemStack *cursor=stack(&f,1,1); CHECK(InventoryPlayer_setItemStack(p,cursor));
    CHECK(Container_slotClick(c,-999,8,5,p)==NULL); CHECK(Container_slotClick(c,0,9,5,p)==NULL); CHECK(Container_slotClick(c,2,9,5,p)==NULL);
    CHECK(ContainerIdentitySet_size(c->dragSlots)==1); CHECK(Container_slotClick(c,-999,10,5,p)==NULL);
    CHECK(InventoryPlayer_getStackInSlot(p,0)->stackSize==64 && InventoryPlayer_getItemStack(p)==NULL && cursor->stackSize==1);
    clear(&f); cursor=stack(&f,1,8); CHECK(InventoryPlayer_setItemStack(p,cursor));
    CHECK(Container_slotClick(c,-999,0,5,p)==NULL); CHECK(Container_slotClick(c,0,0,0,p)==NULL);
    CHECK(c->dragEvent==0 && InventoryPlayer_getItemStack(p)==cursor && InventoryPlayer_getStackInSlot(p,0)==NULL);
    clear(&f); cursor=stack(&f,1,0); CHECK(InventoryPlayer_setItemStack(p,cursor));
    CHECK(Container_slotClick(c,-999,0,5,p)==NULL); CHECK(Container_slotClick(c,0,1,5,p)==NULL); CHECK(Container_slotClick(c,-999,2,5,p)==NULL);
    CHECK(InventoryPlayer_getItemStack(p)==cursor && cursor->stackSize==0 && c->dragEvent==0);
    CHECK(!MCObjectHeap_failed(f.heap)); MCObjectHeap_free(f.heap);
}
static void observers_and_transactions(void) {
    Fixture f=fixture(2); Container *c=f.container; ItemStack *a=stack(&f,1,0); CHECK(InventoryPlayer_setInventorySlotContents(f.inv,0,a));
    ContainerList *live=Container_getInventory(c); CHECK(live && ContainerList_size(live)==2 && ContainerList_get(live,0)==(MCObject *)a);
    CHECK(Container_onCraftGuiOpened(c,(ICrafting){(MCObject *)f.actor,&listener_methods}));
    CHECK(f.actor->full==1 && f.actor->updates==1 && f.actor->last!=a && f.actor->last->stackSize==0);
    a->stackSize=-1; MCObjectHeap_touch(f.heap); CHECK(Container_detectAndSendChanges(c)); CHECK(f.actor->updates==2 && f.actor->last->stackSize==-1);
    CHECK(Container_detectAndSendChanges(c)); CHECK(f.actor->updates==2);
    CHECK(Container_removeCraftingFromCrafters(c,(ICrafting){(MCObject *)f.actor,&listener_methods}));
    c->transactionID=INT16_MAX; CHECK(Container_getNextTransactionID(c,f.inv)==INT16_MIN); CHECK(Container_getNextTransactionID(c,NULL)==INT16_MIN+1);
    CHECK(Container_getCanCraft(c,(MCObject *)f.actor)); CHECK(Container_setCanCraft(c,(MCObject *)f.actor,false)); CHECK(!Container_getCanCraft(c,(MCObject *)f.actor));
    CHECK(Container_setCanCraft(c,(MCObject *)f.actor,true)); CHECK(Container_getCanCraft(c,(MCObject *)f.actor));
    CHECK(Container_calcRedstoneFromInventory(mc_IInventory_player(f.inv))==0);
    a->stackSize=0; CHECK(Container_calcRedstoneFromInventory(mc_IInventory_player(f.inv))==1);
    CHECK(!MCObjectHeap_failed(f.heap)); MCObjectHeap_free(f.heap);
    f=fixture(1); c=f.container; c->drop=NULL; a=stack(&f,1,2); CHECK(InventoryPlayer_setItemStack(f.inv,a));
    CHECK(Container_slotClick(c,-999,0,0,f.inv)==NULL); CHECK(MCObjectHeap_failed(f.heap)); CHECK(InventoryPlayer_getItemStack(f.inv)==a);
    MCObjectHeap_free(f.heap);
}
static bool reject_pickup(Slot *slot,MCObject *player,ItemStack *stack) { (void)slot; (void)player; (void)stack; return false; }
static const SlotOverrides rejected_pickup={.onPickupFromSlot=reject_pickup};
static void drag_snapshot_and_helpers(void) {
    Fixture f=fixture(21); Container *c=f.container; InventoryPlayer *p=f.inv;
    int32_t original_order[20],original_hash[20];
    CHECK(InventoryPlayer_setItemStack(p,stack(&f,1,64))); CHECK(Container_slotClick(c,-999,0,5,p)==NULL);
    for (int i=0;i<20;i++) CHECK(Container_slotClick(c,i,1,5,p)==NULL);
    CHECK(ContainerIdentitySet_size(c->dragSlots)==20);
    for (int i=0;i<20;i++) { Slot *s=(Slot *)ContainerIdentitySet_getAt(c->dragSlots,i); original_order[i]=s->slotNumber; original_hash[i]=MCObjectHeap_identityHashCode((MCObject *)s); }
    MCObjectHeap *working=MCObjectHeap_clone(f.heap); CHECK(working!=NULL); MCObjectRoot copied={0},inventory={0};
    CHECK(MCObjectRoot_rebind(&copied,working,&f.root)); CHECK(MCObjectRoot_rebind(&inventory,working,&f.inventory_root));
    Container *copy=(Container *)MCObjectRoot_get(&copied); InventoryPlayer *copy_inventory=(InventoryPlayer *)MCObjectRoot_get(&inventory);
    for (int i=0;i<20;i++) { Slot *s=(Slot *)ContainerIdentitySet_getAt(copy->dragSlots,i); CHECK(s->slotNumber==original_order[i] && MCObjectHeap_identityHashCode((MCObject *)s)==original_hash[i]); }
    CHECK(Container_slotClick(copy,0,1,5,copy_inventory)==NULL); CHECK(ContainerIdentitySet_size(copy->dragSlots)==20);
    CHECK(Container_slotClick(copy,20,1,5,copy_inventory)==NULL); CHECK(ContainerIdentitySet_size(copy->dragSlots)==21);
    CHECK(MCObjectHeap_adopt(f.heap,working)); MCObjectHeap_free(working); c=(Container *)MCObjectRoot_get(&f.root); p=(InventoryPlayer *)MCObjectRoot_get(&f.inventory_root);
    CHECK(Container_slotClick(c,-999,2,5,p)==NULL); CHECK(InventoryPlayer_getItemStack(p)->stackSize==1);
    for (int i=0;i<21;i++) CHECK(Slot_getStack(Container_getSlot(c,i))->stackSize==3);
    Container_resetDrag(c);
    const int32_t amounts[]={0,1,-1,INT32_MIN,INT32_MAX};
    for (unsigned i=0;i<sizeof amounts/sizeof amounts[0];i++) {
        ItemStack *s=ItemStack_new(f.heap,ItemStack_registryItem(1),amounts[i],0); CHECK(s!=NULL);
        Container_computeStackSize(c->dragSlots,0,s,0); CHECK(s->stackSize==(amounts[i]==0 ? 0 : INT32_MAX));
    }
    CHECK(Container_extractDragMode(-1)==3 && Container_getDragEvent(-1)==3 && Container_func_94534_d(6,5)==6);
    CHECK(!MCObjectHeap_failed(f.heap)); MCObjectHeap_free(f.heap);
    f=fixture(1); Slot *slot=Container_getSlot(f.container,0); slot->overrides=&rejected_pickup;
    ItemStack *a=stack(&f,1,2); CHECK(InventoryPlayer_setInventorySlotContents(f.inv,0,a));
    CHECK(Container_slotClick(f.container,0,0,4,f.inv)==NULL); CHECK(MCObjectHeap_failed(f.heap));
    CHECK(f.actor->drops==0 && a->stackSize==1);
    MCObjectHeap_free(f.heap);
}
static unsigned transfers;
static ItemStack *transfer_one(Container *c,InventoryPlayer *p,int32_t index) {
    ++transfers; Slot *source=Container_getSlot(c,index); ItemStack *stack=Slot_getStack(source);
    if (!stack || stack->stackSize<=0) return NULL;
    ItemStack *previous=ItemStack_copy(c->object.heap,stack),*part=Slot_decrStackSize(source,1);
    CHECK(previous && part);
    ItemStack *target=InventoryPlayer_getStackInSlot(p,1);
    if (target) target->stackSize+=part->stackSize; else CHECK(InventoryPlayer_setInventorySlotContents(p,1,part));
    CHECK(Slot_onPickupFromSlot(source,p->player,part)); return previous;
}
static const ContainerOverrides transfer_overrides={.canInteractWith=can_interact,.transferStackInSlot=transfer_one};
static void shift_number_collect(void) {
    Fixture f=fixture(3); Container *c=f.container; InventoryPlayer *p=f.inv; ItemStack *a=stack(&f,1,3);
    CHECK(InventoryPlayer_setInventorySlotContents(p,0,a)); c->overrides=&transfer_overrides; transfers=0;
    ItemStack *returned=Container_slotClick(c,0,1,1,p);
    CHECK(returned && returned!=a && returned->stackSize==3 && transfers==3);
    CHECK(InventoryPlayer_getStackInSlot(p,0)==NULL && InventoryPlayer_getStackInSlot(p,1)->stackSize==3);
    clear(&f); c->overrides=&overrides; a=stack(&f,1,127); CHECK(InventoryPlayer_setInventorySlotContents(p,0,a));
    CHECK(Container_slotClick(c,1,0,2,p)==NULL); CHECK(InventoryPlayer_getStackInSlot(p,0)==NULL && InventoryPlayer_getStackInSlot(p,1)==a && a->stackSize==127);
    clear(&f); a=stack(&f,1,10); ItemStack *b=stack(&f,5,5);
    CHECK(InventoryPlayer_setInventorySlotContents(p,0,a)); CHECK(InventoryPlayer_setInventorySlotContents(p,1,b));
    CHECK(Container_slotClick(c,1,0,2,p)==NULL); CHECK(InventoryPlayer_getStackInSlot(p,1)==a && InventoryPlayer_getStackInSlot(p,0)!=b && InventoryPlayer_getStackInSlot(p,0)->stackSize==5);
    clear(&f); a=stack(&f,1,10); CHECK(InventoryPlayer_setInventorySlotContents(p,0,a));
    CHECK(Container_slotClick(c,0,0,2,p)==NULL); CHECK(InventoryPlayer_getStackInSlot(p,0)==a && a->stackSize==10);
    clear(&f); ItemStack *cursor=stack(&f,1,1); CHECK(InventoryPlayer_setItemStack(p,cursor));
    CHECK(InventoryPlayer_setInventorySlotContents(p,1,stack(&f,1,0))); CHECK(InventoryPlayer_setInventorySlotContents(p,2,stack(&f,1,-1)));
    CHECK(Container_slotClick(c,0,0,6,p)==NULL); CHECK(InventoryPlayer_getItemStack(p)==cursor && cursor->stackSize==0);
    CHECK(InventoryPlayer_getStackInSlot(p,1)==NULL && InventoryPlayer_getStackInSlot(p,2)==NULL);
    clear(&f); cursor=stack(&f,1,1); CHECK(InventoryPlayer_setItemStack(p,cursor));
    CHECK(InventoryPlayer_setInventorySlotContents(p,1,stack(&f,1,50))); CHECK(InventoryPlayer_setInventorySlotContents(p,2,stack(&f,1,50)));
    CHECK(Container_slotClick(c,0,127,6,p)==NULL); CHECK(cursor->stackSize==64 && InventoryPlayer_getStackInSlot(p,1)->stackSize==37 && InventoryPlayer_getStackInSlot(p,2)==NULL);
    clear(&f); a=stack(&f,1,5); CHECK(InventoryPlayer_setInventorySlotContents(p,0,a)); CHECK(InventoryPlayer_setItemStack(p,a));
    returned=Container_slotClick(c,0,0,0,p); CHECK(returned && returned!=a && returned->stackSize==5);
    CHECK(InventoryPlayer_getStackInSlot(p,0)==a && a->stackSize==5 && InventoryPlayer_getItemStack(p)==NULL);
    CHECK(!MCObjectHeap_failed(f.heap)); MCObjectHeap_free(f.heap);
}
int main(void) {
    normal_and_signed(); drag_identity_and_cancel(); observers_and_transactions(); drag_snapshot_and_helpers(); shift_number_collect();
    printf("source Container: %u checks passed\n",checks); return 0;
}
