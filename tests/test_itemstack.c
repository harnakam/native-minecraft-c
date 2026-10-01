#include "item/ItemStack.h"
#include "item/crafting/RecipeBookCloning.h"
#include "entity/player/InventoryPlayer.h"
#include "inventory/InventoryCraftResult.h"
#include "nbt/NBTTagCompound.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { MCObject object; bool creative; int calls; int32_t observed; } Actor;
static const MCObjectClass actor_class={"TestActor",MCObjectHeap_plainClone,NULL,NULL};
static bool creative(const MCObject *o) { return ((const Actor *)o)->creative; }
static bool changed(MCObject *o,InventoryCrafting *g) { Actor *a=(Actor *)o; a->calls++; ItemStack *s=InventoryCrafting_getStackInSlot(g,0); a->observed=s?s->stackSize:INT32_MIN; return true; }
static NBTString *display(MCObject *context,const ItemStack *stack) { Actor *a=(Actor *)context; a->calls++; return NBTString_fromASCII(stack->object.heap,"native display dependency"); }
static ItemStack *make(MCObjectHeap *h,int id,int32_t count,int32_t damage) { ItemStack *s=ItemStack_new(h,ItemStack_registryItem(id),count,damage); CHECK(s); return s; }
static void core(void) {
    MCObjectHeap *h=MCObjectHeap_new(16*1024*1024); CHECK(h);
    MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h));
    const int32_t counts[]={INT32_MIN,-1,0,1,127,128,INT32_MAX};
    for (unsigned i=0;i<sizeof(counts)/sizeof(*counts);i++) { ItemStack *s=make(h,1,counts[i],-1); CHECK(s->stackSize==counts[i]); CHECK(s->itemDamage==0); }
    ItemStack *s=make(h,1,2,3),*part=ItemStack_splitStack(h,s,5); CHECK(part&&part->stackSize==5&&s->stackSize==-3);
    s=make(h,1,2,3); part=ItemStack_splitStack(h,s,-2); CHECK(part&&part->stackSize==-2&&s->stackSize==4);
    s=make(h,1,INT32_MAX,3); part=ItemStack_splitStack(h,s,INT32_MIN); CHECK(part&&part->stackSize==INT32_MIN&&s->stackSize==-1);
    NBTTagCompound *tag=NBTTagCompound_new(h); CHECK(tag); CHECK(NBTTagCompound_setInteger_ascii(tag,"original",42));
    s=make(h,1,128,7); CHECK(ItemStack_setTagCompound(s,tag)); s->animationsToGo=8;
    ItemStack *copy=ItemStack_copy(h,s); CHECK(copy&&copy!=s&&copy->stackTagCompound!=tag); CHECK(copy->animationsToGo==0); CHECK(ItemStack_areItemStackTagsEqual(s,copy));
    NBTTagCompound *out=NBTTagCompound_new(h); CHECK(ItemStack_writeToNBT(s,out)==out); CHECK(NBTTagCompound_getTag_ascii(out,"tag")== (NBTBase *)tag); CHECK(NBTTagCompound_getByte_ascii(out,"Count")==-128);
    CHECK(NBTTagCompound_setInteger_ascii(tag,"original",43)); CHECK(NBTTagCompound_getInteger_ascii(copy->stackTagCompound,"original")==42);
    NBTTagCompound *empty=NBTTagCompound_new(h); CHECK(NBTTagCompound_setString_ascii(empty,"id",NBTString_fromASCII(h,"minecraft:stone"))); CHECK(NBTTagCompound_setByte_ascii(empty,"Count",-128)); CHECK(NBTTagCompound_setShort_ascii(empty,"Damage",-1));
    CHECK(ItemStack_readFromNBT(s,empty)==ITEMSTACK_NBT_OK); CHECK(s->stackSize==-128&&s->itemDamage==0&&s->stackTagCompound==tag);
    s=make(h,276,1,5); CHECK(ItemStack_getMaxDamage(s)==1561&&ItemStack_isItemDamaged(s)); s->itemDamage=-5; copy=ItemStack_copy(h,s); CHECK(copy&&copy->itemDamage==0);
    s=make(h,1,0,0); CHECK(s&&ItemStack_getItem(s)); CHECK(!ItemStack_areItemStacksEqual(NULL,s)); CHECK(ItemStack_areItemStacksEqual(NULL,NULL));
    CHECK(ItemStack_setRepairCost(s,23)); CHECK(ItemStack_getRepairCost(s)==23); CHECK(NBTTagCompound_setFloat_ascii(s->stackTagCompound,"RepairCost",23.5f)); CHECK(ItemStack_getRepairCost(s)==0);
    Actor *a=(Actor *)MCObjectHeap_alloc(h,sizeof(*a),&actor_class); CHECK(a); CHECK(ItemStack_setStackDisplayName(s,NBTString_fromUTF8(h,"本\xE6\x97\xA5"))); CHECK(ItemStack_hasDisplayName(s));
    CHECK(NBTString_equals(ItemStack_getDisplayName(s,display,(MCObject *)a),NBTTagCompound_getString_ascii(NBTTagCompound_getCompoundTag_ascii(s->stackTagCompound,"display"),"Name"))); CHECK(a->calls==1); CHECK(ItemStack_clearCustomName(s)); CHECK(!ItemStack_hasDisplayName(s));
    CHECK(!MCObjectHeap_failed(h)); MCObjectRootScope_end(&scope); MCObjectHeap_free(h);
}
static void copy_ownership(void) {
    MCObjectHeap *source=MCObjectHeap_new(1024*1024),*destination=MCObjectHeap_new(1024*1024); CHECK(source&&destination);
    ItemStack *s=make(source,1,1,0); CHECK(!ItemStack_copy(destination,s)); CHECK(MCObjectHeap_failed(destination)); CHECK(!MCObjectHeap_liveObjects(destination)); CHECK(!MCObjectHeap_failed(source));
    MCObjectHeap_free(destination); MCObjectHeap_free(source);
}
static void references(void) {
    MCObjectHeap *h=MCObjectHeap_new(16*1024*1024); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h));
    Actor *a=(Actor *)MCObjectHeap_alloc(h,sizeof(*a),&actor_class); CHECK(a);
    InventoryCrafting *g=InventoryCrafting_new(h,(MCObject *)a,changed,3,3); InventoryCraftResult *r=InventoryCraftResult_new(h); InventoryPlayer *p=InventoryPlayer_new(h,(MCObject *)a,creative); CHECK(g&&r&&p);
    ItemStack *s=make(h,1,2,0); CHECK(InventoryCrafting_setInventorySlotContents(g,0,s)); CHECK(a->calls==1&&a->observed==2); CHECK(InventoryCrafting_decrStackSize(g,0,5)==s); CHECK(a->calls==2&&a->observed==INT32_MIN);
    CHECK(InventoryCrafting_setInventorySlotContents(g,0,s)); ItemStack *neg=InventoryCrafting_decrStackSize(g,0,-2); CHECK(neg&&neg->stackSize==-2&&s->stackSize==4); CHECK(a->observed==4);
    int before=a->calls; CHECK(InventoryCrafting_removeStackFromSlot(g,0)==s); InventoryCrafting_clear(g); CHECK(a->calls==before);
    s=make(h,1,0,0); CHECK(InventoryCraftResult_setInventorySlotContents(r,-300,s)); CHECK(InventoryCraftResult_getStackInSlot(r,INT32_MAX)==s); CHECK(InventoryCraftResult_decrStackSize(r,44,-1)==s); CHECK(!InventoryCraftResult_getStackInSlot(r,0));
    CHECK(InventoryCrafting_setInventorySlotContents(g,0,s)); CHECK(InventoryPlayer_setItemStack(p,s)); CHECK(InventoryCraftResult_setInventorySlotContents(r,0,s));
    MCObjectRoot rg,rp,rr; CHECK(MCObjectRoot_init(&rg,h,(MCObject *)g)); CHECK(MCObjectRoot_init(&rp,h,(MCObject *)p)); CHECK(MCObjectRoot_init(&rr,h,(MCObject *)r)); MCObjectRootScope_end(&scope);
    MCObjectHeap *clone=MCObjectHeap_clone(h); CHECK(clone); MCObjectRoot cg,cp,cr; CHECK(MCObjectRoot_rebind(&cg,clone,&rg)); CHECK(MCObjectRoot_rebind(&cp,clone,&rp)); CHECK(MCObjectRoot_rebind(&cr,clone,&rr));
    InventoryCrafting *gg=(InventoryCrafting *)MCObjectRoot_get(&cg); InventoryPlayer *pp=(InventoryPlayer *)MCObjectRoot_get(&cp); InventoryCraftResult *result=(InventoryCraftResult *)MCObjectRoot_get(&cr);
    ItemStack *shared=InventoryCrafting_getStackInSlot(gg,0); CHECK(shared!=s&&shared==InventoryPlayer_getItemStack(pp)&&shared==InventoryCraftResult_getStackInSlot(result,0)); CHECK(gg->eventHandler==pp->player); CHECK(shared->stackSize==0);
    CHECK(MCObjectHeap_collect(clone)); CHECK(InventoryPlayer_getItemStack(pp)==shared); MCObjectHeap_free(clone); MCObjectHeap_free(h);
}
static void books(void) {
    /* Numeric goldens independently observed on 1.8.9, source metadata 0/7.
       Original SlotCrafting remainder sequence below exercises class methods. */
    for (int metadata=0;metadata<=7;metadata+=7) for (int creative_mode=0;creative_mode<=1;creative_mode++) for (int n=1;n<=18;n=(n==1?2:18)+ (n==18?1:0)) for (int empty=0;empty<=36;empty=(empty==0?1:36)+(empty==36?1:0)) {
        MCObjectHeap *h=MCObjectHeap_new(16*1024*1024); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h)); Actor *a=(Actor *)MCObjectHeap_alloc(h,sizeof(*a),&actor_class); CHECK(a); a->creative=creative_mode!=0;
        InventoryCrafting *g=InventoryCrafting_new(h,(MCObject *)a,changed,3,3); InventoryPlayer *p=InventoryPlayer_new(h,(MCObject *)a,creative); CHECK(g&&p);
        for (int i=empty;i<36;i++) CHECK(InventoryPlayer_setInventorySlotContents(p,i,make(h,1,64,0)));
        ItemStack *source=make(h,387,n,metadata); NBTTagCompound *tag=NBTTagCompound_new(h); CHECK(tag); CHECK(NBTTagCompound_setInteger_ascii(tag,"generation",0)); CHECK(ItemStack_setTagCompound(source,tag)); CHECK(InventoryCrafting_setInventorySlotContents(g,4,source)); CHECK(InventoryCrafting_setInventorySlotContents(g,0,make(h,386,1,0)));
        CHECK(RecipeBookCloning_matches(g,NULL)); ItemStack *output=RecipeBookCloning_getCraftingResult(g,display,(MCObject *)a); CHECK(output&&output->stackSize==1&&output->itemDamage==0&&output->stackTagCompound!=tag); CHECK(NBTTagCompound_getInteger_ascii(output->stackTagCompound,"generation")==1);
        ItemStackArray *rem=RecipeBookCloning_getRemainingItems(g); CHECK(rem&&rem->items[4]==source);
        ItemStack *dropped=NULL;
        for (int i=0;i<9;i++) {
            ItemStack *in=InventoryCrafting_getStackInSlot(g,i),*rest=rem->items[i];
            if (in) { CHECK(InventoryCrafting_decrStackSize(g,i,1)); in=InventoryCrafting_getStackInSlot(g,i); }
            if (rest) {
                if (!in) CHECK(InventoryCrafting_setInventorySlotContents(g,i,rest));
                else if (!InventoryPlayer_addItemStackToInventory(p,rest)) dropped=rest;
            }
        }
        int books_count=0; for (int i=0;i<36;i++) { ItemStack *it=InventoryPlayer_getStackInSlot(p,i); if (it&&ItemStack_registryId(it->item)==387) books_count+=it->stackSize; }
        int expected=n==1?1:(!creative_mode&&(empty==0||(empty==1&&n==18))?(empty==0?n-1:1):0);
        if (InventoryCrafting_getStackInSlot(g,4)!=source || source->stackSize!=expected) fprintf(stderr,"book meta=%d creative=%d n=%d empty=%d count=%d expected=%d same=%d inventory=%d\n",metadata,creative_mode,n,empty,source->stackSize,expected,InventoryCrafting_getStackInSlot(g,4)==source,books_count);
        CHECK(InventoryCrafting_getStackInSlot(g,4)==source&&source->stackSize==expected);
        CHECK(books_count==(n==1?0:(empty==0?0:(empty==1&&n==18?16:n-1))));
        CHECK((dropped!=NULL)==(!creative_mode&&n>1&&(empty==0||(empty==1&&n==18)))); CHECK(!dropped||dropped==source); CHECK(!MCObjectHeap_failed(h));
        MCObjectRootScope_end(&scope); MCObjectHeap_free(h);
    }
}
static void signed_insertion(void) {
    /* Independent actual-JVM observations, not calculated from the C method. */
    typedef struct { bool creative; int id,damage; int32_t count; int empty; bool result; int32_t remaining,stored; int slots; } Golden;
    static const Golden golden[]={
        {false,1,0,INT32_MIN,0,false,INT32_MIN,0,0}, {false,1,0,INT32_MIN,1,false,0,INT32_MIN,1},
        {false,1,0,-2,0,false,-2,0,0}, {false,1,0,-2,1,false,0,-2,1}, {false,1,0,0,36,false,0,0,0},
        {false,1,0,128,1,false,64,64,1}, {false,1,0,128,36,true,0,128,2},
        {false,1,0,INT32_MAX,1,false,2147483583,64,1}, {false,1,0,INT32_MAX,36,false,2147481343,2304,36},
        {true,1,0,INT32_MIN,0,true,0,0,0}, {true,1,0,INT32_MIN,1,false,0,INT32_MIN,1},
        {true,1,0,-2,0,true,0,0,0}, {true,1,0,-2,1,false,0,-2,1}, {true,1,0,0,36,false,0,0,0},
        {true,1,0,128,1,true,0,64,1}, {true,1,0,INT32_MAX,36,true,0,2304,36},
        {false,276,0,-2,1,false,0,-2,1}, {false,276,0,17,1,false,16,1,1}, {false,276,0,17,36,true,0,17,17},
        {false,276,0,128,36,false,92,36,36}, {true,276,0,-2,1,false,0,-2,1}, {true,276,0,128,1,true,0,1,1},
        {false,276,5,-2,0,false,-2,0,0}, {false,276,5,-2,1,true,0,-2,1}, {true,276,5,-2,0,true,0,0,0},
        {false,276,5,INT32_MIN,1,true,0,INT32_MIN,1},
        {false,387,0,17,0,false,17,0,0}, {false,387,0,17,1,false,1,16,1}, {false,387,0,17,36,true,0,17,2},
        {true,387,0,17,0,true,0,0,0}, {true,387,0,17,1,true,0,16,1}
    };
    for (unsigned i=0;i<sizeof(golden)/sizeof(*golden);i++) {
        const Golden *v=&golden[i]; MCObjectHeap *h=MCObjectHeap_new(2*1024*1024); Actor *a=(Actor *)MCObjectHeap_alloc(h,sizeof(*a),&actor_class); CHECK(a); a->creative=v->creative;
        InventoryPlayer *p=InventoryPlayer_new(h,(MCObject *)a,creative); CHECK(p);
        for (int j=v->empty;j<36;j++) CHECK(InventoryPlayer_setInventorySlotContents(p,j,make(h,5,64,0)));
        ItemStack *s=make(h,v->id,v->count,v->damage); CHECK(InventoryPlayer_addItemStackToInventory(p,s)==v->result); CHECK(s->stackSize==v->remaining);
        int32_t sum=0; int slots=0;
        for (int j=0;j<36;j++) { ItemStack *it=InventoryPlayer_getStackInSlot(p,j); if (it&&it->item==s->item) { sum+=it->stackSize; slots++; CHECK(it!=s&&it->animationsToGo==5); } }
        CHECK(sum==v->stored&&slots==v->slots); CHECK(!MCObjectHeap_failed(h)); MCObjectHeap_free(h);
    }
}
static void storage(void) {
    MCObjectHeap *h=MCObjectHeap_new(4*1024*1024); Actor *a=(Actor *)MCObjectHeap_alloc(h,sizeof(*a),&actor_class); CHECK(a); InventoryPlayer *p=InventoryPlayer_new(h,(MCObject *)a,creative); CHECK(p);
    ItemStack *s=make(h,387,0,7); NBTTagCompound *tag=NBTTagCompound_new(h); CHECK(tag); CHECK(NBTTagCompound_setInteger_ascii(tag,"generation",-1)); CHECK(ItemStack_setTagCompound(s,tag));
    CHECK(InventoryPlayer_setInventorySlotContents(p,0,s)); CHECK(InventoryPlayer_setItemStack(p,s)); p->currentItem=3;
    NBTTagList *list=NBTTagList_new(h); CHECK(list&&InventoryPlayer_writeToNBT(p,list)==list&&NBTTagList_tagCount(list)==1);
    NBTTagCompound *saved=NBTTagList_getCompoundTagAt(list,0); CHECK(NBTTagCompound_getByte_ascii(saved,"Slot")==0); CHECK(NBTTagCompound_getByte_ascii(saved,"Count")==0); CHECK(NBTString_equalsASCII(NBTTagCompound_getString_ascii(saved,"id"),"minecraft:written_book"));
    CHECK(NBTTagCompound_getTag_ascii(saved,"tag")==(NBTBase *)tag);
    ItemStackArray *old_main=p->mainInventory; CHECK(InventoryPlayer_readFromNBT(p,list)==ITEMSTACK_NBT_OK); ItemStack *loaded=InventoryPlayer_getStackInSlot(p,0);
    CHECK(p->mainInventory!=old_main&&loaded&&loaded!=s&&loaded->stackSize==0&&loaded->itemDamage==7); CHECK(loaded->stackTagCompound==tag&&InventoryPlayer_getItemStack(p)==s&&p->currentItem==3);
    NBTTagCompound *legacy=NBTTagCompound_new(h); CHECK(legacy); CHECK(NBTTagCompound_setShort_ascii(legacy,"id",276)); CHECK(NBTTagCompound_setByte_ascii(legacy,"Count",-2)); CHECK(NBTTagCompound_setShort_ascii(legacy,"Damage",-1));
    ItemStackNBTResult status; loaded=ItemStack_loadItemStackFromNBT(h,legacy,&status); CHECK(status==ITEMSTACK_NBT_OK&&loaded&&loaded->stackSize==-2&&loaded->itemDamage==0);
    const char *names[]={"+1","0001","MINECRAFT:stone","x:stone",":stone","stone"};
    for (unsigned i=0;i<sizeof(names)/sizeof(*names);i++) { CHECK(NBTTagCompound_setString_ascii(legacy,"id",NBTString_fromASCII(h,names[i]))); loaded=ItemStack_loadItemStackFromNBT(h,legacy,&status); CHECK(status==ITEMSTACK_NBT_OK&&loaded&&ItemStack_registryId(loaded->item)==1); }
    uint16_t arabic_one=0x661; CHECK(NBTTagCompound_setString_ascii(legacy,"id",NBTString_fromUTF16(h,&arabic_one,1))); loaded=ItemStack_loadItemStackFromNBT(h,legacy,&status); CHECK(loaded&&ItemStack_registryId(loaded->item)==1);
    CHECK(NBTTagCompound_setString_ascii(legacy,"id",NBTString_fromASCII(h,"minecraft:missing"))); CHECK(!ItemStack_loadItemStackFromNBT(h,legacy,&status)&&status==ITEMSTACK_NBT_OK);
    CHECK(NBTTagCompound_setShort_ascii(legacy,"id",397)); CHECK(NBTTagCompound_setTag_ascii(legacy,"tag",(NBTBase *)tag)); CHECK(ItemStack_loadItemStackFromNBT(h,legacy,&status)&&status==ITEMSTACK_NBT_OK);
    CHECK(NBTTagCompound_setString_ascii(tag,"SkullOwner",NBTString_fromASCII(h,"Alice"))); CHECK(!ItemStack_loadItemStackFromNBT(h,legacy,&status)&&status==ITEMSTACK_NBT_UNSUPPORTED_PROFILE);
    CHECK(NBTTagCompound_setString_ascii(tag,"SkullOwner",NBTString_fromASCII(h,""))); CHECK(ItemStack_loadItemStackFromNBT(h,legacy,&status)&&status==ITEMSTACK_NBT_OK);
    NBTTagCompound *owner=NBTTagCompound_new(h); CHECK(owner&&NBTTagCompound_setTag_ascii(tag,"SkullOwner",(NBTBase *)owner)); CHECK(ItemStack_loadItemStackFromNBT(h,legacy,&status)&&status==ITEMSTACK_NBT_OK);
    CHECK(!MCObjectHeap_failed(h)); MCObjectHeap_free(h);
}
int main(void) { core(); copy_ownership(); references(); books(); signed_insertion(); storage(); printf("ItemStack/reference classes: %u checks passed\n",checks); return 0; }
