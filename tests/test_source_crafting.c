#include "inventory/ContainerPlayer.h"
#include "inventory/ContainerWorkbench.h"
#include "item/crafting/RecipeBookCloning.h"
#include "nbt/NBTTagCompound.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"source crafting check %u at %d: %s\n",checks,__LINE__,#x); exit(1); } } while (0)
typedef struct { MCObject object; bool remote,table; unsigned crafted; int32_t amount; ItemStack *last; } TestWorld;
typedef struct {
    MCObject object; InventoryPlayer *inventory; TestWorld *world; ItemStackArray *drops;
    unsigned drop_count,achievement_count; bool creative; double x,y,z;
} TestPlayer;
static void world_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    TestWorld *world=(TestWorld *)object; world->last=(ItemStack *)visitor((MCObject *)world->last,context);
}
static void player_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    TestPlayer *player=(TestPlayer *)object;
    player->inventory=(InventoryPlayer *)visitor((MCObject *)player->inventory,context);
    player->world=(TestWorld *)visitor((MCObject *)player->world,context);
    player->drops=(ItemStackArray *)visitor((MCObject *)player->drops,context);
}
static const MCObjectClass world_class={"fixture.World",MCObjectHeap_plainClone,world_trace,NULL};
static const MCObjectClass player_class={"fixture.Player",MCObjectHeap_plainClone,player_trace,NULL};
static bool creative(const MCObject *player) { return ((const TestPlayer *)player)->creative; }
static InventoryPlayer *inventory(MCObject *player) { return ((TestPlayer *)player)->inventory; }
static MCObject *world(MCObject *player) { return (MCObject *)((TestPlayer *)player)->world; }
static bool drop(MCObject *object,ItemStack *stack,bool scatter) {
    TestPlayer *player=(TestPlayer *)object; (void)scatter;
    if (player->drop_count>=32) return false;
    player->drops->items[player->drop_count++]=stack; MCObjectHeap_touch(object->heap); return true;
}
static bool crafted(ItemStack *stack,MCObject *object,MCObject *player,int32_t amount) {
    TestWorld *w=(TestWorld *)object; (void)player;
    ++w->crafted; w->amount=amount; w->last=stack; MCObjectHeap_touch(object->heap); return true;
}
static bool achievement(MCObject *object,mc_crafting_achievement value) {
    TestPlayer *p=(TestPlayer *)object; (void)value; ++p->achievement_count; MCObjectHeap_touch(object->heap); return true;
}
static int id(const Item *item) { return ItemStack_registryId(item); }
static bool pickaxe(const Item *item) { int n=id(item); return n==257 || n==270 || n==274 || n==278 || n==285; }
static bool hoe(const Item *item) { return id(item)>=290 && id(item)<=294; }
static bool sword(const Item *item) { int n=id(item); return n==267 || n==268 || n==272 || n==276 || n==283; }
static bool wood_pickaxe(const Item *item) { return id(item)==270; }
static int32_t armor(const Item *item) { int n=id(item); return n>=298 && n<=317 ? (n-298)%4 : -1; }
static bool remote(const MCObject *object) { return ((const TestWorld *)object)->remote; }
static bool table(const MCObject *object,int32_t x,int32_t y,int32_t z) { (void)x;(void)y;(void)z;return ((const TestWorld *)object)->table; }
static double distance(const MCObject *object,double x,double y,double z) {
    const TestPlayer *p=(const TestPlayer *)object; x-=p->x;y-=p->y;z-=p->z;return x*x+y*y+z*z;
}
static NBTString *display(MCObject *context,const ItemStack *stack) { (void)context; return NBTString_fromASCII(stack->object.heap,"Written Book"); }
/* Fixture exposes the real translated book recipe. Its world contains no
   other recipes; tests exercise the production class dispatch, not a copied
   click or remainder algorithm. */
