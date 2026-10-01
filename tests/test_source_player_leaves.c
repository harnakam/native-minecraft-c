#include "util/FoodStats.h"
#include "inventory/InventoryEnderChest.h"
#include "nbt/NBTTagCompound.h"
#include "nbt/NBTTagList.h"
#include "nbt/NBTTagInt.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)){fprintf(stderr,"player leaves check %u line %d: %s\n",checks,__LINE__,#x);exit(1);} } while(0)
static uint32_t bits(float f){uint32_t b;memcpy(&b,&f,4);return b;}
typedef struct Fixture {
    MCObject object;FoodStats *food;InventoryBasic *inventory;MCObject *extra;
    int32_t difficulty;float health;bool natural,shouldHeal,attackAccepted;
    char events[256];unsigned used;char failAt;int action;int calls;
} Fixture;
static void trace(MCObject *o,MCObjectVisitor v,void *c){Fixture *f=(Fixture *)o;f->food=(FoodStats *)v((MCObject *)f->food,c);f->inventory=(InventoryBasic *)v((MCObject *)f->inventory,c);f->extra=v(f->extra,c);}
static const MCObjectClass fixture_class={"test.playerLeaves",MCObjectHeap_plainClone,trace,NULL};
static Fixture *fixture(MCObjectHeap *h){Fixture *f=(Fixture *)MCObjectHeap_alloc(h,sizeof *f,&fixture_class);CHECK(f);f->natural=true;f->shouldHeal=true;f->health=2;f->difficulty=2;return f;}
static bool event(Fixture *f,char c){CHECK(f->used+1<sizeof f->events);f->events[f->used++]=c;f->events[f->used]=0;return f->failAt!=c;}
static MCObject *world(MCObject *ctx,MCObject *p){CHECK(ctx==p);return event((Fixture *)ctx,'W')?p:NULL;}
static bool difficulty(MCObject *ctx,MCObject *w,int32_t *out){CHECK(ctx==w);*out=((Fixture *)ctx)->difficulty;return event((Fixture *)ctx,'D');}
static MCObject *rules(MCObject *ctx,MCObject *w){CHECK(ctx==w);return event((Fixture *)ctx,'R')?ctx:NULL;}
static bool rule(MCObject *ctx,MCObject *r,const char *key,bool *out){CHECK(ctx==r&&strcmp(key,"naturalRegeneration")==0);*out=((Fixture *)ctx)->natural;return event((Fixture *)ctx,'B');}
static bool should(MCObject *ctx,MCObject *p,bool *out){CHECK(ctx==p);*out=((Fixture *)ctx)->shouldHeal;if(((Fixture *)ctx)->action==99)FoodStats_setFoodLevel(((Fixture *)ctx)->food,0);return event((Fixture *)ctx,'S');}
static bool heal(MCObject *ctx,MCObject *p,float amount){CHECK(ctx==p&&amount==1.0f);Fixture *f=(Fixture *)ctx;f->health+=amount;return event(f,'H');}
static bool health(MCObject *ctx,MCObject *p,float *out){CHECK(ctx==p);*out=((Fixture *)ctx)->health;return event((Fixture *)ctx,'G');}
static bool attack(MCObject *ctx,MCObject *p,float amount,bool *accepted){CHECK(ctx==p&&amount==1.0f);*accepted=((Fixture *)ctx)->attackAccepted;return event((Fixture *)ctx,'A');}
static const FoodStatsPlayerDependencies player_deps={world,difficulty,rules,rule,should,heal,health,attack};
static bool amount(MCObject *ctx,const Item *item,ItemStack *stack,int32_t *out){CHECK(item==stack->item);*out=3;return event((Fixture *)ctx,'I');}
static bool saturation(MCObject *ctx,const Item *item,ItemStack *stack,float *out){CHECK(item==stack->item);*out=.25f;return event((Fixture *)ctx,'F');}
static const FoodStatsItemDependencies item_deps={amount,saturation};
static bool changed(MCObject *ctx,MCObject *l,InventoryBasic *inv){
    Fixture *f=(Fixture *)ctx,*listener=(Fixture *)l;CHECK(listener->inventory==inv);CHECK(event(f,(char)listener->action));++listener->calls;
    if(listener->action=='1'&&f->action==1)CHECK(InventoryBasic_removeInventoryChangeListener(inv,l));
    if(listener->action=='1'&&f->action==2){f->action=0;CHECK(InventoryBasic_addInventoryChangeListener(inv,f->extra));}
    if(ctx->heap==inv->object.heap)CHECK(!MCObjectHeap_collect(ctx->heap));
    return true;
}
static bool equals(MCObject *ctx,MCObject *a,MCObject *b,bool *out){(void)ctx;*out=a==b;return true;}
static bool semantic_equals(MCObject *ctx,MCObject *a,MCObject *b,bool *out){(void)ctx;*out=b&&((Fixture *)a)->action==((Fixture *)b)->action;return true;}
static NBTString *unformatted(MCObject *ctx,MCObject *chat){CHECK(event((Fixture *)ctx,'U'));return (NBTString *)chat;}
static MCObject *text_chat(MCObject *ctx,NBTString *s){CHECK(event((Fixture *)ctx,'T'));return (MCObject *)s;}
static MCObject *translation(MCObject *ctx,NBTString *s){CHECK(event((Fixture *)ctx,'L'));return (MCObject *)s;}
static const InventoryBasicDependencies basic_deps={changed,equals,unformatted,text_chat,translation};
static bool can_use(MCObject *ctx,MCObject *chest,MCObject *p,bool *out){CHECK(ctx==chest);CHECK(p);*out=((Fixture *)ctx)->shouldHeal;return event((Fixture *)ctx,'C');}
static bool open_chest(MCObject *ctx,MCObject *chest){CHECK(ctx==chest);return event((Fixture *)ctx,'O');}
static bool close_chest(MCObject *ctx,MCObject *chest){CHECK(ctx==chest);return event((Fixture *)ctx,'X');}
static const InventoryEnderChestDependencies chest_deps={can_use,open_chest,close_chest};
static void defaults(void){
    IInventory missing=InventoryEnderChest_asIInventory(NULL);CHECK(missing.instance==NULL);
    MCObjectHeap *heap=MCObjectHeap_new(4*1024*1024);CHECK(heap);
    {
        FoodStats *food=FoodStats_new(heap);CHECK(food);
        CHECK(food->foodLevel==20&&food->prevFoodLevel==20&&food->foodSaturationLevel==5.0f);
    }
    {
        NBTString *title=NBTString_fromASCII(heap,"same-title");CHECK(title);
        InventoryBasic *basic=InventoryBasic_new(heap,title,true,3,NULL,NULL);CHECK(basic);
        CHECK(basic->inventoryTitle==title&&basic->slotsCount==3&&basic->inventoryContents&&basic->inventoryContents->length==3);
    }
    {
        InventoryEnderChest *ender=InventoryEnderChest_new(heap,NULL,NULL,NULL);CHECK(ender);
        CHECK(ender->basic.slotsCount==27&&ender->basic.inventoryContents&&ender->basic.inventoryContents->length==27);
    }
    MCObjectHeap_free(heap);
}
/* Wrong saturation multiplication order, integer clamp before wrapping, or
   normalizing NaN/signed-zero changes source food/NBT behavior. */
