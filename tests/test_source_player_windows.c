#include "entity/player/EntityPlayerMPWindows.h"
#include "inventory/ContainerWorkbench.h"
#include "item/crafting/RecipeBookCloning.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x) do { ++checks;if (!(x)) {fprintf(stderr,"source player windows check %u at %d: %s\n",checks,__LINE__,#x);exit(1);} } while (0)
enum {WINDOW_ITEMS=1,SET_SLOT,CLOSE_WINDOW,DROP};
typedef struct {int kind;int32_t window,index;ContainerList *list;ItemStack *stack;Container *open;} Event;
typedef struct {MCObject object;Event events[64];unsigned count;int failKind;ItemStack *replaceCursor;} Effects;
static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    Effects *effects=(Effects *)object;
    for (unsigned i=0;i<effects->count;i++) {
        Event *e=&effects->events[i];e->list=(ContainerList *)visitor((MCObject *)e->list,context);
        e->stack=(ItemStack *)visitor((MCObject *)e->stack,context);e->open=(Container *)visitor((MCObject *)e->open,context);
    }
    effects->replaceCursor=(ItemStack *)visitor((MCObject *)effects->replaceCursor,context);
}
static const MCObjectClass effect_class={"fixture.windows.required-effect-record",MCObjectHeap_plainClone,trace,NULL};
/* These fixtures retain callback arguments to verify source evaluation order
   and aliasing. They are not production packet constructors or socket hooks. */
