#include "client/gui/GuiIngameHotbar.h"
#include "client/native_runtime.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"hotbar %u line%d: %s\n",checks,__LINE__,#x); exit(1); } } while (0)
enum { PUSH=1,TRANSLATE,SCALE,BODY,POP,FONT,OVERLAY };
typedef struct { MCObject object; MCGameplayPlayer *player; ItemStack *stack; GuiIngameHotbar *gui; MCObject *swapRenderer; int events[16]; unsigned count,failAt; float values[3][3]; int32_t x,y; bool replace,nullRenderer; } Fixture;
static void trace(MCObject *o,MCObjectVisitor v,void *c) { Fixture *f=(Fixture *)o; f->player=(MCGameplayPlayer *)v((MCObject *)f->player,c); f->stack=(ItemStack *)v((MCObject *)f->stack,c); f->gui=(GuiIngameHotbar *)v((MCObject *)f->gui,c); f->swapRenderer=v(f->swapRenderer,c); }
static const MCObjectClass klass={"HotbarFixture",MCObjectHeap_plainClone,trace,NULL};
static bool event(Fixture *f,int id) { CHECK(f->count<16); f->events[f->count++]=id; return f->count!=f->failAt; }
static bool push(MCObject *o) { return event((Fixture *)o,PUSH); }
static bool translate(MCObject *o,float x,float y,float z) { Fixture *f=(Fixture *)o; unsigned n=f->count==1 ? 0u : 2u; f->values[n][0]=x; f->values[n][1]=y; f->values[n][2]=z; return event(f,TRANSLATE); }
static bool scale(MCObject *o,float x,float y,float z) { Fixture *f=(Fixture *)o; f->values[1][0]=x; f->values[1][1]=y; f->values[1][2]=z; return event(f,SCALE); }
static bool body(MCObject *o,MCObject *r,ItemStack *s,int32_t x,int32_t y) { Fixture *f=(Fixture *)o; CHECK(r==o && s==f->stack); f->x=x; f->y=y; if (f->replace) f->player->inventory->mainInventory->items[0]=NULL; if (f->nullRenderer) f->gui->itemRenderer=NULL; return event(f,BODY); }
static bool pop(MCObject *o) { return event((Fixture *)o,POP); }
static MCObject *font(MCObject *o,MCObject *mc) { Fixture *f=(Fixture *)o; CHECK(o==mc); if (!event(f,FONT)) { MCObjectHeap_fail(o->heap); return NULL; } if (f->swapRenderer) f->gui->itemRenderer=f->swapRenderer; return o; }
static bool overlay(MCObject *o,MCObject *r,MCObject *fnt,ItemStack *s,int32_t x,int32_t y) { Fixture *f=(Fixture *)o; CHECK(r==(f->nullRenderer ? NULL : o) && fnt==o && s==f->stack && x==f->x && y==f->y); return event(f,OVERLAY) && r!=NULL; }
static const GuiIngameHotbarDependencies deps={push,translate,scale,body,pop,font,overlay};
static uint32_t bits(float f) { uint32_t b; memcpy(&b,&f,4); return b; }
int main(void) {
    mc_world terrain; mc_world_init(&terrain,0); MCGameplay game={0}; CHECK(mc_client_graph_init(&game,&terrain,"Hotbar"));
    MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,game.heap));
    Fixture *f=(Fixture *)MCObjectHeap_alloc(game.heap,sizeof *f,&klass); CHECK(f); f->player=mc_client_graph_player(&game);
    GuiIngameHotbar *gui=GuiIngameHotbar_nativeNew(game.heap,(MCObject *)f,(MCObject *)f,(MCObject *)f,&deps); CHECK(gui);
    MCObjectRoot root={0}; CHECK(MCObjectRoot_init(&root,game.heap,(MCObject *)gui));
    CHECK(GuiIngame_renderHotbarItem(gui,0,10,20,0.5f,f->player)); CHECK(f->count==0);
    f->stack=ItemStack_new(game.heap,ItemStack_registryItem(276),0,2); CHECK(f->stack);
    f->player->inventory->mainInventory->items[0]=f->stack; f->stack->animationsToGo=5;
    CHECK(GuiIngame_renderHotbarItem(gui,0,10,20,0.5f,f->player));
    const int expected[]={PUSH,TRANSLATE,SCALE,TRANSLATE,BODY,POP,FONT,OVERLAY}; CHECK(f->count==8 && !memcmp(f->events,expected,sizeof expected));
    CHECK(f->values[0][0]==18 && f->values[0][1]==32 && f->values[2][0]==-18 && f->values[2][1]==-32);
    CHECK(bits(f->values[1][0])==UINT32_C(0x3f06bca2)); CHECK(bits(f->values[1][1])==UINT32_C(0x3fb9999a));
    f->gui=gui; f->swapRenderer=MCObjectHeap_alloc(game.heap,sizeof(Fixture),&klass); CHECK(f->swapRenderer); f->count=0;
    CHECK(GuiIngame_renderHotbarItem(gui,0,10,20,0.5f,f->player)); CHECK(gui->itemRenderer==f->swapRenderer);
    gui->itemRenderer=(MCObject *)f; f->swapRenderer=NULL;
    for (int32_t count=-128;count<=1;count++) {
        f->stack->stackSize=count; f->stack->animationsToGo=0; f->count=0;
        CHECK(GuiIngame_renderHotbarItem(gui,0,10,20,0.25f,f->player)); CHECK(f->count==3 && f->events[0]==BODY && f->events[1]==FONT && f->events[2]==OVERLAY);
    }
    f->count=0; f->stack->animationsToGo=5; f->replace=true;
    CHECK(GuiIngame_renderHotbarItem(gui,0,INT32_MAX,INT32_MIN,0,f->player)); CHECK(f->player->inventory->mainInventory->items[0]==NULL);
    CHECK(f->values[0][0]==(float)(INT32_MIN+7) && f->values[0][1]==(float)(INT32_MIN+12));
    f->replace=false; f->player->inventory->mainInventory->items[0]=f->stack;
    MCObjectRootScope_end(&scope); CHECK(MCObjectHeap_collect(game.heap));
    MCObjectHeap *clone=MCObjectHeap_clone(game.heap); CHECK(clone); MCObjectRoot cloned={0}; CHECK(MCObjectRoot_rebind(&cloned,clone,&root));
    GuiIngameHotbar *copied=(GuiIngameHotbar *)MCObjectRoot_get(&cloned); CHECK(copied!=gui && copied->mc!=gui->mc && copied->mc==copied->itemRenderer);
    MCObjectRoot_drop(&cloned); MCObjectHeap_free(clone); MCObjectRoot_drop(&root); CHECK(MCGameplay_free(&game)); mc_world_free(&terrain);
    for (unsigned failure=1;failure<=8;failure++) {
        mc_world_init(&terrain,0); CHECK(mc_client_graph_init(&game,&terrain,"FailHotbar")); MCObjectHeap *heap=game.heap;
        CHECK(MCObjectRootScope_begin(&scope,heap));
        f=(Fixture *)MCObjectHeap_alloc(heap,sizeof *f,&klass); CHECK(f); f->failAt=failure;
        f->player=mc_client_graph_player(&game); f->stack=ItemStack_new(heap,ItemStack_registryItem(1),-7,0); CHECK(f->stack);
        f->stack->animationsToGo=5; f->player->inventory->mainInventory->items[0]=f->stack;
        gui=GuiIngameHotbar_nativeNew(heap,(MCObject *)f,(MCObject *)f,(MCObject *)f,&deps); CHECK(gui);
        CHECK(!GuiIngame_renderHotbarItem(gui,0,0,0,0.5f,f->player)); CHECK(f->count==failure && MCObjectHeap_failed(heap));
        CHECK(!memcmp(f->events,expected,failure*sizeof expected[0]));
        MCObjectRootScope_end(&scope); CHECK(MCGameplay_free(&game)); mc_world_free(&terrain);
    }
    /* Java array length/class/heap invariants are checked at the native
       boundary before an indexed read. No draw callback may run on failure. */
    for (unsigned invalid=0;invalid<10;invalid++) {
        mc_world_init(&terrain,0); CHECK(mc_client_graph_init(&game,&terrain,"GuardHotbar"));
        CHECK(MCObjectRootScope_begin(&scope,game.heap));
        f=(Fixture *)MCObjectHeap_alloc(game.heap,sizeof *f,&klass); CHECK(f);
        f->player=mc_client_graph_player(&game);
        gui=GuiIngameHotbar_nativeNew(game.heap,(MCObject *)f,(MCObject *)f,(MCObject *)f,&deps); CHECK(gui);
        ItemStackArray *array=f->player->inventory->mainInventory; int32_t index=0;
        MCObjectHeap *foreign=MCObjectHeap_new(1024*1024); CHECK(foreign);
        switch (invalid) {
            case 0: index=-1; break;
            case 1: index=array->length; break;
            case 2: array->length++; break;
            case 3: array=ItemStackArray_new(game.heap,1); CHECK(array); array->length=2; index=1; f->player->inventory->mainInventory=array; break;
            case 4: array->length=-1; break;
            case 5: f->player->inventory->mainInventory=(ItemStackArray *)f; break;
            case 6: f->player->inventory->mainInventory=ItemStackArray_new(foreign,1); CHECK(f->player->inventory->mainInventory); break;
            case 7: array->items[0]=(ItemStack *)f; break;
            case 8: array->items[0]=ItemStack_new(foreign,ItemStack_registryItem(1),1,0); CHECK(array->items[0]); break;
            case 9: f->player->inventory->mainInventory=(ItemStackArray *)MCObjectHeap_alloc(game.heap,sizeof(MCObject),array->object.klass); CHECK(f->player->inventory->mainInventory); break;
        }
        CHECK(!GuiIngame_renderHotbarItem(gui,index,0,0,0,f->player));
        CHECK(MCObjectHeap_failed(game.heap) && f->count==0);
        MCObjectRootScope_end(&scope); CHECK(MCGameplay_free(&game)); MCObjectHeap_free(foreign); mc_world_free(&terrain);
    }
    /* A null overlay receiver is evaluated first, but fails at invocation
       after the font argument has been evaluated. */
    mc_world_init(&terrain,0); CHECK(mc_client_graph_init(&game,&terrain,"NullRenderer"));
    CHECK(MCObjectRootScope_begin(&scope,game.heap));
    f=(Fixture *)MCObjectHeap_alloc(game.heap,sizeof *f,&klass); CHECK(f); f->player=mc_client_graph_player(&game);
    f->stack=ItemStack_new(game.heap,ItemStack_registryItem(1),1,0); CHECK(f->stack); f->player->inventory->mainInventory->items[0]=f->stack;
    f->gui=GuiIngameHotbar_nativeNew(game.heap,(MCObject *)f,(MCObject *)f,(MCObject *)f,&deps); CHECK(f->gui); f->nullRenderer=true;
    CHECK(!GuiIngame_renderHotbarItem(f->gui,0,0,0,0,f->player)); CHECK(MCObjectHeap_failed(game.heap));
    CHECK(f->count==3 && f->events[0]==BODY && f->events[1]==FONT && f->events[2]==OVERLAY);
    MCObjectRootScope_end(&scope); CHECK(MCGameplay_free(&game)); mc_world_free(&terrain);
    printf("source hotbar: %u checks GREEN\n",checks); return 0;
}
