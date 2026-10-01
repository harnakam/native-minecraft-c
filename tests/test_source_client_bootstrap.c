#include "client/network/NetHandlerPlayClient.h"
#include "client/entity/EntityPlayerSP.h"
#include "client/native_runtime.h"
#include "world/WorldProviderHell.h"
#include "world/WorldProviderEnd.h"
#include "util/MCGameplay.h"
#include "item/crafting/CraftingManager.h"
#include "util/MCGameplayCrafting.h"
#include "stats/StatFileWriter.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"client bootstrap %u line%d: %s\n",checks,__LINE__,#x);exit(1); } } while(0)
typedef struct {MCObject object;MCGameplayPlayer *player;unsigned lookups,profileGets;} Controller;
static void trace(MCObject *o,MCObjectVisitor v,void *c) {
    Controller *f=(Controller *)o;f->player=(MCGameplayPlayer *)v((MCObject *)f->player,c);
}
static const MCObjectClass controllerClass={"fixture.client.bootstrap.controller",MCObjectHeap_plainClone,trace,NULL};
static MCPacketThreadResult thread(MCObject *ctx,NetHandlerPlayClient *h,MCObject *packet) {
    (void)ctx;(void)h;(void)packet;return MC_PACKET_THREAD_EXECUTE;
}
static MCGameplayPlayer *player(MCObject *ctx,MCObject *mc) {
    CHECK(ctx==mc);Controller *c=(Controller *)ctx;c->lookups++;return c->player;
}
static bool screen(MCObject *ctx,MCObject *mc) {(void)ctx;(void)mc;return false;}
static int32_t tab(MCObject *ctx,MCObject *mc) {(void)ctx;(void)mc;return 0;}
static int32_t inventory_tab(MCObject *ctx) {(void)ctx;return 0;}
static bool close_screen(MCObject *ctx,MCGameplayPlayer *p) {(void)ctx;(void)p;return false;}
static bool send_packet(MCObject *ctx,NetHandlerPlayClient *h,C0FPacketConfirmTransaction *p) {(void)ctx;(void)h;(void)p;return false;}
static const NetHandlerPlayClientDependencies deps={.checkThreadAndEnqueue=thread,.getPlayer=player,
    .isCreativeScreen=screen,.selectedCreativeTabIndex=tab,.inventoryCreativeTabIndex=inventory_tab,
    .closeScreenAndDropStack=close_screen,.addToSendQueue=send_packet};