static ItemStack *recipe(InventoryCrafting *matrix,MCObject *object) {
    return RecipeBookCloning_matches(matrix,object) ? RecipeBookCloning_getCraftingResult(matrix,display,object) : NULL;
}
static ItemStackArray *remainders(InventoryCrafting *matrix,MCObject *object) {
    CHECK(RecipeBookCloning_matches(matrix,object)); return RecipeBookCloning_getRemainingItems(matrix);
}
static const mc_crafting_dispatch dependencies={
    inventory,world,recipe,remainders,crafted,achievement,drop,pickaxe,hoe,sword,wood_pickaxe,
    armor,remote,table,distance
};
static TestPlayer *player_new(MCObjectHeap *heap,TestWorld *w,bool is_creative) {
    TestPlayer *p=(TestPlayer *)MCObjectHeap_alloc(heap,sizeof(*p),&player_class); CHECK(p);
    p->world=w;p->creative=is_creative;p->drops=ItemStackArray_new(heap,32);CHECK(p->drops);
    p->inventory=InventoryPlayer_new(heap,(MCObject *)p,creative);CHECK(p->inventory);return p;
}
static TestWorld *world_new(MCObjectHeap *heap) {
    TestWorld *w=(TestWorld *)MCObjectHeap_alloc(heap,sizeof(*w),&world_class);CHECK(w);w->table=true;return w;
}
static ItemStack *item(MCObjectHeap *heap,int n,int32_t count,int32_t damage) {
    ItemStack *s=ItemStack_new(heap,ItemStack_registryItem(n),count,damage);CHECK(s);return s;
}
static ItemStack *book(MCObjectHeap *heap,int32_t count,int32_t damage) {
    ItemStack *s=item(heap,387,count,damage);NBTTagCompound *tag=NBTTagCompound_new(heap);CHECK(tag);
    CHECK(NBTTagCompound_setInteger_ascii(tag,"generation",0));CHECK(ItemStack_setTagCompound(s,tag));return s;
}
static void book_cases(void) {
    /* Source goldens from the actual game: amount1/2/18, free0/1/36,
       survival/creative, source metadata0/7; run both original containers. */
    const int amounts[]={1,2,18}, spaces[]={0,1,36};
    for (int bench=0;bench<2;bench++) for (int meta=0;meta<=7;meta+=7)
    for (int c=0;c<2;c++) for (unsigned n=0;n<3;n++) for (unsigned e=0;e<3;e++) {
        MCObjectHeap *heap=MCObjectHeap_new(16*1024*1024);CHECK(heap);
        MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,heap));
        TestWorld *w=world_new(heap);TestPlayer *p=player_new(heap,w,c!=0);
        for (int i=spaces[e];i<36;i++) CHECK(InventoryPlayer_setInventorySlotContents(p->inventory,i,item(heap,1,64,0)));
        Container *container;InventoryCrafting *grid;InventoryCraftResult *result;
        if (bench) {
            mc_crafting_position position={0};ContainerWorkbench *b=ContainerWorkbench_new(p->inventory,(MCObject *)w,&position,&dependencies);CHECK(b);
            container=&b->container;grid=b->craftMatrix;result=b->craftResult;
        } else {
            ContainerPlayer *b=ContainerPlayer_new(p->inventory,false,(MCObject *)p,&dependencies);CHECK(b);
            container=&b->container;grid=b->craftMatrix;result=b->craftResult;
        }
        CHECK(ContainerList_size(container->inventorySlots)==(bench?46:45));
        ItemStack *source=book(heap,amounts[n],meta);
        CHECK(InventoryCrafting_setInventorySlotContents(grid,1,source));
        CHECK(InventoryCrafting_setInventorySlotContents(grid,0,item(heap,386,1,0)));
        ItemStack *output=InventoryCraftResult_getStackInSlot(result,0);CHECK(output&&output->stackSize==1&&output->itemDamage==0);
        CHECK(Container_slotClick(container,0,0,0,p->inventory));CHECK(!MCObjectHeap_failed(heap));
        CHECK(InventoryPlayer_getItemStack(p->inventory)==output);
        int expected=amounts[n]==1?1:(c?0:(spaces[e]==0?amounts[n]-1:(spaces[e]==1&&amounts[n]==18?1:0)));
        if (InventoryCrafting_getStackInSlot(grid,1)!=source || source->stackSize!=expected)
            fprintf(stderr,"book bench=%d meta=%d creative=%d amount=%d free=%d actual=%d expected=%d same=%d\n",bench,meta,c,amounts[n],spaces[e],source->stackSize,expected,InventoryCrafting_getStackInSlot(grid,1)==source);
        CHECK(InventoryCrafting_getStackInSlot(grid,1)==source&&source->stackSize==expected);
        int written=0;for (int i=0;i<36;i++) {ItemStack *s=InventoryPlayer_getStackInSlot(p->inventory,i);if(s&&id(s->item)==387)written+=s->stackSize;}
        CHECK(written==(amounts[n]==1?0:(spaces[e]==0?0:(spaces[e]==1&&amounts[n]==18?16:amounts[n]-1))));
        bool dropped=!c&&amounts[n]>1&&(spaces[e]==0||(spaces[e]==1&&amounts[n]==18));
        CHECK(p->drop_count==(dropped?1u:0u));CHECK(!dropped||p->drops->items[0]==source);
        CHECK(w->crafted==1&&w->amount==1&&w->last==output);CHECK(p->achievement_count==0);
        MCObjectRootScope_end(&scope);MCObjectHeap_free(heap);
    }
}
static void cross_player_snapshot(void) {
    MCObjectHeap *heap=MCObjectHeap_new(16*1024*1024);CHECK(heap);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,heap));
    TestWorld *w=world_new(heap);TestPlayer *a=player_new(heap,w,false),*b=player_new(heap,w,false);
    ContainerPlayer *container=ContainerPlayer_new(a->inventory,false,(MCObject *)a,&dependencies);CHECK(container);
    for (int i=0;i<36;i++) CHECK(InventoryPlayer_setInventorySlotContents(a->inventory,i,item(heap,1,64,0)));
    ItemStack *source=book(heap,2,7);CHECK(InventoryCrafting_setInventorySlotContents(container->craftMatrix,1,source));
    CHECK(InventoryCrafting_setInventorySlotContents(container->craftMatrix,0,item(heap,386,1,0)));
    CHECK(Container_slotClick(&container->container,0,0,0,a->inventory));CHECK(a->drop_count==1&&a->drops->items[0]==source);
    MCObjectRoot ra={0},rb={0},rc={0};CHECK(MCObjectRoot_init(&ra,heap,(MCObject *)a));CHECK(MCObjectRoot_init(&rb,heap,(MCObject *)b));CHECK(MCObjectRoot_init(&rc,heap,(MCObject *)container));
    MCObjectRootScope_end(&scope);MCObjectHeap *working=MCObjectHeap_clone(heap);CHECK(working);
    MCObjectRoot wa={0},wb={0},wc={0};CHECK(MCObjectRoot_rebind(&wa,working,&ra));CHECK(MCObjectRoot_rebind(&wb,working,&rb));CHECK(MCObjectRoot_rebind(&wc,working,&rc));
    a=(TestPlayer *)MCObjectRoot_get(&wa);b=(TestPlayer *)MCObjectRoot_get(&wb);container=(ContainerPlayer *)MCObjectRoot_get(&wc);
    CHECK(MCObjectRootScope_begin(&scope,working));ItemStack *drop_ref=a->drops->items[0];CHECK(drop_ref==InventoryCrafting_getStackInSlot(container->craftMatrix,1));
    CHECK(InventoryPlayer_addItemStackToInventory(b->inventory,drop_ref));CHECK(drop_ref->stackSize==0);
    CHECK(InventoryCrafting_getStackInSlot(container->craftMatrix,1)==drop_ref);
    CHECK(InventoryPlayer_getStackInSlot(b->inventory,0)!=drop_ref&&InventoryPlayer_getStackInSlot(b->inventory,0)->stackSize==1);
    CHECK(((TestPlayer *)MCObjectRoot_get(&ra))->drops->items[0]->stackSize==1);
    MCObjectRootScope_end(&scope);CHECK(MCObjectHeap_adopt(heap,working));MCObjectHeap_free(working);
    CHECK(((TestPlayer *)MCObjectRoot_get(&ra))->drops->items[0]->stackSize==0);CHECK(MCObjectHeap_collect(heap));
    MCObjectHeap_free(heap);
}
static void layout_and_close(void) {
    MCObjectHeap *heap=MCObjectHeap_new(16*1024*1024);CHECK(heap);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,heap));
    TestWorld *w=world_new(heap);TestPlayer *p=player_new(heap,w,false);ContainerPlayer *pc=ContainerPlayer_new(p->inventory,false,(MCObject *)p,&dependencies);CHECK(pc);
    Slot *helmet=Container_getSlot(&pc->container,5);CHECK(helmet->slotIndex==39&&helmet->xDisplayPosition==8&&helmet->yDisplayPosition==8);
    CHECK(Slot_getSlotStackLimit(helmet)==1&&Slot_isItemValid(helmet,item(heap,298,1,0)));CHECK(!Slot_isItemValid(helmet,item(heap,299,1,0)));
    CHECK(Slot_isItemValid(helmet,item(heap,86,1,0)));CHECK(Slot_getSlotTexture(helmet));
    ItemStack *input=item(heap,1,2,0),*cursor=item(heap,2,1,0);CHECK(InventoryCrafting_setInventorySlotContents(pc->craftMatrix,0,input));CHECK(InventoryPlayer_setItemStack(p->inventory,cursor));
    CHECK(Container_onContainerClosed(&pc->container,p->inventory));CHECK(p->drop_count==2&&p->drops->items[0]==cursor&&p->drops->items[1]==input);
    CHECK(!InventoryCrafting_getStackInSlot(pc->craftMatrix,0)&&!InventoryCraftResult_getStackInSlot(pc->craftResult,0));
    mc_crafting_position position={0};ContainerWorkbench *bc=ContainerWorkbench_new(p->inventory,(MCObject *)w,&position,&dependencies);CHECK(bc);
    CHECK(ContainerWorkbench_canInteractWith(bc,p->inventory));p->x=8.5;p->y=0.5;p->z=0.5;CHECK(ContainerWorkbench_canInteractWith(bc,p->inventory));p->x=8.5001;CHECK(!ContainerWorkbench_canInteractWith(bc,p->inventory));
    w->table=false;p->x=0;CHECK(!ContainerWorkbench_canInteractWith(bc,p->inventory));w->remote=true;
    CHECK(InventoryCrafting_setInventorySlotContents(bc->craftMatrix,8,input));CHECK(Container_onContainerClosed(&bc->container,p->inventory));CHECK(InventoryCrafting_getStackInSlot(bc->craftMatrix,8)==input);
    w->remote=false;CHECK(Container_onContainerClosed(&bc->container,p->inventory));CHECK(!InventoryCrafting_getStackInSlot(bc->craftMatrix,8));CHECK(p->drops->items[p->drop_count-1]==input);
    CHECK(!MCObjectHeap_failed(heap));MCObjectRootScope_end(&scope);MCObjectHeap_free(heap);
}
/* Adapter failure stands for a Java exception. These deliberately failing
   fixture callbacks do not mark the heap themselves: Slot must propagate it. */
