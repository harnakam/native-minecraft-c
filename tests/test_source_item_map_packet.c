#include "item/ItemMapPacket.h"
#include "util/MCGameplayPlayer.h"
#include "util/Vec4b.h"
#include "world/WorldDataStorage.h"
#include "world/storage/SaveDataMemoryStorage.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"Source map factory check %u line %d: %s\n",checks,__LINE__,#x); exit(1); } } while(0)
typedef struct Fixture {
    MCObjectHeap *heap;
    World *world;
    MCGameplayPlayer *player;
    ItemStack *stack;
} Fixture;
/* Partial real receivers expose the actual fields reached by this method.
   This is a saved-data/packet fixture, not a World/Player constructor claim. */
static Fixture fixture(bool remote) {
    Fixture f={0};f.heap=MCObjectHeap_new(1024*1024);CHECK(f.heap);
    f.world=World_nativeAllocate(f.heap,NULL,NULL);CHECK(f.world);
    f.world->isRemote=remote;
    f.world->mapStorage=(MapStorage *)SaveDataMemoryStorage_new(f.heap);CHECK(f.world->mapStorage);
    f.world->worldInfo=WorldInfo_nativeAllocate(f.heap,NULL,NULL);CHECK(f.world->worldInfo);
    f.world->worldInfo->spawnX=99;f.world->worldInfo->spawnZ=-65;
    f.world->provider=WorldProvider_nativeAllocate(f.heap,NULL,NULL);CHECK(f.world->provider);
    f.world->provider->dimensionId=2;
    f.player=MCGameplayPlayer_nativeAllocate(f.heap);CHECK(f.player);
    f.player->living.entity.entityId=17;
    f.stack=ItemStack_new(f.heap,ItemStack_registryItem(358),1,7);CHECK(f.stack);
    return f;
}
static MapData *map(Fixture *f) {
    NBTString *name=NBTString_fromASCII(f->heap,"map_7");CHECK(name);
    MapData *m=MapData_new(f->heap,name,NULL,NULL);CHECK(m);
    CHECK(World_setItemData(f->world,name,&m->base));return m;
}
static void cache_and_cadence(void) {
    Fixture f=fixture(true);MapData *m=map(&f);MapInfo *info=NULL;
    CHECK(MapData_getMapInfo(m,f.player,&info)==WORLD_SAVED_DATA_OK&&info);
    Vec4b *icon=Vec4b_new(f.heap,2,3,4,5);CHECK(icon);
    NBTString *key=NBTString_fromASCII(f.heap,"viewer");CHECK(key);
    CHECK(NativeLinkedHashMap_put(m->mapDecorations,(MCObject *)key,(MCObject *)icon));
    m->scale=-1;m->colors->values[0]=39;
    S34PacketMaps *p=(S34PacketMaps *)(void *)m;
    CHECK(ItemMap_createMapDataPacket(f.stack,f.world,f.player,&p)==WORLD_SAVED_DATA_OK&&p);
    CHECK(p->mapId==7&&p->mapScale==-1&&p->mapMaxX==128&&p->mapMaxY==128);
    CHECK(p->mapDataBytes->values[0]==39&&p->mapVisiblePlayersVec4b->values[0]==(MCObject *)icon);
    CHECK(!info->field_176105_d&&info->field_176109_i==0&&f.world->maps.count==0);
    m->colors->values[0]=88;CHECK(Vec4b_construct(icon,6,7,8,9));
    CHECK(p->mapDataBytes->values[0]==39&&((Vec4b *)p->mapVisiblePlayersVec4b->values[0])->field_176117_a==6);
    mc_buf bytes={0};PacketBuffer buffer;
    CHECK(PacketBuffer_init(&buffer,f.heap,&bytes));
    CHECK(S34PacketMaps_writePacketData(p,&buffer)==NATIVE_ARRAY_OK);
    CHECK(mc_get_varint(&bytes)==7&&mc_get_u8(&bytes)==255&&mc_get_varint(&bytes)==1);
    CHECK(mc_get_u8(&bytes)==0x69&&mc_get_u8(&bytes)==7&&mc_get_u8(&bytes)==8);
    mc_buf_free(&bytes);
    p=NULL;CHECK(ItemMap_createMapDataPacket(f.stack,f.world,f.player,&p)==WORLD_SAVED_DATA_OK&&p);
    CHECK(p->mapMaxX==0&&info->field_176109_i==1);
    for(int i=1;i<5;++i) {
        p=(S34PacketMaps *)(void *)m;
        CHECK(ItemMap_createMapDataPacket(f.stack,f.world,f.player,&p)==WORLD_SAVED_DATA_OK&&p==NULL);
        CHECK(info->field_176109_i==i+1);
    }
    CHECK(MapData_updateMapData(m,12,13)==WORLD_SAVED_DATA_OK);
    CHECK(ItemMap_createMapDataPacket(f.stack,f.world,f.player,&p)==WORLD_SAVED_DATA_OK&&p);
    CHECK(p->mapMinX==12&&p->mapMinY==13&&p->mapMaxX==1&&p->mapMaxY==1);
    CHECK(!MCObjectHeap_failed(f.heap));MCObjectHeap_free(f.heap);
}
static void missing_data_and_output_prefix(void) {
    Fixture f=fixture(true);S34PacketMaps *sentinel=(S34PacketMaps *)(void *)f.stack,*p=sentinel;
    CHECK(ItemMap_createMapDataPacket(f.stack,f.world,f.player,&p)==WORLD_SAVED_DATA_EXCEPTION);
    CHECK(p==sentinel&&f.stack->itemDamage==7&&NativeHashMap_size(f.world->mapStorage->loadedDataMap)==0);
    CHECK(!MCObjectHeap_failed(f.heap));MCObjectHeap_free(f.heap);
    f=fixture(false);p=sentinel;
    CHECK(ItemMap_createMapDataPacket(f.stack,f.world,f.player,&p)==WORLD_SAVED_DATA_OK&&p==NULL);
    CHECK(f.stack->itemDamage==0&&f.world->maps.count==0);
    NBTString *key=NBTString_fromASCII(f.heap,"map_0");CHECK(key);
    WorldSavedData *stored=NULL;CHECK(World_loadItemData(f.world,NULL,key,&stored)&&stored);
    MapData *m=(MapData *)stored;
    CHECK(m->scale==3&&m->dimension==2&&m->base.dirty&&m->xCenter==448&&m->zCenter==-576);
    CHECK(NativeHashMap_size(m->playersHashMap)==0&&m->playersArrayList->size==0);
    p=(S34PacketMaps *)(void *)m;
    CHECK(ItemMap_createMapDataPacket(NULL,f.world,f.player,&p)==WORLD_SAVED_DATA_EXCEPTION&&p==(S34PacketMaps *)(void *)m);
    CHECK(ItemMap_createMapDataPacket(f.stack,NULL,f.player,&p)==WORLD_SAVED_DATA_EXCEPTION&&p==(S34PacketMaps *)(void *)m);
    CHECK(!MCObjectHeap_failed(f.heap));MCObjectHeap_free(f.heap);
}
static void unused_world_and_packet_failure(void) {
    Fixture f=fixture(true);MapData *m=map(&f);
    MCObjectHeap *other=MCObjectHeap_new(65536);CHECK(other);
    World *unused=World_nativeAllocate(other,NULL,NULL);CHECK(unused);
    S34PacketMaps *p=(S34PacketMaps *)(void *)m;
    CHECK(MapData_getMapPacket(m,NULL,unused,NULL,&p)==WORLD_SAVED_DATA_OK&&p==NULL);
    MapInfo *info=NULL;CHECK(MapData_getMapInfo(m,f.player,&info)==WORLD_SAVED_DATA_OK);
    m->colors=NativeByteArray_new(f.heap,1);CHECK(m->colors);
    p=(S34PacketMaps *)(void *)m;
    CHECK(ItemMap_createMapDataPacket(f.stack,f.world,f.player,&p)==WORLD_SAVED_DATA_EXCEPTION);
    CHECK(p==(S34PacketMaps *)(void *)m&&!info->field_176105_d&&!MCObjectHeap_failed(f.heap));
    CHECK(NativeHashMap_get(f.world->mapStorage->loadedDataMap,(MCObject *)NBTString_fromASCII(f.heap,"map_7"))==(MCObject *)m);
    CHECK(MapData_getMapPacket(m,f.stack,unused,f.player,&p)==WORLD_SAVED_DATA_OK&&p&&p->mapMaxX==0);
    CHECK(info->field_176109_i==1);
    MCObjectHeap_free(other);MCObjectHeap_free(f.heap);
}
int main(void) {
    cache_and_cadence();missing_data_and_output_prefix();unused_world_and_packet_failure();
    printf("Source ItemMap packet factory: %u checks\n",checks);return 0;
}