static void descriptor_safety_existing_boundary(void) {
    MCGameplay game={0};CHECK(MCGameplay_init(&game,8*1024*1024));
    CraftingManager *manager=CraftingManager_newEmpty(game.heap);CHECK(manager);
    MCGameplayWorld *world=MCGameplayWorld_new(game.heap,MCGameplay_get(&game),NULL,manager);CHECK(world);
    world->isRemote=true;CHECK(MCGameplay_setWorld(&game,(MCObject *)world));
    MCGameplayPlayer *p=MCGameplayPlayer_nativeAllocate(game.heap);CHECK(p);
    p->living.entity.worldObj=(MCObject *)world;
    p->gameProfile=NativeGameProfile_new(game.heap,NULL,NBTString_fromASCII(game.heap,"Bootstrap"));CHECK(p->gameProfile);
    Controller *c=(Controller *)MCObjectHeap_alloc(game.heap,sizeof(*c),&controllerClass);CHECK(c);c->player=p;
    NetHandlerPlayClient *handler=NetHandlerPlayClient_nativeNew(p,(MCObject *)c,(MCObject *)c,&deps);CHECK(handler);
    CHECK(!c->lookups&&NetHandlerPlayClient_getGameProfile(handler)==p->gameProfile);
    MCObject *shortHandler=MCObjectHeap_alloc(game.heap,sizeof(MCObject),handler->object.klass);CHECK(shortHandler);
    CHECK(!NetHandlerPlayClient_isInstance(shortHandler));
    MCObject *shortPlayer=MCObjectHeap_alloc(game.heap,sizeof(MCObject),p->living.entity.object.klass);CHECK(shortPlayer);
    CHECK(!MCGameplayPlayer_isInstance(shortPlayer));
    CHECK(!MCObjectHeap_failed(game.heap));CHECK(MCGameplay_free(&game));
}
static Controller *controller(MCObjectHeap *heap) {
    Controller *c=(Controller *)MCObjectHeap_alloc(heap,sizeof(*c),&controllerClass);CHECK(c);return c;
}
static NativeGameProfile *profile(MCObjectHeap *heap) {
    NativeJavaUUID *id=NativeJavaUUID_new(heap,17,-3);CHECK(id);
    NBTString *name=NBTString_fromASCII(heap,"Profile");CHECK(name);
    NativeGameProfile *p=NativeGameProfile_new(heap,id,name);CHECK(p);return p;
}
static void profile_before_player_lifetime(void) {
    for(unsigned nullable=0;nullable<2;nullable++) {
        MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);Controller *c=controller(heap);
        NativeGameProfile *p=nullable?NULL:profile(heap);
        NetHandlerPlayClient *h=NetHandlerPlayClient_nativeBootstrap(heap,p,(MCObject *)c,(MCObject *)c,&deps);CHECK(h);
        CHECK(!c->player&&!c->lookups&&!h->clientWorldController&&h->profile==p);
        CHECK(NetHandlerPlayClient_getGameProfile(h)==p&&!MCObjectHeap_failed(heap));
        MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,heap,(MCObject *)h));
        CHECK(MCObjectHeap_collect(heap)&&NetHandlerPlayClient_getGameProfile(h)==p);
        if(p)CHECK(NativeGameProfile_getId(p)->mostSignificantBits==17&&NBTString_equalsASCII(NativeGameProfile_getName(p),"Profile"));
        MCObjectHeap *copy=MCObjectHeap_clone(heap);CHECK(copy);MCObjectRoot branch={0};CHECK(MCObjectRoot_rebind(&branch,copy,&root));
        NetHandlerPlayClient *other=(NetHandlerPlayClient *)MCObjectRoot_get(&branch);CHECK(other&&other!=h);
        Controller *cc=(Controller *)other->gameController;CHECK(cc!=(Controller *)h->gameController&&other->dependencyContext==(MCObject *)cc);
        CHECK(!cc->player&&!cc->lookups&&!other->clientWorldController&&other->dependencies==&deps);
        NativeGameProfile *cp=NetHandlerPlayClient_getGameProfile(other);
        CHECK(nullable?cp==NULL:cp&&cp!=p&&cp->id!=p->id&&cp->name!=p->name);
        if(cp)CHECK(cp->object.heap==copy&&cp->id->mostSignificantBits==17&&NBTString_equalsASCII(cp->name,"Profile"));
        CHECK(MCObjectHeap_collect(copy)&&NetHandlerPlayClient_getGameProfile(other)==cp);
        CHECK(MCObjectHeap_adopt(heap,copy));MCObjectHeap_free(copy);
        h=(NetHandlerPlayClient *)MCObjectRoot_get(&root);
        CHECK(h==other&&NetHandlerPlayClient_getGameProfile(h)==cp);
        CHECK(!((Controller *)h->gameController)->lookups);
        CHECK(MCObjectHeap_collect(heap)&&!MCObjectHeap_failed(heap));
        MCObjectRoot_drop(&root);CHECK(MCObjectHeap_collect(heap)&&!MCObjectHeap_liveObjects(heap));
        MCObjectHeap_free(heap);
    }
}
static void invalid_bootstrap_boundaries(void) {
    for(unsigned mode=0;mode<7;mode++) {
        MCObjectHeap *heap=MCObjectHeap_new(1024*1024),*foreign=MCObjectHeap_new(1024*1024);CHECK(heap&&foreign);
        Controller *c=controller(heap),*external=controller(foreign);NativeGameProfile *p=profile(heap),*outside=profile(foreign);
        NativeGameProfile *argument=mode==0?outside:p;
        if(mode==3)argument=(NativeGameProfile *)MCObjectHeap_alloc(heap,sizeof(MCObject),p->object.klass);
        if(mode==4)argument=(NativeGameProfile *)c;
        MCObject *mc=mode==1?(MCObject *)external:mode==5?NULL:(MCObject *)c;
        MCObject *context=mode==2?(MCObject *)external:(MCObject *)c;
        CHECK(!NetHandlerPlayClient_nativeBootstrap(heap,argument,mc,context,mode==6?NULL:&deps));
        CHECK(MCObjectHeap_failed(heap)&&!MCObjectHeap_failed(foreign)&&!c->lookups&&!external->lookups);
        CHECK(!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);MCObjectHeap_free(foreign);
    }
    for(unsigned field=0;field<2;field++) {
        MCObjectHeap *heap=MCObjectHeap_new(1024*1024),*foreign=MCObjectHeap_new(1024*1024);CHECK(heap&&foreign);
        Controller *c=controller(heap);NativeGameProfile *p=profile(heap);
        NetHandlerPlayClient *h=NetHandlerPlayClient_nativeBootstrap(heap,p,(MCObject *)c,(MCObject *)c,&deps);CHECK(h);
        h->profile=field?profile(foreign):(NativeGameProfile *)c;
        CHECK(!NetHandlerPlayClient_getGameProfile(h)&&MCObjectHeap_failed(heap)&&!MCObjectHeap_failed(foreign));
        CHECK(!c->lookups&&!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);MCObjectHeap_free(foreign);
    }
}
static void native_binding_preserves_callback_order(void) {
    for(unsigned mode=0;mode<5;mode++) {
        MCGameplay game={0};CHECK(MCGameplay_init(&game,8*1024*1024));
        CraftingManager *manager=CraftingManager_newEmpty(game.heap);CHECK(manager);
        MCGameplayWorld *w=MCGameplayWorld_new(game.heap,MCGameplay_get(&game),NULL,manager);CHECK(w);w->isRemote=mode!=1;
        MCGameplayPlayer *p=MCGameplayPlayer_nativeAllocate(game.heap);CHECK(p);
        p->living.entity.worldObj=mode==2?NULL:(MCObject *)w;
        Controller *c=controller(game.heap);
        NetHandlerPlayClient *h=NetHandlerPlayClient_nativeBootstrap(game.heap,NULL,(MCObject *)c,(MCObject *)c,&deps);CHECK(h);
        MCObjectHeap *foreign=MCObjectHeap_new(1024*1024);CHECK(foreign);
        MCGameplayPlayer *passed=mode==3?MCGameplayPlayer_nativeAllocate(foreign):mode==4?NULL:p;
        CHECK(NetHandlerPlayClient_nativeBindPlayer(h,passed)==(mode==0));
        CHECK(!c->player&&!c->lookups); /* Binding cannot evaluate dynamic getPlayer. */
        CHECK(mode==0?(h->clientWorldController==w&&p->handler==(MCObject *)h):(!h->clientWorldController&&!p->handler));
        CHECK(MCObjectHeap_failed(game.heap)==(mode!=0)&&!MCObjectHeap_failed(foreign));
        CHECK(!MCObjectHeap_hasBorrowers(game.heap));MCObjectHeap_free(foreign);CHECK(MCGameplay_free(&game));
    }
}
static void attachment_does_not_reconstruct_source_parent(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);
    MCGameplayPlayer *p=MCGameplayPlayer_nativeAllocate(heap);CHECK(p);
    StatFileWriter *stats=StatFileWriter_new(heap);CHECK(stats);
    NBTTagCompound *inventoryWitness=NBTTagCompound_new(heap);CHECK(inventoryWitness);
    /* Allocation-only state deliberately carries constructor sentinels. The
       native environment operation must not run a source constructor/reset. */
    p->sleeping=true;p->speedInAir=7;p->living.entity.posY=42;p->handler=(MCObject *)inventoryWitness;
    p->effects=(MCObject *)inventoryWitness;p->pendingPackets=(MCObject *)inventoryWitness;
    p->savedRootName=NBTString_fromASCII(heap,"SavedName");CHECK(p->savedRootName);
    CHECK(MCGameplayPlayer_nativeAttachEnvironment(p,stats));
    CHECK(p->stats==stats&&p->savedFields&&p->sleeping&&p->speedInAir==7&&p->living.entity.posY==42);
    CHECK(p->handler==(MCObject *)inventoryWitness&&p->effects==p->handler&&p->pendingPackets==p->handler);
    CHECK(!p->inventory&&!p->gameProfile); /* No parent or profile construction. */
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,heap,(MCObject *)p));
    CHECK(MCObjectHeap_collect(heap)&&NBTString_equalsASCII(p->savedRootName,"SavedName"));
    MCObjectHeap *copy=MCObjectHeap_clone(heap);CHECK(copy);MCObjectRoot branch={0};CHECK(MCObjectRoot_rebind(&branch,copy,&root));
    MCGameplayPlayer *other=(MCGameplayPlayer *)MCObjectRoot_get(&branch);CHECK(other&&other!=p);
    CHECK(other->stats!=stats&&other->savedFields!=p->savedFields&&other->savedRootName!=p->savedRootName);
    CHECK(other->handler!=p->handler&&other->effects==other->handler&&other->pendingPackets==other->handler);
    CHECK(other->sleeping&&other->speedInAir==7&&other->living.entity.posY==42&&!other->gameProfile&&!other->inventory);
    CHECK(MCObjectHeap_collect(copy)&&NBTString_equalsASCII(other->savedRootName,"SavedName"));
    MCObjectRoot_drop(&branch);MCObjectHeap_free(copy);
    MCObjectHeap *foreign=MCObjectHeap_new(65536);CHECK(foreign);StatFileWriter *external=StatFileWriter_new(foreign);CHECK(external);
    NBTTagCompound *before=p->savedFields;
    CHECK(!MCGameplayPlayer_nativeAttachEnvironment(p,external)&&MCObjectHeap_failed(heap)&&!MCObjectHeap_failed(foreign));
    CHECK(p->stats==stats&&p->savedFields==before&&p->sleeping&&p->living.entity.posY==42);
    CHECK(!MCObjectHeap_hasBorrowers(heap));MCObjectRoot_drop(&root);MCObjectHeap_free(heap);MCObjectHeap_free(foreign);
    const size_t budget=65536;heap=MCObjectHeap_new(budget);CHECK(heap);p=MCGameplayPlayer_nativeAllocate(heap);CHECK(p);
    before=NBTTagCompound_new(heap);CHECK(before);p->savedFields=before;
    CHECK(MCObjectHeap_alloc(heap,budget-MCObjectHeap_liveBytes(heap),&controllerClass));
    CHECK(!MCGameplayPlayer_nativeAttachEnvironment(p,NULL)&&MCObjectHeap_failed(heap));
    CHECK(!p->stats&&!p->savedFields&&!MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}