static bool reject_set(MCObject *instance,int32_t index,ItemStack *stack) {
    (void)instance;(void)index;(void)stack;return false;
}
static bool reject_pickup(Slot *slot,MCObject *player,ItemStack *stack) {
    (void)slot;(void)player;(void)stack;return false;
}
static void callback_failure(void) {
    for (int bench=0;bench<2;bench++) for (int pickup=0;pickup<2;pickup++) {
        MCObjectHeap *heap=MCObjectHeap_new(16*1024*1024);CHECK(heap);
        MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,heap));
        TestWorld *w=world_new(heap);TestPlayer *p=player_new(heap,w,false);
        Container *container;
        if (bench) {
            mc_crafting_position position={0};
            ContainerWorkbench *b=ContainerWorkbench_new(p->inventory,(MCObject *)w,&position,&dependencies);CHECK(b);
            container=&b->container;
        } else {
            ContainerPlayer *b=ContainerPlayer_new(p->inventory,false,(MCObject *)p,&dependencies);CHECK(b);
            container=&b->container;
        }
        ItemStack *source=item(heap,1,1,0);CHECK(InventoryPlayer_setInventorySlotContents(p->inventory,9,source));
        int index=bench?10:9;Slot *slot=Container_getSlot(container,index);CHECK(slot);
        if (pickup) {
            const SlotOverrides failing={.onPickupFromSlot=reject_pickup};slot->overrides=&failing;
            CHECK(!Slot_onPickupFromSlot(slot,(MCObject *)p,source));CHECK(MCObjectHeap_failed(heap));
            CHECK(InventoryPlayer_getStackInSlot(p->inventory,9)==source&&source->stackSize==1);
        } else {
            IInventoryMethods failing=*slot->inventory.methods;
            failing.setInventorySlotContents=reject_set;slot->inventory.methods=&failing;
            CHECK(!Container_transferStackInSlot(container,p->inventory,index));CHECK(MCObjectHeap_failed(heap));
            /* Merge happened before source clearing failed. The failed graph
               must be discarded instead of committing it as an ordinary NULL. */
            CHECK(source->stackSize==0&&Slot_getStack(slot)==source);
            ItemStack *moved=InventoryPlayer_getStackInSlot(p->inventory,0);CHECK(moved&&moved->stackSize==1);
        }
        MCObjectRootScope_end(&scope);MCObjectHeap_free(heap);
    }
}
int main(void) {book_cases();cross_player_snapshot();layout_and_close();callback_failure();printf("source crafting containers: %u checks passed\n",checks);return 0;}
