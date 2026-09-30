#include "util/transfer.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#define remove_dir(p) _rmdir(p)
#else
#include <unistd.h>
#define make_dir(p) mkdir(p,0700)
#define remove_dir(p) rmdir(p)
#endif
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"transfer check %u failed: %s at %d\n",checks,#x,__LINE__); exit(1); } } while(0)
static void value(mc_nbt *out,int number) {
    mc_buf data; mc_buf_init(&data); mc_put_u8(&data,10); mc_put_i16(&data,0);
    mc_put_u8(&data,3); mc_put_i16(&data,1); mc_put_u8(&data,'v'); mc_put_i32(&data,number); mc_put_u8(&data,0);
    CHECK(mc_nbt_read(&data,out)); mc_buf_free(&data);
}
static void same_file(const char *path,const mc_nbt *expected) {
    char error[256]; mc_nbt loaded; mc_nbt_init(&loaded);
    CHECK(mc_nbt_load_gzip(&loaded,path,error,sizeof error)); CHECK(mc_nbt_equal(&loaded,expected)); mc_nbt_free(&loaded);
}
static bool prepare(const char *base,const char *uuid,const mc_nbt *player,const mc_nbt *items,char *error,size_t size) {
    bool committed=false; bool ok=mc_transfer_prepare(base,uuid,player,items,&committed,error,size);
    if (ok) CHECK(committed);
    return ok;
}
int main(void) {
    const char *base="test-transfer-world.c919",*uuid="00000000-0000-3000-8000-000000000001";
    const char *player_path="test-transfer-world.c919.players/00000000-0000-3000-8000-000000000001.dat";
    const char *items_path="test-transfer-world.c919.items.dat",*journal="test-transfer-world.c919.transfer.dat";
    char error[256]; mc_nbt old_player,new_player,old_items,new_items;
    mc_nbt_init(&old_player); mc_nbt_init(&new_player); mc_nbt_init(&old_items); mc_nbt_init(&new_items);
    value(&old_player,1); value(&new_player,2); value(&old_items,3); value(&new_items,4);
    CHECK(mc_transfer_recover(base,error,sizeof error));
    CHECK(make_dir("test-transfer-world.c919.players")==0);
    CHECK(mc_nbt_save_gzip(&old_player,player_path,error,sizeof error));
    CHECK(mc_nbt_save_gzip(&old_items,items_path,error,sizeof error));
    CHECK(!prepare(base,"../unsafe",&new_player,&new_items,error,sizeof error));
    CHECK(!prepare(base,uuid,NULL,&new_items,error,sizeof error));
    CHECK(prepare(base,uuid,&new_player,&new_items,error,sizeof error));
    same_file(player_path,&old_player); same_file(items_path,&old_items);
    CHECK(!prepare(base,uuid,&old_player,&old_items,error,sizeof error));
    /* The writer has stopped after the durable commit point. Startup must
       finish both files, regardless of which checkpoint already exists. */
    CHECK(mc_nbt_save_gzip(&new_player,player_path,error,sizeof error));
    CHECK(mc_transfer_recover(base,error,sizeof error));
    same_file(player_path,&new_player); same_file(items_path,&new_items);
    CHECK(mc_transfer_recover(base,error,sizeof error));
    CHECK(remove(items_path)==0); CHECK(make_dir(items_path)==0);
    bool committed=false;
    CHECK(!mc_transfer_commit(base,uuid,&old_player,&old_items,&committed,error,sizeof error)); CHECK(committed);
    same_file(player_path,&old_player);
    CHECK(remove_dir(items_path)==0); CHECK(mc_transfer_recover(base,error,sizeof error));
    same_file(items_path,&old_items); same_file(player_path,&old_player);
    /* The second payload must be validated before either target is replaced. */
    CHECK(prepare(base,uuid,&new_player,&new_items,error,sizeof error));
    mc_nbt record; mc_nbt_init(&record);
    CHECK(mc_nbt_load_gzip(&record,journal,error,sizeof error));
    CHECK(mc_nbt_save_gzip(&old_player,"test-transfer-world.c919.pending-player.dat",error,sizeof error));
    CHECK(!mc_transfer_recover(base,error,sizeof error));
    same_file(player_path,&old_player); same_file(items_path,&old_items); same_file(journal,&record);
    CHECK(mc_nbt_save_gzip(&new_player,"test-transfer-world.c919.pending-player.dat",error,sizeof error));
    CHECK(mc_transfer_recover(base,error,sizeof error));
    same_file(player_path,&new_player); same_file(items_path,&new_items);
    struct stat info; CHECK(stat(journal,&info)==-1 && errno==ENOENT);
    /* A damaged journal UUID must not write outside the player directory. */
    CHECK(prepare(base,uuid,&old_player,&old_items,error,sizeof error));
    CHECK(mc_nbt_load_gzip(&record,journal,error,sizeof error));
    mc_nbt_view root,who;
    CHECK(mc_nbt_root(&record,&root) && mc_nbt_find(&root,"Player",&who));
    CHECK(who.type==8 && who.size==38);
    size_t uuid_offset=(size_t)(who.data-record.data)+2;
    memcpy(record.data+uuid_offset,"../",3);
    CHECK(mc_nbt_save_gzip(&record,journal,error,sizeof error));
    CHECK(!mc_transfer_recover(base,error,sizeof error));
    same_file(player_path,&new_player); same_file(items_path,&new_items); same_file(journal,&record);
    CHECK(stat("00000-0000-3000-8000-000000000001.dat",&info)==-1 && errno==ENOENT);
    memcpy(record.data+uuid_offset,uuid,3);
    CHECK(mc_nbt_save_gzip(&record,journal,error,sizeof error));
    CHECK(mc_transfer_recover(base,error,sizeof error));
    same_file(player_path,&old_player); same_file(items_path,&old_items);
    CHECK(stat(journal,&info)==-1 && errno==ENOENT);
    mc_nbt_free(&record);
    CHECK(prepare(base,NULL,NULL,&new_items,error,sizeof error));
    /* Corrupt staging must preserve both targets and retain the journal. */
    CHECK(mc_nbt_save_gzip(&old_items,"test-transfer-world.c919.pending-items.dat",error,sizeof error));
    CHECK(!mc_transfer_recover(base,error,sizeof error)); same_file(items_path,&old_items);
    CHECK(!stat(journal,&info));
    CHECK(mc_nbt_save_gzip(&new_items,"test-transfer-world.c919.pending-items.dat",error,sizeof error));
    CHECK(mc_transfer_recover(base,error,sizeof error)); same_file(items_path,&new_items); same_file(player_path,&old_player);
    committed=false;
    CHECK(!mc_transfer_commit("missing-transfer-dir/world",uuid,&new_player,&new_items,&committed,error,sizeof error)); CHECK(!committed);
    /* Three owners share one commit point. A corrupt final staging snapshot
       must be detected before even the first destination is replaced. */
    const char *maps_path="test-transfer-world.c919.maps.dat",*maps_stage="test-transfer-world.c919.pending-maps.dat";
    mc_nbt old_maps,new_maps; mc_nbt_init(&old_maps); mc_nbt_init(&new_maps);
    value(&old_maps,5); value(&new_maps,6);
    CHECK(mc_nbt_save_gzip(&old_maps,maps_path,error,sizeof error));
    committed=false;
    CHECK(mc_transfer_prepare_all(base,uuid,&new_player,&old_items,&new_maps,&committed,error,sizeof error)); CHECK(committed);
    same_file(player_path,&old_player); same_file(items_path,&new_items); same_file(maps_path,&old_maps);
    CHECK(mc_nbt_load_gzip(&record,journal,error,sizeof error));
    CHECK(mc_nbt_save_gzip(&old_maps,maps_stage,error,sizeof error));
    CHECK(!mc_transfer_recover(base,error,sizeof error));
    same_file(player_path,&old_player); same_file(items_path,&new_items); same_file(maps_path,&old_maps); same_file(journal,&record);
    CHECK(mc_nbt_save_gzip(&new_maps,maps_stage,error,sizeof error));
    CHECK(mc_transfer_recover(base,error,sizeof error));
    same_file(player_path,&new_player); same_file(items_path,&old_items); same_file(maps_path,&new_maps);
    CHECK(mc_transfer_recover(base,error,sizeof error));
    CHECK(remove(maps_path)==0); CHECK(make_dir(maps_path)==0);
    committed=false;
    CHECK(!mc_transfer_commit_all(base,uuid,&old_player,&new_items,&old_maps,&committed,error,sizeof error)); CHECK(committed);
    same_file(player_path,&old_player); same_file(items_path,&new_items); CHECK(!stat(journal,&info));
    CHECK(remove_dir(maps_path)==0); CHECK(mc_transfer_recover(base,error,sizeof error));
    same_file(player_path,&old_player); same_file(items_path,&new_items); same_file(maps_path,&old_maps);
    /* Old journals leave MapData alone; invalid map preparation commits none. */
    CHECK(prepare(base,uuid,&new_player,&old_items,error,sizeof error));
    CHECK(mc_transfer_recover(base,error,sizeof error)); same_file(maps_path,&old_maps);
    mc_nbt invalid_maps; mc_nbt_init(&invalid_maps); committed=true;
    CHECK(!mc_transfer_prepare_all(base,uuid,&old_player,&new_items,&invalid_maps,&committed,error,sizeof error)); CHECK(!committed);
    same_file(player_path,&new_player); same_file(items_path,&old_items); same_file(maps_path,&old_maps);
    CHECK(remove(maps_stage)==0); CHECK(make_dir(maps_stage)==0);
    committed=true;
    CHECK(!mc_transfer_prepare_all(base,uuid,&old_player,&new_items,&new_maps,&committed,error,sizeof error)); CHECK(!committed);
    CHECK(stat(journal,&info)==-1 && errno==ENOENT);
    same_file(player_path,&new_player); same_file(items_path,&old_items); same_file(maps_path,&old_maps);
    CHECK(remove_dir(maps_stage)==0);
    CHECK(mc_transfer_commit_all(base,NULL,NULL,&new_items,&new_maps,&committed,error,sizeof error)); CHECK(committed);
    same_file(player_path,&new_player); same_file(items_path,&new_items); same_file(maps_path,&new_maps);
    CHECK(remove(maps_path)==0); CHECK(remove(maps_stage)==0);
    mc_nbt_free(&record); mc_nbt_free(&old_maps); mc_nbt_free(&new_maps);
    CHECK(remove(player_path)==0); CHECK(remove(items_path)==0);
    CHECK(remove("test-transfer-world.c919.pending-player.dat")==0); CHECK(remove("test-transfer-world.c919.pending-items.dat")==0);
    CHECK(remove_dir("test-transfer-world.c919.players")==0);
    mc_nbt_free(&old_player); mc_nbt_free(&new_player); mc_nbt_free(&old_items); mc_nbt_free(&new_items);
    printf("transfer: %u checks passed\n",checks); return 0;
}
