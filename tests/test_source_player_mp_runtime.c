/* Exercise the actual native server provider seams and Source MP receiver.
   The executable core resolves these included bindings once, as with the
   existing actual-server lifetime fixture. No terrain/collision callback mock. */
#include "../src/minecraft/net/minecraft/server/native_gameplay.c"
#include "util/CombatTracker.h"
#include <stdlib.h>
#include <time.h>
static unsigned checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"MP runtime check %u line %d: %s\n",checks,__LINE__,#x);exit(1);}} while(0)
static void terrain_init(mc_world *terrain,bool blocks) {
    mc_world_init(terrain,919);
    for(int z=-1;z<=1;z++)for(int x=-1;x<=1;x++) {
        mc_chunk *chunk=mc_world_chunk(terrain,x,z,true);CHECK(chunk);
        if(blocks)for(int lz=0;lz<16;lz++)for(int lx=0;lx<16;lx++) {
            CHECK(mc_world_set(terrain,x*16+lx,5,z*16+lz,1<<4));
            CHECK(mc_world_set(terrain,x*16+lx,30,z*16+lz,18<<4));
            CHECK(mc_world_set(terrain,x*16+lx,40,z*16+lz,9<<4));
        }
    }
}
static void same_player(MCGameplay *game,EntityPlayerMP *mp) {
    MCGameplayPlayer *p=&mp->player;MCGameplayWorld *world=mc_server_graph_world(game);
    RuntimeWorld *r=bindings(world);CHECK(r);
    CHECK(EntityPlayerMP_isInstance((MCObject *)mp)&&MCGameplayPlayer_isInstance((MCObject *)mp));
    CHECK(EntityPlayerMP_asPlayer(mp)==p&&(MCObject *)&p->living.entity==(MCObject *)mp);
    CHECK(p->inventory->player==(MCObject *)p&&p->living._combatTracker->fighter==&p->living);
    CHECK(mp->theItemInWorldManager->thisPlayerMP==mp&&mp->theItemInWorldManager->theWorld==(MCObject *)world);
    CHECK(mp->constructorContext==(MCObject *)mp&&p->playerContext==(MCObject *)p);
    CHECK(mp->mcServer==(MCObject *)r->server&&mp->statsFile->mcServer==(MCObject *)r->server);
    CHECK(!p->stats&&!p->handler&&!p->isChangingQuantityOnly);
    CHECK(MCGameplayPlayer_statFile(p)==(StatFileWriter *)mp->statsFile);
    CHECK(MCGameplayPlayer_handler(p)==(MCObject *)mp->playerNetServerHandler);
    CHECK(mp->playerNetServerHandler->playerEntity==p);
    CHECK(NativeReferenceList_isInstance(mp->loadedChunks)&&NativeReferenceList_isInstance(mp->destroyedItemsNetCache));
    CHECK(mp->loadedChunks!=mp->destroyedItemsNetCache);
    CHECK(mp_statistics((MCObject *)mp,(MCObject *)r->server->configuration,mp)==mp->statsFile);
    CHECK(NativeReferenceList_size(r->server->configuration->playerStatFiles)==1);
    CHECK(mp->loadedChunks->heap==game->heap&&mp->statsFile->base.object.heap==game->heap);
}
static void construction_and_adoption(void) {
    mc_world terrain;terrain_init(&terrain,true);MCGameplay game={0};CHECK(mc_server_graph_init(&game,&terrain,0,0,919));
    CHECK(mc_server_graph_set_world_game_type(&game,&WorldSettingsGameType_CREATIVE));
    MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&game,&tx));
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));
    int64_t before=(int64_t)time(NULL)*1000;
    CHECK(mc_server_graph_create_player(&tx.working,0,"00000000-0000-0000-0000-000000000001","SourceMP",true));
    int64_t after=(int64_t)time(NULL)*1000+999;
    EntityPlayerMP *mp=(EntityPlayerMP *)mc_server_graph_player(&tx.working,0);CHECK(mp);
    same_player(&tx.working,mp);
    Entity *e=&mp->player.living.entity;
    CHECK(e->posX>=-9.5&&e->posX<=9.5&&e->posZ>=-9.5&&e->posZ<=9.5&&e->posY==6);
    CHECK(e->stepHeight==0&&e->boundingBox->minY==6);
    CHECK(mp->playerLastActiveTime>=before&&mp->playerLastActiveTime<=after);
    mc_server_graph_world(&tx.working)->worldTime=7;
    CHECK(active(NULL,&mp->player)&&mp->playerLastActiveTime!=7);
    CHECK(ItemInWorldManager_isCreative(mp->theItemInWorldManager)&&mp->player.capabilities->isCreativeMode);
    CHECK(MCGameplayPlayer_setChangingQuantityOnly(&mp->player,true));
    CHECK(mp->isChangingQuantityOnly&&!mp->player.isChangingQuantityOnly);
    CHECK(MCGameplayPlayer_setChangingQuantityOnly(&mp->player,false));
    mp->currentWindowId=99;
    CHECK(mc_server_graph_open_workbench(&mp->player,0,5,0));
    CHECK(mp->currentWindowId==100&&mp->player.openContainer->windowId==100);
    CHECK(mc_server_graph_close(&mp->player,false));
    CHECK(mc_server_graph_open_workbench(&mp->player,0,5,0));
    CHECK(mp->currentWindowId==1&&mp->player.openContainer->windowId==1);
    CHECK(mc_server_graph_close(&mp->player,false));
    mp->spectatingEntity=e;mp->chatVisibility=(MCObject *)mp->player.inventory;
    CHECK(NativeReferenceList_add((NativeReferenceList *)mp->loadedChunks,(MCObject *)mp));
    CHECK(NativeReferenceList_add((NativeReferenceList *)mp->destroyedItemsNetCache,(MCObject *)mp->statsFile));
    MCObjectRootScope_end(&scope);
    CHECK(MCObjectHeap_adopt(game.heap,tx.working.heap));CHECK(MCGameplay_abort(&tx));
    CHECK(MCObjectHeap_collect(game.heap));
    mp=(EntityPlayerMP *)mc_server_graph_player(&game,0);same_player(&game,mp);
    CHECK(mp->spectatingEntity==&mp->player.living.entity&&mp->chatVisibility==(MCObject *)mp->player.inventory);
    CHECK(NativeReferenceList_get((NativeReferenceList *)mp->loadedChunks,0)==(MCObject *)mp);
    CHECK(NativeReferenceList_get((NativeReferenceList *)mp->destroyedItemsNetCache,0)==(MCObject *)mp->statsFile);
    CHECK(MCGameplay_begin(&game,&tx));
    EntityPlayerMP *copy=(EntityPlayerMP *)mc_server_graph_player(&tx.working,0);CHECK(copy!=mp);
    same_player(&tx.working,copy);
    CHECK(copy->spectatingEntity==&copy->player.living.entity&&copy->statsFile!=mp->statsFile);
    CHECK(NativeReferenceList_get((NativeReferenceList *)copy->loadedChunks,0)==(MCObject *)copy);
    CHECK(MCGameplay_abort(&tx));CHECK(MCGameplay_free(&game));mc_world_free(&terrain);
}
static void actual_terrain_dependencies(void) {
    mc_world terrain;terrain_init(&terrain,true);MCGameplay game={0};CHECK(mc_server_graph_init(&game,&terrain,0,0,0));
    MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&game,&tx));
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));
    MCGameplayWorld *world=mc_server_graph_world(&tx.working);RuntimeWorld *r=bindings(world);CHECK(r);
    CHECK(mc_server_graph_create_player(&tx.working,0,"00000000-0000-0000-0000-000000000001","Collisions",false));
    EntityPlayerMP *mp=(EntityPlayerMP *)mc_server_graph_player(&tx.working,0);
    CHECK(mp->player.living.entity.posY==6);
    CHECK(Entity_setPosition(&mp->player.living.entity,.5,5,.5));
    MCObject *list=mp_collisions((MCObject *)mp,(MCObject *)world,mp,mp_box(NULL,mp));CHECK(list);
    CHECK(NativeReferenceList_size((NativeReferenceList *)list)>0);
    CHECK(Entity_setPosition(&mp->player.living.entity,.5,6,.5));
    list=mp_collisions((MCObject *)mp,(MCObject *)world,mp,mp_box(NULL,mp));CHECK(list);
    CHECK(NativeReferenceList_size((NativeReferenceList *)list)==0);
    BlockPos *pos=DataWatcher_blockPos(tx.working.heap,0,99,0);CHECK(pos);
    CHECK(mp_top_solid(NULL,(MCObject *)world,pos)->y==6);
    CHECK(WorldBorder_setCenter(r->border,0,0)&&WorldBorder_setTransition(r->border,4));
    double distance;CHECK(mp_border_distance(NULL,(MCObject *)r->border,0,0,&distance)&&distance==2);
    mp->theItemInWorldManager->gameType=&WorldSettingsGameType_SPECTATOR;
    CHECK(manager_spectator(mp->player.effects)&&!manager_creative(mp->player.effects));
    mp->player.itemInUse=InventoryPlayer_getCurrentItem(mp->player.inventory);
    CHECK(!using_item(mp->player.effects,&mp->player));
    mp->player.itemInUse=ItemStack_new(tx.working.heap,ItemStack_registryItem(1),1,0);CHECK(mp->player.itemInUse);
    CHECK(using_item(mp->player.effects,&mp->player));
    MCObjectRootScope_end(&scope);CHECK(MCGameplay_abort(&tx));CHECK(MCGameplay_free(&game));mc_world_free(&terrain);
    mc_world_init(&terrain,0);CHECK(mc_server_graph_init(&game,&terrain,0,0,0));CHECK(MCGameplay_begin(&game,&tx));
    CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));
    CHECK(!mc_server_graph_create_player(&tx.working,0,"00000000-0000-0000-0000-000000000001","MissingChunk",false));
    CHECK(MCObjectHeap_failed(tx.working.heap));
    MCObjectRootScope_end(&scope);CHECK(MCGameplay_abort(&tx));CHECK(MCGameplay_free(&game));mc_world_free(&terrain);
}
int main(void) {
    construction_and_adoption();actual_terrain_dependencies();
    printf("Source MP live runtime: %u checks passed\n",checks);return 0;
}