static bool unexpected_crafting(ItemStack *s,MCObject *w,MCObject *p,int32_t n) {
    (void)s;(void)w;(void)p;(void)n;CHECK(false);return false;
}
static bool unexpected_achievement(MCObject *p,mc_crafting_achievement a) {
    (void)p;(void)a;CHECK(false);return false;
}
static bool unexpected_drop(MCObject *p,ItemStack *s,bool scatter) {
    (void)p;(void)s;(void)scatter;CHECK(false);return false;
}
static const MCGameplayCraftingEffects craftEffects={
    unexpected_crafting,unexpected_achievement,unexpected_drop
};
static NBTString *registry_display(MCObject *context,const ItemStack *stack) {
    (void)context;
    const char *name=ItemStack_registryResourceName(stack->item);
    if(!name){MCObjectHeap_fail(stack->object.heap);return NULL;}
    return NBTString_fromASCII(stack->object.heap,name);
}
static NativeGameProfile *source_profile(MCObject *context,NetHandlerPlayClient *handler) {
    MCGameplayPlayer *p=(MCGameplayPlayer *)context;
    CHECK(MCGameplayPlayer_isInstance(context)&&EntityPlayerSP_isInstance(context));
    Controller *c=(Controller *)p->effects;
    CHECK(c&&c->player==p&&handler->gameController==(MCObject *)c);
    /* The source super argument is evaluated before any parent constructor. */
    CHECK(!p->inventory&&!p->capabilities&&!p->living.entity.dataWatcher&&!p->living.entity.rand);
    CHECK(!p->gameProfile&&!p->living.entity.worldObj&&!c->lookups);
    c->profileGets++;
    return NetHandlerPlayClient_getGameProfile(handler);
}
static void check_source_aliases(NetHandlerPlayClient *h,MCObjectHeap *heap) {
    Controller *c=(Controller *)h->gameController;
    EntityPlayerSP *sp=(EntityPlayerSP *)c->player;
    MCGameplayPlayer *p=EntityPlayerSP_asPlayer(sp);
    CHECK(EntityPlayerSP_asObject(sp)==(MCObject *)p&&(void *)&p->living==(void *)p&&
        (void *)&p->living.entity==(void *)p);
    CHECK(EntityPlayerSP_isInstance((MCObject *)p)&&AbstractClientPlayer_isInstance((MCObject *)p)&&
        MCGameplayPlayer_isInstance((MCObject *)p)&&EntityLivingBase_isInstance((MCObject *)p)&&Entity_isInstance((MCObject *)p));
    CHECK(((MCObject *)p)->heap==heap&&sp->sendQueue==h&&sp->mc==(MCObject *)c&&p->handler==(MCObject *)h);
    CHECK(p->gameProfile==h->profile&&NetHandlerPlayClient_getGameProfile(h)==p->gameProfile);
    CHECK(p->living.entity.entityUniqueID==p->gameProfile->id);
    CHECK(p->inventory&&p->inventory->player==(MCObject *)p&&p->inventoryContainer==p->openContainer);
    ContainerPlayer *container=(ContainerPlayer *)p->inventoryContainer;
    CHECK(container->thePlayer==(MCObject *)p&&!container->isLocalWorld&&
        container->craftMatrix->eventHandler==(MCObject *)container);
    CHECK(p->living.entity.entityContext==(MCObject *)p&&p->living.livingContext==(MCObject *)p&&p->playerContext==(MCObject *)p);
    CHECK(p->effects==(MCObject *)c&&sp->clientPlayer.playerInfo==p->pendingPackets&&p->pendingPackets==(MCObject *)p->savedFields);
    CHECK(sp->statWriter==p->stats&&NBTString_equalsASCII(sp->clientBrand,"NativeClient"));
    CHECK(sp->movementInput&&sp->movementInput->sneak&&NBTString_equalsASCII(p->savedRootName,"NativeSaved"));
    CHECK(h->clientWorldController==(MCGameplayWorld *)p->living.entity.worldObj&&h->clientWorldController->isRemote);
    CHECK(!c->lookups&&c->profileGets==1&&!MCObjectHeap_failed(heap));
}
static void bootstrap_actual_source_subtype(void) {
    MCGameplay game={0};CHECK(MCGameplay_init(&game,8*1024*1024));
    CraftingManager *manager=CraftingManager_newEmpty(game.heap);CHECK(manager);
    MCGameplayWorld *world=MCGameplayWorld_new(game.heap,MCGameplay_get(&game),NULL,manager);CHECK(world);
    world->isRemote=true;world->provider->dimensionId=7;world->itemDisplayName=registry_display;
    CHECK(MCGameplay_setWorld(&game,(MCObject *)world));
    Controller *c=controller(game.heap);
    NetHandlerPlayClient *h=NetHandlerPlayClient_nativeBootstrap(game.heap,profile(game.heap),(MCObject *)c,(MCObject *)c,&deps);CHECK(h);
    MCObjectRoot handlerRoot={0};CHECK(MCObjectRoot_init(&handlerRoot,game.heap,(MCObject *)h));
    CHECK(MCObjectHeap_collect(game.heap)&&!c->player&&!c->lookups&&!h->clientWorldController);
    CHECK(NetHandlerPlayClient_getGameProfile(h)&&NBTString_equalsASCII(h->profile->name,"Profile"));
    EntityPlayerSP *sp=EntityPlayerSP_nativeAllocate(game.heap);CHECK(sp);
    MCGameplayPlayer *p=EntityPlayerSP_asPlayer(sp);c->player=p;p->effects=(MCObject *)c;
    StatFileWriter *stats=StatFileWriter_new(game.heap);CHECK(stats);
    mc_crafting_dispatch crafting;CHECK(MCGameplayCrafting_nativeDispatch(&crafting,&craftEffects));
    const EntityPlayerSPConstructorDependencies constructorDeps={
        MCGameplayPlayer_nativeConstructorDependencies(),source_profile
    };
    CHECK(EntityPlayerSP_construct(sp,(MCObject *)c,(MCObject *)world,h,stats,&constructorDeps,&crafting,
        (MCObject *)p,world->randomRuntime,NativeEntityIDRuntime_process()));
    CHECK(sp->sendQueue==h&&sp->mc==(MCObject *)c&&sp->statWriter==stats&&!p->handler&&!p->stats&&!p->savedFields);
    CHECK(p->living.entity.dimension==0&&c->profileGets==1&&!c->lookups);
    CHECK(MCGameplayPlayer_nativeAttachEnvironment(p,stats)&&NetHandlerPlayClient_nativeBindPlayer(h,p));
    p->pendingPackets=(MCObject *)p->savedFields;sp->clientPlayer.playerInfo=p->pendingPackets;
    p->savedRootName=NBTString_fromASCII(game.heap,"NativeSaved");CHECK(p->savedRootName);
    sp->clientBrand=NBTString_fromASCII(game.heap,"NativeClient");CHECK(sp->clientBrand);
    sp->movementInput=MovementInput_new(game.heap);CHECK(sp->movementInput);sp->movementInput->sneak=true;
    check_source_aliases(h,game.heap);
    CHECK(MCObjectHeap_collect(game.heap));check_source_aliases(h,game.heap);
    MCObjectHeap *copy=MCObjectHeap_clone(game.heap);CHECK(copy);
    MCObjectRoot branch={0};CHECK(MCObjectRoot_rebind(&branch,copy,&handlerRoot));
    NetHandlerPlayClient *other=(NetHandlerPlayClient *)MCObjectRoot_get(&branch);CHECK(other&&other!=h);
    CHECK(other->profile!=h->profile&&((Controller *)other->gameController)->player!=p);
    check_source_aliases(other,copy);CHECK(MCObjectHeap_collect(copy));check_source_aliases(other,copy);
    CHECK(MCObjectHeap_adopt(game.heap,copy));MCObjectHeap_free(copy);
    /* Reacquire every managed reference after adoption; no old address oracle. */
    h=(NetHandlerPlayClient *)MCObjectRoot_get(&handlerRoot);
    check_source_aliases(h,game.heap);CHECK(MCObjectHeap_collect(game.heap));check_source_aliases(h,game.heap);
    c=(Controller *)h->gameController;p=c->player;
    ItemStack *sent=ItemStack_new(game.heap,ItemStack_registryItem(1),-3,2);CHECK(sent);
    S2FPacketSetSlot *packet=S2FPacketSetSlot_new(game.heap,-1,-1,sent);CHECK(packet);
    CHECK(NetHandlerPlayClient_handleSetSlot(h,packet));
    CHECK(c->lookups==1&&p->inventory->itemStack==packet->item&&p->inventory->itemStack!=sent&&p->inventory->itemStack->stackSize==-3);
    CHECK(((EntityPlayerSP *)p)->clientPlayer.player.inventory==p->inventory&&MCGameplayPlayer_inventory((MCObject *)p)==p->inventory);
    MCObject *shortSubtype=MCObjectHeap_alloc(game.heap,sizeof(MCGameplayPlayer),((MCObject *)p)->klass);CHECK(shortSubtype);
    CHECK(!EntityPlayerSP_isInstance(shortSubtype)&&!MCGameplayPlayer_isInstance(shortSubtype)&&
        !EntityLivingBase_isInstance(shortSubtype)&&!Entity_isInstance(shortSubtype));
    CHECK(!MCObjectHeap_failed(game.heap)&&!MCObjectHeap_hasBorrowers(game.heap));
    MCObjectRoot_drop(&handlerRoot);CHECK(MCGameplay_free(&game));
}
static void nullable_profile_reaches_source_parent_failure(void) {
    MCGameplay game={0};CHECK(MCGameplay_init(&game,8*1024*1024));
    CraftingManager *manager=CraftingManager_newEmpty(game.heap);CHECK(manager);
    MCGameplayWorld *world=MCGameplayWorld_new(game.heap,MCGameplay_get(&game),NULL,manager);CHECK(world);world->isRemote=true;
    Controller *c=controller(game.heap);
    NetHandlerPlayClient *h=NetHandlerPlayClient_nativeBootstrap(game.heap,NULL,(MCObject *)c,(MCObject *)c,&deps);CHECK(h);
    CHECK(!NetHandlerPlayClient_getGameProfile(h)&&!MCObjectHeap_failed(game.heap));
    EntityPlayerSP *sp=EntityPlayerSP_nativeAllocate(game.heap);CHECK(sp);
    MCGameplayPlayer *p=EntityPlayerSP_asPlayer(sp);c->player=p;p->effects=(MCObject *)c;
    mc_crafting_dispatch crafting;CHECK(MCGameplayCrafting_nativeDispatch(&crafting,&craftEffects));
    const EntityPlayerSPConstructorDependencies constructorDeps={
        MCGameplayPlayer_nativeConstructorDependencies(),source_profile
    };
    CHECK(!EntityPlayerSP_construct(sp,(MCObject *)c,(MCObject *)world,h,NULL,&constructorDeps,&crafting,
        (MCObject *)p,world->randomRuntime,NativeEntityIDRuntime_process()));
    CHECK(MCObjectHeap_failed(game.heap)&&!MCObjectHeap_hasBorrowers(game.heap));
    CHECK(c->profileGets==1&&!c->lookups&&!sp->sendQueue&&!sp->statWriter&&!sp->mc);
    CHECK(p->living.entity.worldObj==(MCObject *)world&&p->living.entity.rand&&p->living.entity.entityUniqueID&&
        p->living.entity.dataWatcher&&p->inventory&&p->capabilities&&p->foodStats&&p->theInventoryEnderChest);
    CHECK(!p->gameProfile&&!p->inventoryContainer&&!p->handler&&!p->savedFields&&!p->stats);
    CHECK(MCGameplay_free(&game));
}
static void native_dimension_import_preserves_canonical_graph(void) {
    /* This is the real native received-dimension adapter, not a claimed
       WorldClient constructor or full respawn-handler translation. */
    mc_world terrain;mc_world_init(&terrain,123);
    MCGameplay game={0};CHECK(mc_client_graph_init(&game,&terrain,"DimensionWitness"));
    MCGameplayObjects *owners=MCGameplay_get(&game);
    MCGameplayPlayer *p=mc_client_graph_player(&game);
    MCClientBindings *bindings=mc_client_graph_bindings(&game);
    CHECK(p&&bindings&&bindings->player==p&&EntityPlayerSP_asPlayer(bindings->sp)==p);
    NetHandlerPlayClient *handler=(NetHandlerPlayClient *)p->handler;
    World *old=mc_client_graph_world(&game);CHECK(old&&handler&&old->isRemote);
    Scoreboard *scoreboard=World_getScoreboard(old);
    InventoryPlayer *inventory=p->inventory;NativeJavaUUID *uuid=p->living.entity.entityUniqueID;
    NativeJavaRandom *random=p->living.entity.rand;int32_t entityId=Entity_getEntityId(&p->living.entity);
    CHECK(NativeReferenceList_size(old->loadedEntityList)==1&&
          NativeReferenceList_get(old->loadedEntityList,0)==(MCObject *)p&&
          NativeReferenceList_get(old->playerEntities,0)==(MCObject *)p&&
          World_getEntityByID(old,entityId)==&p->living.entity);
    const int32_t dimensions[]={-1,1};
    for(size_t i=0;i<2;i++) {
        MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,game.heap));
        old=mc_client_graph_world(&game);
        CHECK(mc_client_graph_nativeImportDimension(&game,dimensions[i],&WorldSettingsGameType_ADVENTURE));
        World *current=mc_client_graph_world(&game);CHECK(current&&current!=old&&current->isRemote);
        CHECK(WorldProvider_getDimensionId(current->provider)==dimensions[i]&&
              (i==0?WorldProviderHell_isInstance((MCObject *)current->provider):WorldProviderEnd_isInstance((MCObject *)current->provider)));
        CHECK(p==mc_client_graph_player(&game)&&bindings==mc_client_graph_bindings(&game)&&
              owners==MCGameplay_get(&game)&&p->living.entity.worldObj==(MCObject *)current&&
              handler->clientWorldController==current&&bindings->sp->sendQueue==handler);
        CHECK(p->living.entity.dimension==dimensions[i]&&p->inventory==inventory&&
              p->living.entity.entityUniqueID==uuid&&p->living.entity.rand==random&&
              Entity_getEntityId(&p->living.entity)==entityId&&World_getScoreboard(current)==scoreboard);
        CHECK(NativeReferenceList_size(old->loadedEntityList)==0&&
              NativeReferenceList_size(old->playerEntities)==0&&!World_getEntityByID(old,entityId));
        CHECK(NativeReferenceList_size(current->loadedEntityList)==1&&
              NativeReferenceList_get(current->loadedEntityList,0)==(MCObject *)p&&
              NativeReferenceList_get(current->playerEntities,0)==(MCObject *)p&&
              World_getEntityByID(current,entityId)==&p->living.entity&&MCGameplay_validateWorldIndexes(owners));
        CHECK(WorldInfo_getGameType(current->worldInfo)==&WorldSettingsGameType_ADVENTURE);
        uint32_t mod=current->loadedEntityList->modCount;
        IntHashMapEntry *entry=IntHashMap_lookupEntry(current->entitiesById,entityId);
        size_t live=MCObjectHeap_liveObjects(game.heap);
        CHECK(mc_client_graph_nativeImportDimension(&game,dimensions[i],&WorldSettingsGameType_SPECTATOR));
        CHECK(mc_client_graph_world(&game)==current&&WorldInfo_getGameType(current->worldInfo)==&WorldSettingsGameType_SPECTATOR&&
              current->loadedEntityList->modCount==mod&&IntHashMap_lookupEntry(current->entitiesById,entityId)==entry&&
              MCObjectHeap_liveObjects(game.heap)==live);
        MCObjectRootScope_end(&scope);CHECK(MCObjectHeap_collect(game.heap));
        CHECK(mc_client_graph_player(&game)==p&&World_getScoreboard(current)==scoreboard&&
              World_getEntityByID(current,entityId)==&p->living.entity&&MCGameplay_validateWorldIndexes(owners));
    }
    /* Snapshot the actual live graph, preserving its aliases and retained
       scoreboard, then adopt and reacquire every managed reference. */
    MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&game,&tx));
    MCGameplayObjects *branch=MCGameplay_get(&tx.working);World *cw=(World *)branch->world;
    MCGameplayPlayer *cp=mc_client_graph_player(&tx.working);
    MCClientBindings *cb=mc_client_graph_bindings(&tx.working);
    NetHandlerPlayClient *ch=(NetHandlerPlayClient *)cp->handler;
    CHECK(cp!=p&&cw!=mc_client_graph_world(&game)&&cb!=bindings&&cb->player==cp&&
          EntityPlayerSP_asPlayer(cb->sp)==cp&&ch->clientWorldController==cw&&cp->living.entity.worldObj==(MCObject *)cw);
    CHECK(cw->worldScoreboard!=scoreboard&&cw->worldScoreboard->object.heap==tx.working.heap&&
          World_getEntityByID(cw,entityId)==&cp->living.entity&&
          NativeReferenceList_get(cw->loadedEntityList,0)==(MCObject *)cp&&MCGameplay_validateWorldIndexes(branch));
    CHECK(MCObjectHeap_collect(tx.working.heap)&&MCGameplay_validateWorldIndexes(branch));
    CHECK(MCObjectHeap_adopt(game.heap,tx.working.heap));CHECK(MCGameplay_abort(&tx));
    owners=MCGameplay_get(&game);p=mc_client_graph_player(&game);bindings=mc_client_graph_bindings(&game);
    World *current=mc_client_graph_world(&game);handler=(NetHandlerPlayClient *)p->handler;
    CHECK(p==cp&&current==cw&&bindings==cb&&handler==ch&&bindings->player==p&&
          handler->clientWorldController==current&&World_getEntityByID(current,entityId)==&p->living.entity);
    CHECK(MCObjectHeap_collect(game.heap)&&MCGameplay_validateWorldIndexes(owners));
    CHECK(MCGameplay_begin(&game,&tx));branch=MCGameplay_get(&tx.working);
    cp=mc_client_graph_player(&tx.working);cw=mc_client_graph_world(&tx.working);ch=(NetHandlerPlayClient *)cp->handler;
    CHECK(!mc_client_graph_nativeImportDimension(&tx.working,123456,&WorldSettingsGameType_SURVIVAL)&&MCObjectHeap_failed(tx.working.heap));
    CHECK(branch->world==(MCObject *)cw&&cp->living.entity.worldObj==(MCObject *)cw&&
          ch->clientWorldController==cw&&cp->living.entity.dimension==1&&World_getEntityByID(current,entityId)==&p->living.entity);
    CHECK(MCGameplay_abort(&tx)&&MCGameplay_get(&game)==owners&&!MCObjectHeap_failed(game.heap)&&
          MCGameplay_validateWorldIndexes(owners));
    CHECK(MCGameplay_free(&game));mc_world_free(&terrain);
}
int main(void) {
    descriptor_safety_existing_boundary();
    profile_before_player_lifetime();
    invalid_bootstrap_boundaries();
    native_binding_preserves_callback_order();
    attachment_does_not_reconstruct_source_parent();
    bootstrap_actual_source_subtype();
    nullable_profile_reaches_source_parent_failure();
    native_dimension_import_preserves_canonical_graph();
    printf("source client bootstrap: %u checks passed\n",checks);return 0;
}
