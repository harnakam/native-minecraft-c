#include "entity/DataWatcher.h"
#include "util/MCGameplayWorld.h"
#include "block/block.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"Native world view check %u line %d: %s\n",checks,__LINE__,#x);exit(1); } } while(0)

static void dense_metadata(void) {
    mc_world *world=(mc_world *)malloc(sizeof(*world));CHECK(world);
    mc_world_init(world,919);
    CHECK(mc_world_set(world,-1,5,-17,1u<<4));
    mc_chunk *chunk=mc_world_chunk(world,-1,-2,false);CHECK(chunk);
    CHECK(chunk->heightMap[255]==6&&chunk->sectionMask==1);
    int32_t opacity=-1;
    CHECK(mc_block_light_opacity(20u<<4,&opacity)&&opacity==0);
    CHECK(mc_world_set(world,-1,250,-17,20u<<4));
    CHECK(chunk->heightMap[255]==6&&chunk->sectionMask==UINT16_C(0x8001));
    CHECK(mc_world_set(world,-1,250,-17,0));
    CHECK(chunk->heightMap[255]==6&&chunk->sectionMask==UINT16_C(0x8001));
    CHECK(mc_world_set(world,-1,200,-17,18u<<4));
    CHECK(chunk->heightMap[255]==201&&chunk->sectionMask==UINT16_C(0x9001));
    CHECK(mc_world_set(world,-1,255,-17,1u<<4));CHECK(chunk->heightMap[255]==256);
    CHECK(mc_world_set(world,-1,255,-17,0));CHECK(chunk->heightMap[255]==201);
    CHECK(mc_world_set(world,-1,210,-17,9u<<4));CHECK(chunk->heightMap[255]==211);
    unsigned revision=chunk->revision;
    CHECK(!mc_world_set(world,-1,210,-17,198u<<4));
    CHECK(chunk->revision==revision&&chunk->heightMap[255]==211&&mc_world_get(world,-1,210,-17)==(9u<<4));
    CHECK(mc_world_set(world,-1,210,-17,0)&&mc_world_set(world,-1,200,-17,0));
    CHECK(chunk->heightMap[255]==6&&chunk->sectionMask==UINT16_C(0xb001));
    CHECK(mc_world_refresh_chunk_metadata(chunk,false));
    CHECK(chunk->heightMap[255]==6&&chunk->sectionMask==UINT16_C(0xb001));
    CHECK(mc_world_refresh_chunk_metadata(chunk,true));
    CHECK(chunk->heightMap[255]==6&&chunk->sectionMask==1);
    CHECK(mc_world_set(world,-1,250,-17,20u<<4)&&mc_world_set(world,-1,250,-17,0));
    mc_world *loaded=(mc_world *)malloc(sizeof(*loaded));CHECK(loaded);mc_world_init(loaded,0);
    char error[160];const char *path="native-world-view-test.c919";
    CHECK(mc_world_save(world,path,error,sizeof(error))&&mc_world_load(loaded,path,error,sizeof(error)));
    mc_chunk *restored=mc_world_chunk(loaded,-1,-2,false);CHECK(restored);
    /* The native C919WRL1 format reconstructs occupied sections. Source
       allocated-air storage lifetime and Anvil serialization are unported. */
    CHECK(restored->heightMap[255]==6&&restored->sectionMask==1);
    CHECK(chunk->sectionMask==UINT16_C(0x8001));
    mc_world_free(world);mc_world_free(loaded);free(world);free(loaded);remove(path);
}
static void source_methods_over_native_views(void) {
    mc_world *terrain=(mc_world *)malloc(sizeof(*terrain));CHECK(terrain);mc_world_init(terrain,919);
    CHECK(mc_world_set(terrain,0,5,0,1u<<4)&&mc_world_set(terrain,0,250,0,20u<<4));
    MCGameplay game={0};CHECK(MCGameplay_init(&game,32u*1024u*1024u));
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,game.heap));
    MCGameplayWorld *world=MCGameplayWorld_new(game.heap,MCGameplay_get(&game),terrain,NULL);CHECK(world);
    CHECK(MCGameplay_setWorld(&game,(MCObject *)world));
    BlockPos *query=DataWatcher_blockPos(game.heap,0,93,0);CHECK(query);
    BlockPos *height=World_getHeight(world,query);CHECK(height&&height->vec3i.y==6);
    BlockPos *top=World_getTopSolidOrLiquidBlock(world,query);CHECK(top&&top->vec3i.y==251);
    CHECK(mc_world_set(terrain,0,250,0,0)&&mc_world_set(terrain,0,200,0,18u<<4));
    height=World_getHeight(world,query);top=World_getTopSolidOrLiquidBlock(world,query);
    CHECK(height&&height->vec3i.y==201&&top&&top->vec3i.y==6);
    CHECK(mc_world_set(terrain,0,210,0,9u<<4));
    height=World_getHeight(world,query);top=World_getTopSolidOrLiquidBlock(world,query);
    CHECK(height&&height->vec3i.y==211&&top&&top->vec3i.y==6);
    query=DataWatcher_blockPos(game.heap,32,18,0);CHECK(query);
    height=World_getHeight(world,query);CHECK(height&&height->vec3i.y==0);
    CHECK(World_setSeaLevel(world,75));
    query=DataWatcher_blockPos(game.heap,30000000,0,0);CHECK(query);
    height=World_getHeight(world,query);CHECK(height&&height->vec3i.x==30000000&&height->vec3i.y==76);
    MCObjectRootScope_end(&scope);CHECK(MCObjectHeap_collect(game.heap));
    MCGameplayTransaction transaction={0};CHECK(MCGameplay_begin(&game,&transaction));
    World *copy=(World *)MCGameplay_get(&transaction.working)->world;
    CHECK(copy!=world&&copy->terrain==terrain&&copy->provider->worldObj==copy);
    CHECK(copy->worldBorder!=world->worldBorder&&copy->worldBorder->object.heap==transaction.working.heap);
    CHECK(MCObjectRootScope_begin(&scope,transaction.working.heap));
    query=DataWatcher_blockPos(transaction.working.heap,0,0,0);CHECK(query);
    height=World_getHeight(copy,query);CHECK(height&&height->vec3i.y==211);
    MCObjectRootScope_end(&scope);CHECK(MCGameplay_abort(&transaction));
    CHECK(MCGameplay_free(&game));mc_world_free(terrain);free(terrain);
}
static void manager_size_guard(bool collect) {
    mc_world *terrain=(mc_world *)malloc(sizeof(*terrain));CHECK(terrain);mc_world_init(terrain,0);
    CHECK(mc_world_set(terrain,0,5,0,1u<<4));
    MCGameplay game={0};CHECK(MCGameplay_init(&game,32u*1024u*1024u));
    World *world=MCGameplayWorld_new(game.heap,MCGameplay_get(&game),terrain,NULL);CHECK(world);
    CHECK(MCGameplay_setWorld(&game,(MCObject *)world));
    const MCObjectClass *descriptor=world->provider->worldChunkMgr->klass;
    world->provider->worldChunkMgr=MCObjectHeap_alloc(game.heap,sizeof(MCObject),descriptor);CHECK(world->provider->worldChunkMgr);
    if(collect)CHECK(!MCObjectHeap_collect(game.heap)&&MCObjectHeap_failed(game.heap));
    else {
        BlockPos *pos=DataWatcher_blockPos(game.heap,0,0,0);CHECK(pos);
        CHECK(!World_getHeight(world,pos)&&MCObjectHeap_failed(game.heap));
    }
    CHECK(MCGameplay_free(&game));mc_world_free(terrain);free(terrain);
}
int main(void) {
    dense_metadata();source_methods_over_native_views();manager_size_guard(false);manager_size_guard(true);
    printf("Native dense world and translated view methods: %u checks passed\n",checks);return 0;
}
