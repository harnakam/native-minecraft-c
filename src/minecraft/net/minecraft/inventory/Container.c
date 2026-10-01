#include "inventory/Container.h"
#include <limits.h>
#include <math.h>
#include <string.h>

typedef struct { MCObject object; int32_t capacity; MCObject *items[]; } RefArray;
struct ContainerList { MCObject object; RefArray *storage; int32_t size; };
typedef struct { MCObject *value; uint32_t hash; int32_t next; bool occupied; } SetNode;
typedef struct { MCObject object; int32_t capacity; SetNode nodes[]; } NodeArray;
typedef struct { MCObject object; int32_t capacity; int32_t heads[]; } BucketArray;
struct ContainerIdentitySet {
    MCObject object; NodeArray *nodes; BucketArray *buckets;
    int32_t size,used,freeHead; uint32_t modifications;
};
typedef struct { MCObject object; ICrafting listener; } Listener;
static int32_t signed_bits(uint32_t x) { return x<=INT32_MAX ? (int32_t)x : -1-(int32_t)(UINT32_MAX-x); }
static int32_t add(int32_t a,int32_t b) { return signed_bits((uint32_t)a+(uint32_t)b); }
static int32_t sub(int32_t a,int32_t b) { return signed_bits((uint32_t)a-(uint32_t)b); }
static bool error(MCObjectHeap *heap) { MCObjectHeap_fail(heap); return false; }
static bool same_heap(MCObjectHeap *heap,const MCObject *object) { return !object || object->heap==heap || error(heap); }
static bool result(MCObjectHeap *heap,bool ok) { return ok ? !MCObjectHeap_failed(heap) : error(heap); }
static void refs_trace(MCObject *o,MCObjectVisitor visit,void *ctx) {
    RefArray *a=(RefArray *)o; for (int32_t i=0;i<a->capacity;i++) a->items[i]=visit(a->items[i],ctx);
}
static void list_trace(MCObject *o,MCObjectVisitor visit,void *ctx) {
    ContainerList *a=(ContainerList *)o; a->storage=(RefArray *)visit((MCObject *)a->storage,ctx);
}
static void nodes_trace(MCObject *o,MCObjectVisitor visit,void *ctx) {
    NodeArray *a=(NodeArray *)o; for (int32_t i=0;i<a->capacity;i++) if (a->nodes[i].occupied) a->nodes[i].value=visit(a->nodes[i].value,ctx);
}
static void set_trace(MCObject *o,MCObjectVisitor visit,void *ctx) {
    ContainerIdentitySet *a=(ContainerIdentitySet *)o;
    a->nodes=(NodeArray *)visit((MCObject *)a->nodes,ctx); a->buckets=(BucketArray *)visit((MCObject *)a->buckets,ctx);
}
static void listener_trace(MCObject *o,MCObjectVisitor visit,void *ctx) { Listener *a=(Listener *)o; a->listener.target=visit(a->listener.target,ctx); }
static const MCObjectClass refs_class={"native.Container.RefArray",MCObjectHeap_plainClone,refs_trace,NULL};
static const MCObjectClass list_class={"native.Container.ArrayList",MCObjectHeap_plainClone,list_trace,NULL};
static const MCObjectClass nodes_class={"native.Container.SetNodes",MCObjectHeap_plainClone,nodes_trace,NULL};
static const MCObjectClass buckets_class={"native.Container.HashBuckets",MCObjectHeap_plainClone,NULL,NULL};
static const MCObjectClass set_class={"native.Container.HashSet",MCObjectHeap_plainClone,set_trace,NULL};
static const MCObjectClass listener_class={"native.Container.ICrafting",MCObjectHeap_plainClone,listener_trace,NULL};
static ContainerList *list_new(MCObjectHeap *heap) { return (ContainerList *)MCObjectHeap_alloc(heap,sizeof(ContainerList),&list_class); }
static bool list_add(ContainerList *list,MCObject *value) {
    MCObjectHeap *heap=list->object.heap;
    if (!same_heap(heap,value) || list->size==INT32_MAX) return error(heap);
    if (!list->storage || list->size==list->storage->capacity) {
        int64_t capacity=list->storage ? (int64_t)list->storage->capacity+list->storage->capacity/2 : 10;
        if (capacity<=list->size) capacity=(int64_t)list->size+1;
        if (capacity>INT32_MAX || (size_t)capacity>(SIZE_MAX-sizeof(RefArray))/sizeof(MCObject *)) return error(heap);
        RefArray *storage=(RefArray *)MCObjectHeap_alloc(heap,sizeof(RefArray)+(size_t)capacity*sizeof(MCObject *),&refs_class);
        if (!storage) return false;
        storage->capacity=(int32_t)capacity;
        if (list->size) memcpy(storage->items,list->storage->items,(size_t)list->size*sizeof(MCObject *));
        list->storage=storage;
    }
    list->storage->items[list->size++]=value; MCObjectHeap_touch(heap); return true;
}
int32_t ContainerList_size(const ContainerList *list) { return list ? list->size : 0; }
MCObject *ContainerList_get(ContainerList *list,int32_t index) {
    if (!list) return NULL;
    if (index<0 || index>=list->size) { error(list->object.heap); return NULL; }
    return list->storage->items[index];
}
static bool list_set(ContainerList *list,int32_t index,MCObject *value) {
    if (!list || index<0 || index>=list->size || !same_heap(list->object.heap,value)) return list ? error(list->object.heap) : false;
    list->storage->items[index]=value; MCObjectHeap_touch(list->object.heap); return true;
}
static void list_remove(ContainerList *list,int32_t index) {
    memmove(list->storage->items+index,list->storage->items+index+1,(size_t)(list->size-index-1)*sizeof(MCObject *));
    list->storage->items[--list->size]=NULL; MCObjectHeap_touch(list->object.heap);
}
static ContainerIdentitySet *set_new(MCObjectHeap *heap) {
    ContainerIdentitySet *set=(ContainerIdentitySet *)MCObjectHeap_alloc(heap,sizeof(*set),&set_class);
    if (set) set->freeHead=-1;
    return set;
}
static uint32_t identity_hash(const MCObject *object) { uint32_t h=(uint32_t)MCObjectHeap_identityHashCode(object); return h^(h>>16); }
static bool set_resize(ContainerIdentitySet *set,int32_t capacity) {
    MCObjectHeap *heap=set->object.heap;
    if (capacity<=0 || (size_t)capacity>(SIZE_MAX-sizeof(NodeArray))/sizeof(SetNode) ||
        (size_t)capacity>(SIZE_MAX-sizeof(BucketArray))/sizeof(int32_t)) return error(heap);
    NodeArray *nodes=(NodeArray *)MCObjectHeap_alloc(heap,sizeof(NodeArray)+(size_t)capacity*sizeof(SetNode),&nodes_class);
    BucketArray *buckets=(BucketArray *)MCObjectHeap_alloc(heap,sizeof(BucketArray)+(size_t)capacity*sizeof(int32_t),&buckets_class);
    if (!nodes || !buckets) return false;
    nodes->capacity=capacity; buckets->capacity=capacity;
    for (int32_t i=0;i<capacity;i++) buckets->heads[i]=-1;
    if (set->used) memcpy(nodes->nodes,set->nodes->nodes,(size_t)set->used*sizeof(SetNode));
    if (set->buckets) for (int32_t b=0;b<set->buckets->capacity;b++) {
        for (int32_t i=set->buckets->heads[b];i>=0;i=set->nodes->nodes[i].next) {
            int32_t bucket=(int32_t)(nodes->nodes[i].hash&(uint32_t)(capacity-1)); nodes->nodes[i].next=-1;
            int32_t *tail=&buckets->heads[bucket]; while (*tail>=0) tail=&nodes->nodes[*tail].next; *tail=i;
        }
    }
    set->nodes=nodes; set->buckets=buckets; MCObjectHeap_touch(heap); return true;
}
static int32_t set_find(const ContainerIdentitySet *set,const MCObject *value) {
    if (!set->buckets) return -1;
    uint32_t h=identity_hash(value); int32_t bucket=(int32_t)(h&(uint32_t)(set->buckets->capacity-1));
    for (int32_t i=set->buckets->heads[bucket];i>=0;i=set->nodes->nodes[i].next)
        if (set->nodes->nodes[i].hash==h && set->nodes->nodes[i].value==value) return i;
    return -1;
}
static bool set_add(ContainerIdentitySet *set,MCObject *value) {
    MCObjectHeap *heap=set->object.heap;
    if (!same_heap(heap,value)) return false;
    if (set_find(set,value)>=0) return true;
    if (!set->buckets && !set_resize(set,16)) return false;
    if (set->size>=set->buckets->capacity-set->buckets->capacity/4) {
        if (set->buckets->capacity>INT32_MAX/2 || !set_resize(set,set->buckets->capacity*2)) return error(heap);
    }
    int32_t index;
    if (set->freeHead>=0) { index=set->freeHead; set->freeHead=set->nodes->nodes[index].next; }
    else { index=set->used++; }
    uint32_t h=identity_hash(value); int32_t bucket=(int32_t)(h&(uint32_t)(set->buckets->capacity-1));
    set->nodes->nodes[index]=(SetNode){value,h,-1,true};
    int32_t *tail=&set->buckets->heads[bucket]; while (*tail>=0) tail=&set->nodes->nodes[*tail].next; *tail=index;
    ++set->size; ++set->modifications; MCObjectHeap_touch(heap); return true;
}
static void set_remove(ContainerIdentitySet *set,MCObject *value) {
    if (!set->buckets) return;
    int32_t bucket=(int32_t)(identity_hash(value)&(uint32_t)(set->buckets->capacity-1));
    int32_t *link=&set->buckets->heads[bucket];
    while (*link>=0) {
        SetNode *node=&set->nodes->nodes[*link];
        if (node->value==value) {
            int32_t index=*link; *link=node->next; *node=(SetNode){NULL,0,set->freeHead,false}; set->freeHead=index;
            --set->size; ++set->modifications; MCObjectHeap_touch(set->object.heap); return;
        }
        link=&node->next;
    }
}
static void set_clear(ContainerIdentitySet *set) {
    if (set->buckets) for (int32_t i=0;i<set->buckets->capacity;i++) set->buckets->heads[i]=-1;
    if (set->nodes) memset(set->nodes->nodes,0,(size_t)set->nodes->capacity*sizeof(SetNode));
    set->size=0; set->used=0; set->freeHead=-1; ++set->modifications; MCObjectHeap_touch(set->object.heap);
}
int32_t ContainerIdentitySet_size(const ContainerIdentitySet *set) { return set ? set->size : 0; }
MCObject *ContainerIdentitySet_getAt(ContainerIdentitySet *set,int32_t index) {
    if (!set) return NULL;
    if (index<0 || index>=set->size) { error(set->object.heap); return NULL; }
    for (int32_t b=0;b<set->buckets->capacity;b++) for (int32_t i=set->buckets->heads[b];i>=0;i=set->nodes->nodes[i].next)
        if (index--==0) return set->nodes->nodes[i].value;
    error(set->object.heap); return NULL;
}
bool Container_construct(Container *c,const ContainerOverrides *overrides,ContainerDrop drop) {
    if (!c) return false;
    MCObjectHeap *heap=c->object.heap;
    c->inventoryItemStacks=list_new(heap); c->inventorySlots=list_new(heap); c->crafters=list_new(heap);
    c->dragSlots=set_new(heap); c->playerList=set_new(heap); c->dragMode=-1;
    c->overrides=overrides; c->drop=drop;
    return c->inventoryItemStacks && c->inventorySlots && c->crafters && c->dragSlots && c->playerList;
}
void Container_trace(Container *c,MCObjectVisitor visit,void *ctx) {
    c->inventoryItemStacks=(ContainerList *)visit((MCObject *)c->inventoryItemStacks,ctx);
    c->inventorySlots=(ContainerList *)visit((MCObject *)c->inventorySlots,ctx);
    c->crafters=(ContainerList *)visit((MCObject *)c->crafters,ctx);
    c->dragSlots=(ContainerIdentitySet *)visit((MCObject *)c->dragSlots,ctx);
    c->playerList=(ContainerIdentitySet *)visit((MCObject *)c->playerList,ctx);
}
Slot *Container_addSlotToContainer(Container *c,Slot *slot) {
    if (!slot || !same_heap(c->object.heap,(MCObject *)slot)) { error(c->object.heap); return NULL; }
    slot->slotNumber=c->inventorySlots->size;
    return list_add(c->inventorySlots,(MCObject *)slot) && list_add(c->inventoryItemStacks,NULL) ? slot : NULL;
}
bool Container_onCraftGuiOpened(Container *c,ICrafting listener) {
    MCObjectHeap *heap=c->object.heap;
    if (!listener.target || !same_heap(heap,listener.target) || !listener.methods ||
        !listener.methods->updateCraftingInventory || !listener.methods->sendSlotContents) return error(heap);
    for (int32_t i=0;i<c->crafters->size;i++) if (((Listener *)ContainerList_get(c->crafters,i))->listener.target==listener.target) return error(heap);
    Listener *entry=(Listener *)MCObjectHeap_alloc(heap,sizeof(*entry),&listener_class); if (!entry) return false;
    entry->listener=listener; if (!list_add(c->crafters,(MCObject *)entry)) return false;
    ContainerList *stacks=Container_getInventory(c); if (!stacks) return false;
    if (!result(heap,listener.methods->updateCraftingInventory(listener.target,c,stacks))) return false;
    return Container_detectAndSendChanges(c);
}
bool Container_removeCraftingFromCrafters(Container *c,ICrafting listener) {
    for (int32_t i=0;i<c->crafters->size;i++) if (((Listener *)ContainerList_get(c->crafters,i))->listener.target==listener.target) { list_remove(c->crafters,i); break; }
    return !MCObjectHeap_failed(c->object.heap);
}
ContainerList *Container_getInventory(Container *c) {
    ContainerList *list=list_new(c->object.heap); if (!list) return NULL;
    for (int32_t i=0;i<c->inventorySlots->size;i++) {
        ItemStack *stack=Slot_getStack(Container_getSlot(c,i));
        if (MCObjectHeap_failed(c->object.heap) || !list_add(list,(MCObject *)stack)) return NULL;
    }
    return list;
}
bool Container_detectAndSendChanges(Container *c) {
    MCObjectHeap *heap=c->object.heap;
    for (int32_t i=0;i<c->inventorySlots->size;i++) {
        ItemStack *current=Slot_getStack(Container_getSlot(c,i));
        ItemStack *previous=(ItemStack *)ContainerList_get(c->inventoryItemStacks,i);
        if (MCObjectHeap_failed(heap)) return false;
        if (!ItemStack_areItemStacksEqual(previous,current)) {
            previous=current ? ItemStack_copy(heap,current) : NULL;
            if (MCObjectHeap_failed(heap) || !list_set(c->inventoryItemStacks,i,(MCObject *)previous)) return false;
            for (int32_t j=0;j<c->crafters->size;j++) {
                ICrafting listener=((Listener *)ContainerList_get(c->crafters,j))->listener;
                if (!result(heap,listener.methods->sendSlotContents(listener.target,c,i,previous))) return false;
            }
        }
    }
    return !MCObjectHeap_failed(heap);
}
bool Container_enchantItem(Container *c,InventoryPlayer *player,int32_t id) { return c->overrides && c->overrides->enchantItem ? c->overrides->enchantItem(c,player,id) : false; }
Slot *Container_getSlotFromInventory(Container *c,IInventory inventory,int32_t index) {
    for (int32_t i=0;i<c->inventorySlots->size;i++) { Slot *slot=Container_getSlot(c,i); if (Slot_isHere(slot,inventory,index)) return slot; }
    return NULL;
}
Slot *Container_getSlot(Container *c,int32_t index) { return (Slot *)ContainerList_get(c->inventorySlots,index); }
ItemStack *Container_transferStackInSlot(Container *c,InventoryPlayer *player,int32_t index) {
    if (c->overrides && c->overrides->transferStackInSlot) return c->overrides->transferStackInSlot(c,player,index);
    Slot *slot=Container_getSlot(c,index); return slot ? Slot_getStack(slot) : NULL;
}
bool Container_canMergeSlot(Container *c,const ItemStack *stack,const Slot *slot) { return !c->overrides || !c->overrides->canMergeSlot || c->overrides->canMergeSlot(c,stack,slot); }
void Container_retrySlotClick(Container *c,int32_t index,int32_t button,bool mode,InventoryPlayer *player) {
    if (c->overrides && c->overrides->retrySlotClick) c->overrides->retrySlotClick(c,index,button,mode,player);
    else (void)Container_slotClick(c,index,button,1,player);
}
static bool drop_stack(Container *c,InventoryPlayer *player,ItemStack *stack,bool scatter) {
    return c->drop ? result(c->object.heap,c->drop(player->player,stack,scatter)) : error(c->object.heap);
}
bool Container_onContainerClosedBase(Container *c,InventoryPlayer *player) {
    ItemStack *stack=InventoryPlayer_getItemStack(player);
    if (stack && (!drop_stack(c,player,stack,false) || !InventoryPlayer_setItemStack(player,NULL))) return false;
    return !MCObjectHeap_failed(c->object.heap);
}
bool Container_onContainerClosed(Container *c,InventoryPlayer *player) { return c->overrides && c->overrides->onContainerClosed ? result(c->object.heap,c->overrides->onContainerClosed(c,player)) : Container_onContainerClosedBase(c,player); }
bool Container_onCraftMatrixChanged(Container *c,IInventory inventory) { return c->overrides && c->overrides->onCraftMatrixChanged ? result(c->object.heap,c->overrides->onCraftMatrixChanged(c,inventory)) : Container_detectAndSendChanges(c); }
bool Container_putStackInSlot(Container *c,int32_t index,ItemStack *stack) { Slot *slot=Container_getSlot(c,index); return slot && result(c->object.heap,Slot_putStack(slot,stack)); }
bool Container_putStacksInSlots(Container *c,const ItemStackArray *stacks) {
    if (!stacks) return error(c->object.heap);
    for (int32_t i=0;i<stacks->length;i++) if (!Container_putStackInSlot(c,i,stacks->items[i])) return false;
    return true;
}
void Container_updateProgressBar(Container *c,int32_t id,int32_t data) { if (c->overrides && c->overrides->updateProgressBar) c->overrides->updateProgressBar(c,id,data); /* Original base body is empty. */ }
int16_t Container_getNextTransactionID(Container *c,InventoryPlayer *unused) {
    (void)unused; uint16_t bits=(uint16_t)((uint16_t)c->transactionID+1u);
    c->transactionID=bits<=INT16_MAX ? (int16_t)bits : (int16_t)(-1-(int32_t)(UINT16_MAX-bits)); MCObjectHeap_touch(c->object.heap); return c->transactionID;
}
bool Container_getCanCraft(Container *c,const MCObject *player) { return set_find(c->playerList,player)<0; }
bool Container_setCanCraft(Container *c,MCObject *player,bool enabled) { if (enabled) set_remove(c->playerList,player); else if (!set_add(c->playerList,player)) return false; return !MCObjectHeap_failed(c->object.heap); }
bool Container_canInteractWith(Container *c,InventoryPlayer *player) { return c->overrides && c->overrides->canInteractWith ? c->overrides->canInteractWith(c,player) : error(c->object.heap); }
bool Container_mergeItemStack(Container *c,ItemStack *stack,int32_t begin,int32_t end,bool reverse) {
    bool moved=false; int32_t i=reverse ? sub(end,1) : begin;
    if (!stack) return error(c->object.heap);
    if (ItemStack_isStackable(stack)) while (stack->stackSize>0 && ((!reverse && i<end) || (reverse && i>=begin))) {
        Slot *slot=Container_getSlot(c,i); if (!slot) return false;
        ItemStack *target=Slot_getStack(slot);
        if (target && ItemStack_getItem(target)==ItemStack_getItem(stack) && (!ItemStack_getHasSubtypes(stack) || ItemStack_getMetadata(stack)==ItemStack_getMetadata(target)) && ItemStack_areItemStackTagsEqual(stack,target)) {
            int32_t sum=add(target->stackSize,stack->stackSize),limit=ItemStack_getMaxStackSize(stack);
            if (sum<=limit) { stack->stackSize=0; target->stackSize=sum; Slot_onSlotChanged(slot); moved=true; }
            else if (target->stackSize<limit) { stack->stackSize=sub(stack->stackSize,sub(limit,target->stackSize)); target->stackSize=limit; Slot_onSlotChanged(slot); moved=true; }
        }
        if (MCObjectHeap_failed(c->object.heap)) return false;
        i=reverse ? sub(i,1) : add(i,1);
    }
    if (stack->stackSize>0) {
        i=reverse ? sub(end,1) : begin;
        while ((!reverse && i<end) || (reverse && i>=begin)) {
            Slot *slot=Container_getSlot(c,i); if (!slot) return false;
            if (!Slot_getStack(slot)) {
                ItemStack *copy=ItemStack_copy(c->object.heap,stack);
                if (!copy || !result(c->object.heap,Slot_putStack(slot,copy))) return false;
                Slot_onSlotChanged(slot); if (MCObjectHeap_failed(c->object.heap)) return false;
                stack->stackSize=0; moved=true; break;
            }
            i=reverse ? sub(i,1) : add(i,1);
        }
    }
    MCObjectHeap_touch(c->object.heap); return moved;
}
int32_t Container_extractDragMode(int32_t button) { return (int32_t)(((uint32_t)button>>2)&3u); }
int32_t Container_getDragEvent(int32_t button) { return button&3; }
int32_t Container_func_94534_d(int32_t event,int32_t mode) { return (event&3)|((mode&3)<<2); }
bool Container_isValidDragMode(int32_t mode,const InventoryPlayer *player) {
    if (mode==0 || mode==1) return true;
    if (mode!=2) return false;
    if (!player || !player->isCreativeMode) { if (player) error(player->object.heap); return false; }
    return player->isCreativeMode(player->player);
}
void Container_resetDrag(Container *c) { c->dragEvent=0; set_clear(c->dragSlots); MCObjectHeap_touch(c->object.heap); }
bool Container_canAddItemToSlot(Slot *slot,const ItemStack *stack,bool size_matters) {
    bool allowed=!slot || !Slot_getHasStack(slot);
    if (slot && Slot_getHasStack(slot) && stack && ItemStack_isItemEqual(stack,Slot_getStack(slot)) && ItemStack_areItemStackTagsEqual(Slot_getStack(slot),stack))
        allowed|=add(Slot_getStack(slot)->stackSize,size_matters ? 0 : stack->stackSize)<=ItemStack_getMaxStackSize(stack);
    return allowed;
}
static int32_t floor_float(float value) {
    int32_t integer=isnan(value) ? 0 : value>=2147483648.0f ? INT32_MAX : value<=-2147483648.0f ? INT32_MIN : (int32_t)value;
    return value<(float)integer ? sub(integer,1) : integer;
}
void Container_computeStackSize(const ContainerIdentitySet *set,int32_t mode,ItemStack *stack,int32_t previous) {
    if (mode==0) stack->stackSize=floor_float((float)stack->stackSize/(float)ContainerIdentitySet_size(set));
    else if (mode==1) stack->stackSize=1;
    else if (mode==2) stack->stackSize=ItemStack_getMaxStackSize(stack);
    stack->stackSize=add(stack->stackSize,previous); MCObjectHeap_touch(stack->object.heap);
}
bool Container_canDragIntoSlot(Container *c,const Slot *slot) { return !c->overrides || !c->overrides->canDragIntoSlot || c->overrides->canDragIntoSlot(c,slot); }
int32_t Container_calcRedstoneFromInventory(IInventory inventory) {
    if (!inventory.instance) return 0;
    if (!inventory.methods || !inventory.methods->getSizeInventory || !inventory.methods->getStackInSlot || !inventory.methods->getInventoryStackLimit) { error(inventory.instance->heap); return 0; }
    int32_t occupied=0,size=inventory.methods->getSizeInventory(inventory.instance); float fullness=0.0f;
    for (int32_t i=0;i<size;i++) {
        ItemStack *stack=inventory.methods->getStackInSlot(inventory.instance,i);
        if (stack) {
            int32_t limit=inventory.methods->getInventoryStackLimit(inventory.instance),item_limit=ItemStack_getMaxStackSize(stack);
            fullness+=(float)stack->stackSize/(float)(limit<item_limit ? limit : item_limit); ++occupied;
        }
    }
    fullness/=(float)size; return add(floor_float(fullness*14.0f),occupied>0 ? 1 : 0);
}
static ItemStack *slot_click(Container *c,int32_t index,int32_t button,int32_t mode,InventoryPlayer *p) {
    MCObjectHeap *heap=c->object.heap; ItemStack *returned=NULL;
#define REQUIRE(expression) do { if (MCObjectHeap_failed(heap) || !result(heap,(expression))) return NULL; } while (0)
#define ALIVE() do { if (MCObjectHeap_failed(heap)) return NULL; } while (0)
    if (mode==5) {
        int32_t previous=c->dragEvent; c->dragEvent=Container_getDragEvent(button);
        if ((previous!=1 || c->dragEvent!=2) && previous!=c->dragEvent) Container_resetDrag(c);
        else if (!InventoryPlayer_getItemStack(p)) Container_resetDrag(c);
        else if (c->dragEvent==0) {
            c->dragMode=Container_extractDragMode(button);
            if (Container_isValidDragMode(c->dragMode,p)) { c->dragEvent=1; set_clear(c->dragSlots); }
            else Container_resetDrag(c);
        } else if (c->dragEvent==1) {
            Slot *slot=Container_getSlot(c,index); ALIVE();
            ItemStack *cursor=InventoryPlayer_getItemStack(p);
            if (slot && Container_canAddItemToSlot(slot,cursor,true) && Slot_isItemValid(slot,cursor) &&
                cursor->stackSize>c->dragSlots->size && Container_canDragIntoSlot(c,slot)) REQUIRE(set_add(c->dragSlots,(MCObject *)slot));
        } else if (c->dragEvent==2) {
            if (c->dragSlots->size) {
                ItemStack *drag=ItemStack_copy(heap,InventoryPlayer_getItemStack(p)); ALIVE();
                if (!drag) { error(heap); return NULL; }
                int32_t remainder=InventoryPlayer_getItemStack(p)->stackSize;
                int32_t selected=c->dragSlots->size; uint32_t modifications=c->dragSlots->modifications;
                for (int32_t i=0;i<selected;i++) {
                    if (c->dragSlots->modifications!=modifications) { error(heap); return NULL; }
                    Slot *slot=(Slot *)ContainerIdentitySet_getAt(c->dragSlots,i); ALIVE();
                    ItemStack *cursor=InventoryPlayer_getItemStack(p);
                    if (slot && Container_canAddItemToSlot(slot,cursor,true) && Slot_isItemValid(slot,cursor) &&
                        cursor->stackSize>=c->dragSlots->size && Container_canDragIntoSlot(c,slot)) {
                        ItemStack *next=ItemStack_copy(heap,drag); ALIVE(); if (!next) { error(heap); return NULL; }
                        int32_t old_count=Slot_getHasStack(slot) ? Slot_getStack(slot)->stackSize : 0;
                        Container_computeStackSize(c->dragSlots,c->dragMode,next,old_count);
                        if (next->stackSize>ItemStack_getMaxStackSize(next)) next->stackSize=ItemStack_getMaxStackSize(next);
                        if (next->stackSize>Slot_getItemStackLimit(slot,next)) next->stackSize=Slot_getItemStackLimit(slot,next);
                        remainder=sub(remainder,sub(next->stackSize,old_count)); REQUIRE(Slot_putStack(slot,next));
                    }
                }
                drag->stackSize=remainder; if (drag->stackSize<=0) drag=NULL;
                REQUIRE(InventoryPlayer_setItemStack(p,drag));
            }
            Container_resetDrag(c);
        } else Container_resetDrag(c);
    } else if (c->dragEvent!=0) Container_resetDrag(c);
    else if ((mode==0 || mode==1) && (button==0 || button==1)) {
        if (index==-999) {
            if (InventoryPlayer_getItemStack(p)) {
                if (button==0) { REQUIRE(drop_stack(c,p,InventoryPlayer_getItemStack(p),true)); REQUIRE(InventoryPlayer_setItemStack(p,NULL)); }
                if (button==1) {
                    ItemStack *dropped=ItemStack_splitStack(heap,InventoryPlayer_getItemStack(p),1); ALIVE();
                    REQUIRE(drop_stack(c,p,dropped,true));
                    if (InventoryPlayer_getItemStack(p)->stackSize==0) REQUIRE(InventoryPlayer_setItemStack(p,NULL));
                }
            }
        } else if (mode==1) {
            if (index<0) return NULL;
            Slot *slot=Container_getSlot(c,index); ALIVE();
            if (slot && Slot_canTakeStack(slot,p->player)) {
                ItemStack *moved=Container_transferStackInSlot(c,p,index); ALIVE();
                if (moved) {
                    const Item *item=ItemStack_getItem(moved); returned=ItemStack_copy(heap,moved); ALIVE();
                    if (Slot_getStack(slot) && ItemStack_getItem(Slot_getStack(slot))==item) { Container_retrySlotClick(c,index,button,true,p); ALIVE(); }
                }
            }
        } else {
            if (index<0) return NULL;
            Slot *slot=Container_getSlot(c,index); ALIVE();
            if (slot) {
                ItemStack *current=Slot_getStack(slot),*cursor=InventoryPlayer_getItemStack(p);
                if (current) { returned=ItemStack_copy(heap,current); ALIVE(); }
                if (!current) {
                    if (cursor && Slot_isItemValid(slot,cursor)) {
                        int32_t amount=button==0 ? cursor->stackSize : 1;
                        if (amount>Slot_getItemStackLimit(slot,cursor)) amount=Slot_getItemStackLimit(slot,cursor);
                        if (cursor->stackSize>=amount) {
                            ItemStack *split=ItemStack_splitStack(heap,cursor,amount); ALIVE(); REQUIRE(Slot_putStack(slot,split));
                        }
                        if (cursor->stackSize==0) REQUIRE(InventoryPlayer_setItemStack(p,NULL));
                    }
                } else if (Slot_canTakeStack(slot,p->player)) {
                    if (!cursor) {
                        int32_t amount=button==0 ? current->stackSize : add(current->stackSize,1)/2;
                        ItemStack *picked=Slot_decrStackSize(slot,amount); ALIVE(); REQUIRE(InventoryPlayer_setItemStack(p,picked));
                        if (current->stackSize==0) REQUIRE(Slot_putStack(slot,NULL));
                        REQUIRE(Slot_onPickupFromSlot(slot,p->player,InventoryPlayer_getItemStack(p)));
                    } else if (Slot_isItemValid(slot,cursor)) {
                        if (ItemStack_getItem(current)==ItemStack_getItem(cursor) && ItemStack_getMetadata(current)==ItemStack_getMetadata(cursor) && ItemStack_areItemStackTagsEqual(current,cursor)) {
                            int32_t amount=button==0 ? cursor->stackSize : 1;
                            if (amount>sub(Slot_getItemStackLimit(slot,cursor),current->stackSize)) amount=sub(Slot_getItemStackLimit(slot,cursor),current->stackSize);
                            if (amount>sub(ItemStack_getMaxStackSize(cursor),current->stackSize)) amount=sub(ItemStack_getMaxStackSize(cursor),current->stackSize);
                            (void)ItemStack_splitStack(heap,cursor,amount); ALIVE();
                            if (cursor->stackSize==0) REQUIRE(InventoryPlayer_setItemStack(p,NULL));
                            current->stackSize=add(current->stackSize,amount);
                        } else if (cursor->stackSize<=Slot_getItemStackLimit(slot,cursor)) {
                            REQUIRE(Slot_putStack(slot,cursor)); REQUIRE(InventoryPlayer_setItemStack(p,current));
                        }
                    } else if (ItemStack_getItem(current)==ItemStack_getItem(cursor) && ItemStack_getMaxStackSize(cursor)>1 &&
                        (!ItemStack_getHasSubtypes(current) || ItemStack_getMetadata(current)==ItemStack_getMetadata(cursor)) && ItemStack_areItemStackTagsEqual(current,cursor)) {
                        int32_t amount=current->stackSize;
                        if (amount>0 && add(amount,cursor->stackSize)<=ItemStack_getMaxStackSize(cursor)) {
                            cursor->stackSize=add(cursor->stackSize,amount); current=Slot_decrStackSize(slot,amount); ALIVE();
                            if (!current) { error(heap); return NULL; }
                            if (current->stackSize==0) REQUIRE(Slot_putStack(slot,NULL));
                            REQUIRE(Slot_onPickupFromSlot(slot,p->player,InventoryPlayer_getItemStack(p)));
                        }
                    }
                }
                ALIVE(); Slot_onSlotChanged(slot); ALIVE();
            }
        }
    } else if (mode==2 && button>=0 && button<9) {
        Slot *slot=Container_getSlot(c,index); ALIVE(); if (!slot) { error(heap); return NULL; }
        if (Slot_canTakeStack(slot,p->player)) {
            ItemStack *hotbar=InventoryPlayer_getStackInSlot(p,button);
            bool possible=!hotbar || (slot->inventory.instance==(MCObject *)p && Slot_isItemValid(slot,hotbar)); int32_t empty=-1;
            if (!possible) { empty=InventoryPlayer_getFirstEmptyStack(p); possible|=empty>-1; }
            if (Slot_getHasStack(slot) && possible) {
                ItemStack *current=Slot_getStack(slot),*copy=ItemStack_copy(heap,current); ALIVE();
                REQUIRE(InventoryPlayer_setInventorySlotContents(p,button,copy));
                if ((slot->inventory.instance!=(MCObject *)p || !Slot_isItemValid(slot,hotbar)) && hotbar) {
                    if (empty>-1) {
                        (void)InventoryPlayer_addItemStackToInventory(p,hotbar); ALIVE();
                        (void)Slot_decrStackSize(slot,current->stackSize); ALIVE(); REQUIRE(Slot_putStack(slot,NULL));
                        REQUIRE(Slot_onPickupFromSlot(slot,p->player,current));
                    }
                } else {
                    (void)Slot_decrStackSize(slot,current->stackSize); ALIVE(); REQUIRE(Slot_putStack(slot,hotbar));
                    REQUIRE(Slot_onPickupFromSlot(slot,p->player,current));
                }
            } else if (!Slot_getHasStack(slot) && hotbar && Slot_isItemValid(slot,hotbar)) {
                REQUIRE(InventoryPlayer_setInventorySlotContents(p,button,NULL)); REQUIRE(Slot_putStack(slot,hotbar));
            }
        }
    } else if (mode==3) {
        if (!p->isCreativeMode) { error(heap); return NULL; }
        if (p->isCreativeMode(p->player) && !InventoryPlayer_getItemStack(p) && index>=0) {
            Slot *slot=Container_getSlot(c,index); ALIVE();
            if (slot && Slot_getHasStack(slot)) {
                ItemStack *copy=ItemStack_copy(heap,Slot_getStack(slot)); ALIVE(); if (!copy) { error(heap); return NULL; }
                copy->stackSize=ItemStack_getMaxStackSize(copy); REQUIRE(InventoryPlayer_setItemStack(p,copy));
            }
        }
    } else if (mode==4 && !InventoryPlayer_getItemStack(p) && index>=0) {
        Slot *slot=Container_getSlot(c,index); ALIVE();
        if (slot && Slot_getHasStack(slot) && Slot_canTakeStack(slot,p->player)) {
            ItemStack *dropped=Slot_decrStackSize(slot,button==0 ? 1 : Slot_getStack(slot)->stackSize); ALIVE();
            REQUIRE(Slot_onPickupFromSlot(slot,p->player,dropped)); REQUIRE(drop_stack(c,p,dropped,true));
        }
    } else if (mode==6 && index>=0) {
        Slot *clicked=Container_getSlot(c,index); ItemStack *cursor=InventoryPlayer_getItemStack(p); ALIVE();
        if (cursor && (!clicked || !Slot_getHasStack(clicked) || !Slot_canTakeStack(clicked,p->player))) {
            int32_t start=button==0 ? 0 : sub(c->inventorySlots->size,1),step=button==0 ? 1 : -1;
            for (int pass=0;pass<2;pass++) for (int32_t i=start;i>=0 && i<c->inventorySlots->size && cursor->stackSize<ItemStack_getMaxStackSize(cursor);i=add(i,step)) {
                Slot *slot=Container_getSlot(c,i);
                if (Slot_getHasStack(slot) && Container_canAddItemToSlot(slot,cursor,true) && Slot_canTakeStack(slot,p->player) && Container_canMergeSlot(c,cursor,slot) &&
                    (pass!=0 || Slot_getStack(slot)->stackSize!=ItemStack_getMaxStackSize(Slot_getStack(slot)))) {
                    int32_t amount=sub(ItemStack_getMaxStackSize(cursor),cursor->stackSize),available=Slot_getStack(slot)->stackSize;
                    if (available<amount) amount=available;
                    ItemStack *picked=Slot_decrStackSize(slot,amount); ALIVE(); if (!picked) { error(heap); return NULL; }
                    cursor->stackSize=add(cursor->stackSize,amount);
                    if (picked->stackSize<=0) REQUIRE(Slot_putStack(slot,NULL));
                    REQUIRE(Slot_onPickupFromSlot(slot,p->player,picked));
                }
                ALIVE();
            }
        }
        REQUIRE(Container_detectAndSendChanges(c));
    }
    ALIVE(); MCObjectHeap_touch(heap);
#undef ALIVE
#undef REQUIRE
    return returned;
}
ItemStack *Container_slotClick(Container *c,int32_t index,int32_t button,int32_t mode,InventoryPlayer *player) {
    if (!c) return NULL;
    if (!player || !same_heap(c->object.heap,(MCObject *)player) || MCObjectHeap_failed(c->object.heap) || c->nativeClickDepth>=512) { error(c->object.heap); return NULL; }
    /* Java recursive shift retry can throw StackOverflowError for a subclass
       that never advances. Bound the native call stack and fail the operation. */
    ++c->nativeClickDepth; ItemStack *returned=slot_click(c,index,button,mode,player); --c->nativeClickDepth;
    return returned;
}