static void food_values(void){
    MCObjectHeap *h=MCObjectHeap_new(4*1024*1024);CHECK(h);FoodStats *s=FoodStats_new(h);CHECK(s);
    CHECK(FoodStats_addStats(s,-25,.25f));CHECK(FoodStats_getFoodLevel(s)==-5&&FoodStats_getSaturationLevel(s)==-7.5f&&FoodStats_needFood(s));
    FoodStats_setFoodLevel(s,INT32_MAX);CHECK(FoodStats_addStats(s,1,0));CHECK(s->foodLevel==INT32_MIN);
    FoodStats_setFoodLevel(s,20);FoodStats_setFoodSaturationLevel(s,-0.0f);CHECK(bits(s->foodSaturationLevel)==UINT32_C(0x80000000));
    CHECK(FoodStats_addExhaustion(s,100));CHECK(s->foodExhaustionLevel==40);CHECK(FoodStats_addExhaustion(s,-50));CHECK(s->foodExhaustionLevel==-10);
    CHECK(FoodStats_addExhaustion(s,NAN));CHECK(isnan(s->foodExhaustionLevel));
    NBTTagCompound *n=NBTTagCompound_new(h);CHECK(n);CHECK(FoodStats_writeNBT(s,n));CHECK(NBTTagCompound_getTagId_ascii(n,"foodLevel")==3&&NBTTagCompound_getTagId_ascii(n,"foodTickTimer")==3&&NBTTagCompound_getTagId_ascii(n,"foodSaturationLevel")==5);
    s->foodLevel=7;s->foodTimer=11;s->foodSaturationLevel=2;s->foodExhaustionLevel=3;
    NBTTagCompound *missing=NBTTagCompound_new(h);CHECK(missing);CHECK(FoodStats_readNBT(s,missing));CHECK(s->foodLevel==7&&s->foodTimer==11&&s->foodSaturationLevel==2);
    CHECK(NBTTagCompound_setFloat_ascii(missing,"foodLevel",-1.5f));CHECK(FoodStats_readNBT(s,missing));CHECK(s->foodLevel==-2&&s->foodTimer==0&&s->foodSaturationLevel==0&&s->prevFoodLevel==20);
    Fixture *f=fixture(h);ItemStack *item=ItemStack_new(h,ItemStack_registryItem(260),1,0);CHECK(item);CHECK(FoodStats_addStats_item(s,item->item,item,&item_deps,(MCObject *)f));CHECK(strcmp(f->events,"IF")==0&&s->foodLevel==1&&s->foodSaturationLevel==1);
    MCObjectHeap_free(h);
}
/* Health is intentionally called twice in the NORMAL starvation branch;
   attack false is an ordinary result, while callback failure keeps timer80. */