static bool record(MCGameplayPlayer *player,int kind,int32_t window,int32_t index,ContainerList *list,ItemStack *stack) {
    Effects *effects=(Effects *)player->effects;CHECK(effects&&effects->count<64);
    effects->events[effects->count++]=(Event){kind,window,index,list,stack,player->openContainer};
    CHECK(!MCObjectHeap_collect(player->living.entity.object.heap));MCObjectHeap_touch(player->living.entity.object.heap);
    return effects->failKind!=kind;
}
static bool items(MCGameplayPlayer *player,int32_t window,ContainerList *list) {
    bool result=record(player,WINDOW_ITEMS,window,0,list,NULL);Effects *effects=(Effects *)player->effects;
    if (effects->replaceCursor) CHECK(InventoryPlayer_setItemStack(player->inventory,effects->replaceCursor));
    return result;
}
static bool set_slot(MCGameplayPlayer *player,int32_t window,int32_t index,ItemStack *stack) {return record(player,SET_SLOT,window,index,NULL,stack);}
static bool close_window(MCGameplayPlayer *player,int32_t window) {return record(player,CLOSE_WINDOW,window,0,NULL,NULL);}
static const EntityPlayerMPWindowsDependencies window_dependencies={items,set_slot,close_window};
static NBTString *display(MCObject *world,const ItemStack *stack) {(void)world;return NBTString_fromASCII(stack->object.heap,"Written Book");}
static ItemStack *recipe(InventoryCrafting *grid,MCObject *object) {return CraftingManager_findMatchingRecipe(((MCGameplayWorld *)object)->manager,grid,object,display,object);}
static ItemStackArray *remaining(InventoryCrafting *grid,MCObject *object) {return CraftingManager_func_180303_b(((MCGameplayWorld *)object)->manager,grid,object);}
static bool crafted(ItemStack *stack,MCObject *world,MCObject *object,int32_t count) {(void)stack;(void)world;(void)object;(void)count;CHECK(false);return false;}
static bool achievement(MCObject *object,mc_crafting_achievement value) {(void)object;(void)value;CHECK(false);return false;}
static bool drop(MCObject *object,ItemStack *stack,bool scatter) {CHECK(!scatter);return record((MCGameplayPlayer *)object,DROP,0,0,NULL,stack);}
static int id(const Item *item) {return ItemStack_registryId(item);}
static bool pickaxe(const Item *item) {int n=id(item);return n==257||n==270||n==274||n==278||n==285;}
static bool hoe(const Item *item) {return id(item)>=290&&id(item)<=294;}
static bool sword(const Item *item) {int n=id(item);return n==267||n==268||n==272||n==276||n==283;}
static bool wood_pickaxe(const Item *item) {return id(item)==270;}
static int32_t armor(const Item *item) {int n=id(item);return n>=298&&n<=317?(n-298)%4:-1;}
static const mc_crafting_dispatch crafting_dependencies={MCGameplayPlayer_inventory,MCGameplayPlayer_world,recipe,remaining,crafted,achievement,drop,pickaxe,hoe,sword,wood_pickaxe,armor,MCGameplayWorld_isRemote,MCGameplayWorld_isCraftingTable,MCGameplayPlayer_getDistanceSq};
static MCGameplayPlayer *setup(MCGameplay *game,MCObjectRootScope *scope,bool remote) {
    CHECK(MCGameplay_init(game,32*1024*1024));CHECK(MCObjectRootScope_begin(scope,game->heap));
    CraftingManager *manager=CraftingManager_newEmpty(game->heap);CHECK(manager);RecipeBookCloning *book=RecipeBookCloning_new(game->heap);CHECK(book);CHECK(CraftingManager_addRecipe(manager,RecipeBookCloning_asRecipe(book)));
    MCGameplayWorld *world=MCGameplayWorld_new(game->heap,MCGameplay_get(game),NULL,manager);CHECK(world);world->remote=remote;CHECK(MCGameplay_setWorld(game,(MCObject *)world));
    MCGameplayPlayer *player=MCGameplayPlayer_new(world,NBTString_fromASCII(game->heap,"Owner"),NULL,&crafting_dependencies);CHECK(player);CHECK(MCGameplay_setPlayer(game,0,"11111111-1111-1111-1111-111111111111",(MCObject *)player));
    Effects *effects=(Effects *)MCObjectHeap_alloc(game->heap,sizeof(*effects),&effect_class);CHECK(effects);player->effects=(MCObject *)effects;CHECK(EntityPlayerMPWindows_bind(player,&window_dependencies));return player;
}
static ItemStack *stack(MCGameplayPlayer *player,int32_t count) {ItemStack *s=ItemStack_new(player->living.entity.object.heap,ItemStack_registryItem(1),count,0);CHECK(s);return s;}
static ContainerWorkbench *workbench(MCGameplayPlayer *player,int32_t window) {
    mc_crafting_position position={0,0,0};ContainerWorkbench *c=ContainerWorkbench_new(player->inventory,(MCObject *)((MCGameplayWorld *)(player->living.entity.worldObj)),&position,&crafting_dependencies);CHECK(c);c->container.windowId=window;player->openContainer=&c->container;return c;
}
static void finish(MCGameplay *game,MCObjectRootScope *scope) {CHECK(!MCObjectHeap_failed(game->heap));MCObjectRootScope_end(scope);CHECK(MCGameplay_free(game));}
static void listener_and_shared_refs(void) {
    MCGameplay game={0};MCObjectRootScope scope={0};MCGameplayPlayer *player=setup(&game,&scope,false);ContainerWorkbench *bench=workbench(player,257);Effects *effects=(Effects *)player->effects;
    ItemStack *shared=stack(player,0);CHECK(InventoryPlayer_setItemStack(player->inventory,shared));CHECK(InventoryPlayer_setInventorySlotContents(player->inventory,0,shared));CHECK(InventoryCrafting_setInventorySlotContents(bench->craftMatrix,0,shared));
    ItemStack *output=stack(player,-7);CHECK(InventoryCraftResult_setInventorySlotContents(bench->craftResult,0,output));
    ICrafting listener=EntityPlayerMPWindows_listener(player);CHECK(listener.target==(MCObject *)player&&listener.methods);CHECK(Container_onCraftGuiOpened(&bench->container,listener));
    CHECK(effects->count==4&&effects->events[0].kind==WINDOW_ITEMS&&effects->events[1].kind==SET_SLOT);
    CHECK(effects->events[0].window==257&&ContainerList_size(effects->events[0].list)==46);
    CHECK(ContainerList_get(effects->events[0].list,0)==(MCObject *)output&&ContainerList_get(effects->events[0].list,1)==(MCObject *)shared&&ContainerList_get(effects->events[0].list,37)==(MCObject *)shared);
    CHECK(effects->events[1].window==-1&&effects->events[1].index==-1&&effects->events[1].stack==shared);
    CHECK(effects->events[2].index==1&&effects->events[3].index==37&&effects->events[2].stack!=shared&&effects->events[3].stack!=shared&&effects->events[2].stack!=effects->events[3].stack);
    CHECK(effects->events[2].stack->stackSize==0&&effects->events[3].stack->stackSize==0);effects->count=0;
    CHECK(EntityPlayerMPWindows_sendSlotContents(player,&bench->container,0,output));CHECK(effects->count==0);
    player->isChangingQuantityOnly=true;CHECK(EntityPlayerMPWindows_sendSlotContents(player,&bench->container,1,shared));CHECK(EntityPlayerMPWindows_updateHeldItem(player));CHECK(effects->count==0);
    CHECK(EntityPlayerMPWindows_sendContainerToPlayer(player,&bench->container));CHECK(effects->count==2&&effects->events[0].kind==WINDOW_ITEMS&&effects->events[1].stack==shared);
    effects->count=0;ItemStack *replacement=stack(player,-1);effects->replaceCursor=replacement;
    CHECK(EntityPlayerMPWindows_updateCraftingInventory(player,&bench->container,Container_getInventory(&bench->container)));CHECK(effects->events[1].stack==replacement);effects->replaceCursor=NULL;effects->count=0;
    player->isChangingQuantityOnly=false;shared->stackSize=-1;CHECK(Container_detectAndSendChanges(&bench->container));CHECK(effects->count==2&&effects->events[0].index==1&&effects->events[1].index==37);
    MCObjectRootScope_end(&scope);MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&game,&tx));CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));MCGameplayPlayer *copy=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];Effects *copyEffects=(Effects *)copy->effects;CHECK(copy->windowDependencies==&window_dependencies&&copyEffects!=effects);copyEffects->count=0;
    CHECK(Container_detectAndSendChanges(copy->openContainer));CHECK(copyEffects->count==0);CHECK(InventoryPlayer_setItemStack(copy->inventory,NULL));CHECK(EntityPlayerMPWindows_updateHeldItem(copy));CHECK(copyEffects->count==1&&copyEffects->events[0].stack==NULL&&effects->count==2);
    CHECK(Container_removeCraftingFromCrafters(copy->openContainer,EntityPlayerMPWindows_listener(copy)));CHECK(ContainerList_size(copy->openContainer->crafters)==0);
    MCObjectRootScope_end(&scope);CHECK(MCGameplay_abort(&tx));CHECK(MCObjectRootScope_begin(&scope,game.heap));CHECK(ContainerList_size(bench->container.crafters)==1);finish(&game,&scope);
}
static void windows_and_close_order(void) {
    const int32_t ids[]={-1,0,127,256,INT32_MIN,INT32_MAX};
    for (unsigned n=0;n<6;n++) for (unsigned remote=0;remote<2;remote++) {
        MCGameplay game={0};MCObjectRootScope scope={0};MCGameplayPlayer *p=setup(&game,&scope,remote!=0);ContainerWorkbench *bench=workbench(p,ids[n]);Effects *effects=(Effects *)p->effects;
        ItemStack *shared=stack(p,-1);CHECK(InventoryPlayer_setItemStack(p->inventory,shared));CHECK(InventoryCrafting_setInventorySlotContents(bench->craftMatrix,0,shared));ItemStack *output=stack(p,0);CHECK(InventoryCraftResult_setInventorySlotContents(bench->craftResult,0,output));
        CHECK(EntityPlayerMPWindows_sendSlotContents(p,&bench->container,1,shared));CHECK(effects->count==1&&effects->events[0].window==ids[n]&&effects->events[0].stack==shared);effects->count=0;
        CHECK(EntityPlayerMPWindows_closeScreen(p));CHECK(effects->count==(remote?2u:3u)&&effects->events[0].kind==CLOSE_WINDOW&&effects->events[0].window==ids[n]);
        CHECK(effects->events[1].kind==DROP&&effects->events[1].stack==shared&&effects->events[1].open==&bench->container);if(!remote)CHECK(effects->events[2].kind==DROP&&effects->events[2].stack==shared);
        CHECK(p->openContainer==p->inventoryContainer&&!InventoryPlayer_getItemStack(p->inventory));CHECK(InventoryCrafting_getStackInSlot(bench->craftMatrix,0)==(remote?shared:NULL));CHECK(InventoryCraftResult_getStackInSlot(bench->craftResult,0)==output);
        effects->count=0;CHECK(InventoryPlayer_setItemStack(p->inventory,shared));CHECK(InventoryCrafting_setInventorySlotContents(((ContainerPlayer *)(p->inventoryContainer))->craftMatrix,0,shared));CHECK(InventoryCraftResult_setInventorySlotContents(((ContainerPlayer *)(p->inventoryContainer))->craftResult,0,output));CHECK(EntityPlayerMPWindows_closeContainer(p));
        CHECK(effects->count==2&&effects->events[0].kind==DROP&&effects->events[1].kind==DROP);CHECK(!InventoryCrafting_getStackInSlot(((ContainerPlayer *)(p->inventoryContainer))->craftMatrix,0)&&!InventoryCraftResult_getStackInSlot(((ContainerPlayer *)(p->inventoryContainer))->craftResult,0));finish(&game,&scope);
    }
}
static void failures_and_suppressed_dependencies(void) {
    for (int failure=WINDOW_ITEMS;failure<=DROP;failure++) {
        MCGameplay game={0};MCObjectRootScope scope={0};MCGameplayPlayer *p=setup(&game,&scope,false);ContainerWorkbench *bench=workbench(p,9);Effects *effects=(Effects *)p->effects;ItemStack *shared=stack(p,0);CHECK(InventoryPlayer_setItemStack(p->inventory,shared));CHECK(InventoryCrafting_setInventorySlotContents(bench->craftMatrix,0,shared));effects->failKind=failure;
        if(failure<=SET_SLOT) {CHECK(!EntityPlayerMPWindows_sendContainerToPlayer(p,&bench->container));CHECK(effects->count==(failure==WINDOW_ITEMS?1u:2u));}
        else {CHECK(!EntityPlayerMPWindows_closeScreen(p));CHECK(effects->count==(failure==CLOSE_WINDOW?1u:2u));}
        CHECK(MCObjectHeap_failed(game.heap)&&p->openContainer==&bench->container&&InventoryPlayer_getItemStack(p->inventory)==shared);MCObjectRootScope_end(&scope);CHECK(MCGameplay_free(&game));
    }
    for (unsigned operation=0;operation<5;operation++) {
        MCGameplay game={0};MCObjectRootScope scope={0};MCGameplayPlayer *p=setup(&game,&scope,false);Effects *effects=(Effects *)p->effects;
        if(operation==0) {p->windowDependencies=NULL;p->isChangingQuantityOnly=true;CHECK(EntityPlayerMPWindows_updateHeldItem(p));CHECK(EntityPlayerMPWindows_sendSlotContents(p,p->openContainer,0,NULL));CHECK(EntityPlayerMPWindows_sendSlotContents(p,p->openContainer,1,NULL));CHECK(effects->count==0);finish(&game,&scope);continue;}
        if(operation==1) {EntityPlayerMPWindowsDependencies d=window_dependencies;d.sendSetSlot=NULL;CHECK(!EntityPlayerMPWindows_bind(p,&d));}
        else if(operation==2) {p->windowDependencies=NULL;CHECK(!EntityPlayerMPWindows_updateHeldItem(p));}
        else if(operation==3) {p->isChangingQuantityOnly=true;CHECK(!EntityPlayerMPWindows_sendSlotContents(p,p->openContainer,INT32_MAX,NULL));}
        else {MCObjectHeap *other=MCObjectHeap_new(1024*1024);CHECK(other);ItemStack *wrong=ItemStack_new(other,ItemStack_registryItem(1),1,0);CHECK(wrong);CHECK(!EntityPlayerMPWindows_sendSlotContents(p,p->openContainer,1,wrong));CHECK(!MCObjectHeap_failed(other));MCObjectHeap_free(other);}
        CHECK(MCObjectHeap_failed(game.heap)&&effects->count==0);MCObjectRootScope_end(&scope);CHECK(MCGameplay_free(&game));
    }
}
int main(void) {listener_and_shared_refs();windows_and_close_order();failures_and_suppressed_dependencies();printf("source player windows: %u checks passed\n",checks);return 0;}
