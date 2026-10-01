/* Original native regression fixture for the actual server's frame seams.
   It links the real core and includes the executable entrypoint exactly once. */
#define main source_server_program_main
#include "../src/minecraft/net/minecraft/server/server.c"
#undef main
static unsigned checks;
#define CHECK(condition) do { ++checks;if(!(condition)) {fprintf(stderr,"Server lifetime check %u line %d: %s\n",checks,__LINE__,#condition);exit(1);} } while(0)

static mc_server *fixture(void) {
    mc_server *server=(mc_server *)calloc(1,sizeof(*server));CHECK(server);
    mc_world_init(&server->world,919);
    for(int z=-1;z<=1;z++)for(int x=-1;x<=1;x++)CHECK(mc_world_chunk(&server->world,x,z,true));
    CHECK(mc_server_graph_init(&server->gameplay,&server->world,0,0,1));
    MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&server->gameplay,&tx));
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));
    CHECK(mc_server_graph_add_player_auto(&tx.working,0,"00000000-0000-0000-0000-000000000001","Alice",.5,80,.5,true));
    MCObjectRootScope_end(&scope);
    /* No save or socket effect belongs to this fixture. Adopt the completed
       same-lineage setup through the real heap fence, then retain its handle. */
    CHECK(MCObjectHeap_adopt(server->gameplay.heap,tx.working.heap));MCGameplay_abort(&tx);
    server_peer *peer=&server->peers[0];peer->game=&server->gameplay;peer->ownerIndex=0;
    peer->x=.5;peer->y=80;peer->z=.5;peer->entity=actor(peer)->living.entity.entityId;
    mc_conn_init(&peer->conn,MC_INVALID_SOCKET);return server;
}
static void fixture_free(mc_server *server) {
    mc_conn_close(&server->peers[0].conn);free_world_state(server);
}
static void bounded_movement_and_owner_roots(void) {
    mc_server *server=fixture();server_peer *peer=&server->peers[0];
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,server->gameplay.heap));
    MCGameplayPlayer *player=actor(peer);ItemStack *shared=ItemStack_new(server->gameplay.heap,ItemStack_registryItem(1),64,0);CHECK(shared);
    CHECK(InventoryPlayer_setInventorySlotContents(player->inventory,0,shared));
    CHECK(InventoryPlayer_setInventorySlotContents(player->inventory,1,shared));
    CHECK(InventoryPlayer_setItemStack(player->inventory,shared));
    CHECK(InventoryBasic_setInventorySlotContents(&player->theInventoryEnderChest->basic,3,shared));
    player->living.previousEquipment->items[0]=shared;MCObjectHeap_touch(server->gameplay.heap);
    MCObjectRoot shared_root={0},player_root={0};CHECK(MCObjectRoot_init(&shared_root,server->gameplay.heap,(MCObject *)shared));
    CHECK(MCObjectRoot_init(&player_root,server->gameplay.heap,(MCObject *)player));MCObjectRootScope_end(&scope);
    CHECK(MCObjectHeap_collect(server->gameplay.heap));
    size_t baseline=MCObjectHeap_liveObjects(server->gameplay.heap),bytes=MCObjectHeap_liveBytes(server->gameplay.heap);
    uint64_t last_collection=0;
    for(unsigned i=1;i<=5000;i++) {
        mc_buf packet;mc_buf_init(&packet);mc_put_f64(&packet,.5);mc_put_f64(&packet,80);mc_put_f64(&packet,.5);mc_put_u8(&packet,1);
        handle_movement(server,peer,&packet,4);mc_buf_free(&packet);
        CHECK(!peer->closing&&!MCObjectHeap_failed(server->gameplay.heap));CHECK(advance_idle_tick(server)==1);
        CHECK(collect_gameplay_if_due(server,(uint64_t)i*20,&last_collection));
        /* No item/map transaction occurs to incidentally discard garbage.
           Source setPosition must remain active, while native GC bounds it. */
        CHECK(MCObjectHeap_liveObjects(server->gameplay.heap)<=baseline+50);
    }
    CHECK(last_collection==100000);CHECK(MCObjectHeap_liveObjects(server->gameplay.heap)==baseline);
    CHECK(MCObjectHeap_liveBytes(server->gameplay.heap)==bytes);
    player=actor(peer);shared=(ItemStack *)MCObjectRoot_get(&shared_root);
    CHECK((MCObject *)player==MCObjectRoot_get(&player_root));CHECK(shared&&shared->stackSize==64);
    CHECK(player->inventory->mainInventory->items[0]==shared&&player->inventory->mainInventory->items[1]==shared);
    CHECK(InventoryPlayer_getItemStack(player->inventory)==shared);
    CHECK(InventoryBasic_getStackInSlot(&player->theInventoryEnderChest->basic,3)==shared);
    CHECK(player->living.previousEquipment->items[0]==shared);
    AxisAlignedBB *box=Entity_getEntityBoundingBox(&player->living.entity);CHECK(box);
    CHECK(box->minX==.5-(double)(player->living.entity.width/2.0f)&&box->minY==80);
    CHECK(box->maxY==80+(double)player->living.entity.height&&box->maxZ==.5+(double)(player->living.entity.width/2.0f));
    CHECK(EntityPlayer_getName(player)==player->gameProfile->name&&EntityPlayer_getUUID(player->gameProfile)==player->living.entity.entityUniqueID);
    CHECK(!server->fatal&&!MCObjectHeap_hasBorrowers(server->gameplay.heap));fixture_free(server);
}
static void guard_failure_and_interval(void) {
    mc_server *server=fixture();uint64_t last_collection=1000;
    CHECK(collect_gameplay_if_due(server,1999,&last_collection));CHECK(last_collection==1000);
    CHECK(collect_gameplay_if_due(server,2000,&last_collection));CHECK(last_collection==2000);
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,server->gameplay.heap));
    CHECK(!collect_gameplay_if_due(server,3000,&last_collection));CHECK(server->fatal&&last_collection==2000);
    CHECK(!MCObjectHeap_failed(server->gameplay.heap));MCObjectRootScope_end(&scope);fixture_free(server);
    server=fixture();last_collection=0;MCObjectHeap_fail(server->gameplay.heap);
    CHECK(!collect_gameplay_if_due(server,1000,&last_collection));CHECK(server->fatal&&last_collection==0);
    fixture_free(server);
}
int main(void) {
    bounded_movement_and_owner_roots();guard_failure_and_interval();
    printf("Server native lifetime: %u checks passed\n",checks);return 0;
}