static void food_update(void){
    for(int c=0;c<10;c++){
        MCObjectHeap *h=MCObjectHeap_new(4*1024*1024);CHECK(h);FoodStats *s=FoodStats_new(h);CHECK(s);Fixture *f=fixture(h);f->food=s;s->foodTimer=79;
        const char *want="WDWRBSH";
        if(c==1){s->foodLevel=0;s->foodSaturationLevel=0;want="WDWRBGGA";}
        if(c==2){s->foodLevel=0;f->difficulty=3;f->health=0;want="WDWRBGA";}
        if(c==3){s->foodLevel=0;f->difficulty=1;want="WDWRBGG";}
        if(c==4){s->foodLevel=17;want="WDWRB";}
        if(c==5){f->natural=false;want="WDWRB";}
        if(c==6){s->foodLevel=0;s->foodTimer=INT32_MAX;want="WDWRB";}
        if(c==7){s->foodExhaustionLevel=5;s->foodSaturationLevel=.25f;}
        if(c==8){f->failAt='H';}
        if(c==9){f->action=99;}
        bool ok=FoodStats_onUpdate(s,(MCObject *)f,&player_deps,(MCObject *)f);
        CHECK(ok==(c!=8));CHECK(strcmp(f->events,want)==0);CHECK(s->prevFoodLevel==(c==1||c==2||c==3||c==6?0:c==4?17:20));
        CHECK(s->foodTimer==(c==6?INT32_MIN:c==8?80:0));
        if(c==0)CHECK(s->foodExhaustionLevel==3&&f->health==3);
        if(c==7)CHECK(s->foodExhaustionLevel==4&&s->foodSaturationLevel==0);
        if(c==8)CHECK(MCObjectHeap_failed(h)&&s->foodExhaustionLevel==0);
        if(c==9)CHECK(s->foodLevel==0&&s->foodExhaustionLevel==3&&f->health==3);
        MCObjectHeap_free(h);
    }
}
/* Insertion copies the incoming stack, ignores tag differences, notifies
   twice on the empty path, and clear/remove deliberately do not notify. */
static void basic_values_and_listeners(void){
    MCObjectHeap *h=MCObjectHeap_new(8*1024*1024);CHECK(h);Fixture *f=fixture(h);NBTString *title=NBTString_fromASCII(h,"inventory");CHECK(title);
    InventoryBasic *b=InventoryBasic_new(h,title,false,3,&basic_deps,(MCObject *)f);CHECK(b);f->inventory=b;
    Fixture *a=fixture(h),*z=fixture(h),*last=fixture(h);a->inventory=z->inventory=last->inventory=b;a->action='1';z->action='2';last->action='3';
    CHECK(InventoryBasic_addInventoryChangeListener(b,(MCObject *)a));CHECK(InventoryBasic_addInventoryChangeListener(b,(MCObject *)z));CHECK(InventoryBasic_addInventoryChangeListener(b,(MCObject *)last));
    f->action=1;CHECK(InventoryBasic_markDirty(b));CHECK(strcmp(f->events,"13")==0&&z->calls==0);f->action=0;f->used=0;f->events[0]=0;
    CHECK(InventoryBasic_removeInventoryChangeListener(b,(MCObject *)z));CHECK(InventoryBasic_removeInventoryChangeListener(b,(MCObject *)last));
    CHECK(InventoryBasic_addInventoryChangeListener(b,(MCObject *)a));CHECK(InventoryBasic_addInventoryChangeListener(b,(MCObject *)z));f->extra=(MCObject *)last;f->action=2;
    CHECK(InventoryBasic_markDirty(b));CHECK(strcmp(f->events,"123")==0);f->action=0;f->used=0;f->events[0]=0;
    ItemStack *in=ItemStack_new(h,ItemStack_registryItem(1),100,0);CHECK(in);CHECK(InventoryBasic_func_174894_a(b,in)==NULL);CHECK(in->stackSize==100&&b->inventoryContents->items[0]!=in&&b->inventoryContents->items[0]->stackSize==64);CHECK(strcmp(f->events,"123123")==0);
    f->used=0;f->events[0]=0;ItemStack *removed=InventoryBasic_decrStackSize(b,0,-2);CHECK(removed&&removed->stackSize==-2&&b->inventoryContents->items[0]->stackSize==66);CHECK(strcmp(f->events,"123")==0);
    f->used=0;f->events[0]=0;CHECK(InventoryBasic_removeStackFromSlot(b,0)->stackSize==66);InventoryBasic_clear(b);CHECK(f->used==0);
    CHECK(InventoryBasic_getStackInSlot(b,-1)==NULL&&InventoryBasic_getStackInSlot(b,3)==NULL&&!MCObjectHeap_failed(h));
    CHECK(InventoryBasic_getDisplayName(b)==(MCObject *)title&&strcmp(f->events,"L")==0);CHECK(InventoryBasic_setCustomName(b,title));CHECK(InventoryBasic_getDisplayName(b)==(MCObject *)title&&strcmp(f->events,"LT")==0);
    InventoryBasic *chat=InventoryBasic_new_chat(h,(MCObject *)title,2,&basic_deps,(MCObject *)f);CHECK(chat&&chat->hasCustomName&&chat->inventoryTitle==title);
    IInventory iface=InventoryBasic_asIInventory(b);CHECK(iface.instance==(MCObject *)b&&iface.methods->getSizeInventory(iface.instance)==3&&iface.methods->getInventoryStackLimit(iface.instance)==64);
    CHECK(InventoryBasic_isUseableByPlayer(b,NULL)&&InventoryBasic_isItemValidForSlot(b,-100,NULL));CHECK(InventoryBasic_getField(b,123)==0&&InventoryBasic_getFieldCount(b)==0);InventoryBasic_setField(b,1,2);InventoryBasic_openInventory(b,NULL);InventoryBasic_closeInventory(b,NULL);
    CHECK(InventoryBasic_addInventoryChangeListener(b,NULL));CHECK(!InventoryBasic_markDirty(b)&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
static void ender_nbt_chest(void){
    MCObjectHeap *h=MCObjectHeap_new(8*1024*1024);CHECK(h);Fixture *f=fixture(h);InventoryEnderChest *e=InventoryEnderChest_new(h,&basic_deps,&chest_deps,(MCObject *)f);CHECK(e);f->inventory=&e->basic;
    CHECK(NBTString_equalsASCII(InventoryBasic_getName(&e->basic),"container.enderchest")&&!e->basic.hasCustomName);
    CHECK(InventoryEnderChest_isUseableByPlayer(e,NULL));CHECK(InventoryEnderChest_setChestTileEntity(e,(MCObject *)f));CHECK(InventoryEnderChest_isUseableByPlayer(e,(MCObject *)f));f->shouldHeal=false;CHECK(!InventoryEnderChest_isUseableByPlayer(e,(MCObject *)f));
    CHECK(InventoryEnderChest_openInventory(e,(MCObject *)f));CHECK(InventoryEnderChest_closeInventory(e,(MCObject *)f));CHECK(e->associatedChest==NULL&&strcmp(f->events,"CCOX")==0);
    ItemStack *shared=ItemStack_new(h,ItemStack_registryItem(1),0,0);CHECK(shared);NBTTagCompound *tag=NBTTagCompound_new(h);CHECK(tag);CHECK(ItemStack_setTagCompound(shared,tag));CHECK(InventoryBasic_setInventorySlotContents(&e->basic,0,shared));CHECK(InventoryBasic_setInventorySlotContents(&e->basic,26,shared));
    NBTTagList *saved=InventoryEnderChest_saveInventoryToNBT(e);CHECK(saved&&NBTTagList_tagCount(saved)==2);NBTTagCompound *first=NBTTagList_getCompoundTagAt(saved,0);CHECK(NBTTagCompound_getByte_ascii(first,"Slot")==0&&NBTTagCompound_getCompoundTag_ascii(first,"tag")==tag);
    CHECK(InventoryEnderChest_loadInventoryFromNBT(e,saved)==ITEMSTACK_NBT_OK);ItemStack *a=InventoryBasic_getStackInSlot(&e->basic,0),*b=InventoryBasic_getStackInSlot(&e->basic,26);CHECK(a&&b&&a!=b&&a!=shared&&a->stackSize==0&&a->stackTagCompound==tag&&b->stackTagCompound==tag);
    CHECK(InventoryEnderChest_setChestTileEntity(e,(MCObject *)f));f->failAt='X';CHECK(!InventoryEnderChest_closeInventory(e,(MCObject *)f)&&e->associatedChest==(MCObject *)f&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
/* Whole heap snapshots must preserve cross-owner stacks, titles, listener
   cycles and chest aliases; original save/load intentionally reconstructs stacks. */
static void lifecycle(void){
    MCObjectHeap *h=MCObjectHeap_new(8*1024*1024);CHECK(h);Fixture *f=fixture(h);f->action='4';InventoryEnderChest *e=InventoryEnderChest_new(h,&basic_deps,&chest_deps,(MCObject *)f);CHECK(e);f->inventory=&e->basic;f->food=FoodStats_new(h);CHECK(f->food);f->extra=(MCObject *)e;
    CHECK(InventoryEnderChest_setChestTileEntity(e,(MCObject *)f));CHECK(InventoryBasic_addInventoryChangeListener(&e->basic,(MCObject *)f));ItemStack *s=ItemStack_new(h,ItemStack_registryItem(260),1,0);CHECK(s);CHECK(InventoryBasic_setInventorySlotContents(&e->basic,1,s));CHECK(InventoryBasic_setInventorySlotContents(&e->basic,2,s));
    MCObjectRoot r={0};CHECK(MCObjectRoot_init(&r,h,(MCObject *)f));CHECK(MCObjectHeap_collect(h));MCObjectHeap *copy=MCObjectHeap_clone(h);CHECK(copy);MCObjectRoot rr={0};CHECK(MCObjectRoot_rebind(&rr,copy,&r));Fixture *ff=(Fixture *)MCObjectRoot_get(&rr);CHECK(ff&&ff!=f);InventoryEnderChest *ee=(InventoryEnderChest *)ff->inventory;
    CHECK(ee->associatedChest==(MCObject *)ff&&ee->basic.context==(MCObject *)ff&&ee->basic.inventoryContents->items[1]==ee->basic.inventoryContents->items[2]);CHECK(MCObjectHeap_adopt(h,copy));f=(Fixture *)MCObjectRoot_get(&r);CHECK(f&&f->inventory&&f->food->foodLevel==20);CHECK(MCObjectHeap_collect(h));MCObjectHeap_free(copy);MCObjectHeap_free(h);
}
/* A foreign native callback context must fail the working heap before any
   callback affects the external graph. The completed source setter prefix is
   still present in the failed disposable graph. */
static void foreign_context_and_bounds(void){
    MCObjectHeap *h=MCObjectHeap_new(4*1024*1024),*external=MCObjectHeap_new(4*1024*1024);CHECK(h&&external);
    Fixture *f=fixture(h),*other=fixture(external),*listener=fixture(h);InventoryBasic *b=InventoryBasic_new(h,NBTString_fromASCII(h,"b"),false,1,&basic_deps,(MCObject *)f);CHECK(b);listener->inventory=b;listener->action='1';CHECK(InventoryBasic_addInventoryChangeListener(b,(MCObject *)listener));
    b->context=(MCObject *)other;ItemStack *s=ItemStack_new(h,ItemStack_registryItem(1),1,0);CHECK(s);CHECK(!InventoryBasic_setInventorySlotContents(b,0,s));CHECK(MCObjectHeap_failed(h)&&b->inventoryContents->items[0]==s&&other->used==0&&!MCObjectHeap_failed(external));MCObjectHeap_free(h);MCObjectHeap_free(external);
    for(int c=0;c<5;c++){
        h=MCObjectHeap_new(4*1024*1024);CHECK(h);b=InventoryBasic_new(h,NULL,false,1,NULL,NULL);CHECK(b);
        if(c==0){CHECK(InventoryBasic_getStackInSlot(b,-1)==NULL&&!MCObjectHeap_failed(h));CHECK(InventoryBasic_decrStackSize(b,-1,1)==NULL&&MCObjectHeap_failed(h));}
        if(c==1){b->inventoryContents->length=INT32_MAX;CHECK(!InventoryBasic_setInventorySlotContents(b,1,NULL)&&MCObjectHeap_failed(h));b->inventoryContents->length=1;}
        if(c==2)CHECK(!InventoryBasic_removeInventoryChangeListener(b,NULL)&&MCObjectHeap_failed(h));
        if(c==3){CHECK(InventoryBasic_addInventoryChangeListener(b,NULL));CHECK(InventoryBasic_addInventoryChangeListener(b,NULL));CHECK(InventoryBasic_removeInventoryChangeListener(b,NULL));CHECK(InventoryBasic_removeInventoryChangeListener(b,NULL));CHECK(InventoryBasic_markDirty(b)&&!MCObjectHeap_failed(h));}
        if(c==4){InventoryEnderChest *e=InventoryEnderChest_new(h,NULL,NULL,NULL);CHECK(e);Fixture *chest=fixture(h);CHECK(InventoryEnderChest_setChestTileEntity(e,(MCObject *)chest));CHECK(!InventoryEnderChest_isUseableByPlayer(e,NULL)&&MCObjectHeap_failed(h));}
        CHECK(!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);
    }
}
/* A failed read clears the inherited 27 slots first, preserving the source
   exception prefix, but aborting its memo-cloned graph leaves the live parent. */
static void source_prefix_abort_and_oom(void){
    MCObjectHeap *h=MCObjectHeap_new(4*1024*1024);CHECK(h);InventoryEnderChest *e=InventoryEnderChest_new(h,NULL,NULL,NULL);CHECK(e);ItemStack *s=ItemStack_new(h,ItemStack_registryItem(1),5,0);CHECK(s);CHECK(InventoryBasic_setInventorySlotContents(&e->basic,0,s));MCObjectRoot r={0};CHECK(MCObjectRoot_init(&r,h,(MCObject *)e));
    MCObjectHeap *copy=MCObjectHeap_clone(h);CHECK(copy);MCObjectRoot rr={0};CHECK(MCObjectRoot_rebind(&rr,copy,&r));InventoryEnderChest *ee=(InventoryEnderChest *)MCObjectRoot_get(&rr);CHECK(InventoryEnderChest_loadInventoryFromNBT(ee,NULL)==ITEMSTACK_NBT_FAILURE);CHECK(ee->basic.inventoryContents->items[0]==NULL&&MCObjectHeap_failed(copy)&&!MCObjectHeap_hasBorrowers(copy));CHECK(e->basic.inventoryContents->items[0]==s&&s->stackSize==5&&!MCObjectHeap_failed(h));MCObjectHeap_free(copy);MCObjectHeap_free(h);
    for(size_t budget=0;budget<=1800;budget+=37){
        h=MCObjectHeap_new(budget);CHECK(h);e=InventoryEnderChest_new(h,NULL,NULL,NULL);if(e){CHECK(e->basic.slotsCount==27&&e->basic.inventoryContents->length==27&&!MCObjectHeap_failed(h));}else CHECK(MCObjectHeap_failed(h));CHECK(!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);
    }
}
/* ArrayList.remove calls the removal receiver's equals, and removes only its
   first match; growth and duplicate refs must not silently deduplicate. */
static void listener_equals_duplicates(void){
    MCObjectHeap *h=MCObjectHeap_new(4*1024*1024);CHECK(h);Fixture *f=fixture(h),*a=fixture(h),*target=fixture(h),*b=fixture(h);InventoryBasicDependencies deps=basic_deps;deps.listenerEquals=semantic_equals;
    InventoryBasic *inv=InventoryBasic_new(h,NBTString_fromASCII(h,"b"),false,1,&deps,(MCObject *)f);CHECK(inv);a->inventory=b->inventory=inv;a->action=target->action='1';b->action='2';
    CHECK(InventoryBasic_addInventoryChangeListener(inv,(MCObject *)a));CHECK(InventoryBasic_addInventoryChangeListener(inv,(MCObject *)a));CHECK(InventoryBasic_addInventoryChangeListener(inv,(MCObject *)b));CHECK(InventoryBasic_removeInventoryChangeListener(inv,(MCObject *)target));CHECK(InventoryBasic_markDirty(inv));CHECK(a->calls==1&&b->calls==1&&strcmp(f->events,"12")==0);
    for(int i=0;i<40;i++)CHECK(InventoryBasic_addInventoryChangeListener(inv,(MCObject *)b));
    CHECK(InventoryBasic_markDirty(inv));CHECK(a->calls==2&&b->calls==42&&!MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
int main(void){defaults();food_values();food_update();basic_values_and_listeners();ender_nbt_chest();lifecycle();foreign_context_and_bounds();source_prefix_abort_and_oom();listener_equals_duplicates();printf("player leaves: %u checks passed\n",checks);return 0;}
