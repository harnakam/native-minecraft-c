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
static void group_uuid(char uuid[37],unsigned index) {
    snprintf(uuid,37,"00000000-0000-3000-8000-%012x",index+1);
}
static void group_path(char path[256],const char *uuid,bool staging) {
    snprintf(path,256,staging ? "test-transfer-group.c919.pending-player.%.36s.dat" : "test-transfer-group.c919.players/%.36s.dat",uuid);
}
static void test_group(void) {
    const char *base="test-transfer-group.c919",*directory="test-transfer-group.c919.players";
    const char *journal="test-transfer-group.c919.transfer.dat",*items_path="test-transfer-group.c919.items.dat";
    const char *maps_path="test-transfer-group.c919.maps.dat",*maps_stage="test-transfer-group.c919.pending-maps.dat";
    char error[256],uuid[MC_TRANSFER_MAX_PLAYERS][37],path[256];
    mc_nbt old[3],next[3],old_items,next_items,old_maps,next_maps,record,invalid;
    for (unsigned i=0;i<3;i++) { mc_nbt_init(&old[i]); mc_nbt_init(&next[i]); value(&old[i],10+(int)i); value(&next[i],20+(int)i); }
    mc_nbt_init(&old_items); mc_nbt_init(&next_items); mc_nbt_init(&old_maps); mc_nbt_init(&next_maps); mc_nbt_init(&record); mc_nbt_init(&invalid);
    value(&old_items,30); value(&next_items,31); value(&old_maps,40); value(&next_maps,41);
    mc_transfer_player players[MC_TRANSFER_MAX_PLAYERS];
    for (unsigned i=0;i<MC_TRANSFER_MAX_PLAYERS;i++) { group_uuid(uuid[i],i); players[i]=(mc_transfer_player){uuid[i],i<3 ? &next[i] : &next[0]}; }
    CHECK(make_dir(directory)==0);
    for (unsigned i=0;i<3;i++) { group_path(path,uuid[i],false); CHECK(mc_nbt_save_gzip(&old[i],path,error,sizeof error)); }
    CHECK(mc_nbt_save_gzip(&old_items,items_path,error,sizeof error)); CHECK(mc_nbt_save_gzip(&old_maps,maps_path,error,sizeof error));
    bool committed=false;
    CHECK(mc_transfer_prepare_group(base,players,3,&next_items,&next_maps,&committed,error,sizeof error)); CHECK(committed);
    for (unsigned i=0;i<3;i++) { group_path(path,uuid[i],false); same_file(path,&old[i]); }
    same_file(items_path,&old_items); same_file(maps_path,&old_maps);
    CHECK(mc_nbt_load_gzip(&record,journal,error,sizeof error)); mc_nbt_view root,field,list; int64_t version;
    CHECK(mc_nbt_root(&record,&root) && mc_nbt_find(&root,"Version",&field) && mc_nbt_get_integer(&field,&version) && version==3);
    CHECK(mc_nbt_find(&root,"Players",&list) && mc_nbt_list_get(&list,2,&field));
    committed=true; CHECK(!mc_transfer_prepare_group(base,players,3,&old_items,NULL,&committed,error,sizeof error)); CHECK(!committed);
    CHECK(!mc_transfer_prepare_all(base,uuid[0],&old[0],&old_items,&old_maps,&committed,error,sizeof error)); CHECK(!committed);
    same_file(journal,&record);
    /* The second and last player must be checked before even player1 writes. */
    for (unsigned bad=1;bad<3;bad++) {
        group_path(path,uuid[bad],true); CHECK(mc_nbt_save_gzip(&old[bad],path,error,sizeof error));
        CHECK(!mc_transfer_recover(base,error,sizeof error));
        for (unsigned i=0;i<3;i++) { group_path(path,uuid[i],false); same_file(path,&old[i]); }
        same_file(items_path,&old_items); same_file(maps_path,&old_maps); same_file(journal,&record);
        group_path(path,uuid[bad],true); CHECK(mc_nbt_save_gzip(&next[bad],path,error,sizeof error));
    }
    CHECK(mc_nbt_save_gzip(&old_maps,maps_stage,error,sizeof error)); CHECK(!mc_transfer_recover(base,error,sizeof error));
    for (unsigned i=0;i<3;i++) { group_path(path,uuid[i],false); same_file(path,&old[i]); }
    same_file(items_path,&old_items); same_file(maps_path,&old_maps); same_file(journal,&record);
    CHECK(mc_nbt_save_gzip(&next_maps,maps_stage,error,sizeof error));
    CHECK(mc_transfer_recover(base,error,sizeof error)); CHECK(mc_transfer_recover(base,error,sizeof error));
    for (unsigned i=0;i<3;i++) { group_path(path,uuid[i],false); same_file(path,&next[i]); }
    same_file(items_path,&next_items); same_file(maps_path,&next_maps);
    /* A failure after player1 checkpoints retains one committed manifest. */
    for (unsigned i=0;i<3;i++) players[i].snapshot=&old[i];
    group_path(path,uuid[1],false); CHECK(remove(path)==0); CHECK(make_dir(path)==0);
    committed=false; CHECK(!mc_transfer_commit_group(base,players,3,&old_items,&old_maps,&committed,error,sizeof error)); CHECK(committed);
    group_path(path,uuid[0],false); same_file(path,&old[0]); group_path(path,uuid[2],false); same_file(path,&next[2]);
    same_file(items_path,&next_items); same_file(maps_path,&next_maps);
    struct stat info; CHECK(!stat(journal,&info)); group_path(path,uuid[1],false); CHECK(remove_dir(path)==0);
    CHECK(mc_transfer_recover(base,error,sizeof error));
    for (unsigned i=0;i<3;i++) { group_path(path,uuid[i],false); same_file(path,&old[i]); }
    same_file(items_path,&old_items); same_file(maps_path,&old_maps);
    mc_transfer_player duplicate[2]={{uuid[0],&next[0]},{uuid[0],&next[1]}};
    committed=true; CHECK(!mc_transfer_prepare_group(base,duplicate,2,&next_items,NULL,&committed,error,sizeof error)); CHECK(!committed);
    CHECK(!mc_transfer_prepare_group(base,NULL,1,&next_items,NULL,&committed,error,sizeof error));
    CHECK(!mc_transfer_prepare_group(base,players,MC_TRANSFER_MAX_PLAYERS+1,&next_items,NULL,&committed,error,sizeof error));
    CHECK(!mc_transfer_prepare_group(base,players,SIZE_MAX,&next_items,NULL,&committed,error,sizeof error));
    mc_transfer_player malformed={"../unsafe",&next[0]};
    CHECK(!mc_transfer_prepare_group(base,&malformed,1,&next_items,NULL,&committed,error,sizeof error));
    malformed=(mc_transfer_player){uuid[0],&invalid};
    CHECK(!mc_transfer_prepare_group(base,&malformed,1,&next_items,NULL,&committed,error,sizeof error));
    mc_nbt oversized={NULL,MC_NBT_MAX_BYTES+1u}; malformed.snapshot=&oversized;
    CHECK(!mc_transfer_prepare_group(base,&malformed,1,&next_items,NULL,&committed,error,sizeof error));
    CHECK(!mc_transfer_prepare_group(base,NULL,0,&next_items,NULL,NULL,error,sizeof error));
    CHECK(stat(journal,&info)==-1 && errno==ENOENT);
    /* A bad last argument must not replace already validated early stages. */
    for (unsigned i=0;i<2;i++) players[i].snapshot=&next[i];
    players[2].snapshot=&invalid;
    committed=true; CHECK(!mc_transfer_prepare_group(base,players,3,&next_items,&next_maps,&committed,error,sizeof error)); CHECK(!committed);
    for (unsigned i=0;i<3;i++) { group_path(path,uuid[i],true); same_file(path,&old[i]); }
    CHECK(stat(journal,&info)==-1 && errno==ENOENT);
    /* An invalid late stage leaves every destination old and no manifest. */
    for (unsigned i=0;i<3;i++) players[i].snapshot=&next[i];
    group_path(path,uuid[2],true); CHECK(remove(path)==0); CHECK(make_dir(path)==0);
    committed=true; CHECK(!mc_transfer_prepare_group(base,players,3,&next_items,&next_maps,&committed,error,sizeof error)); CHECK(!committed);
    CHECK(stat(journal,&info)==-1 && errno==ENOENT);
    for (unsigned i=0;i<3;i++) { group_path(path,uuid[i],false); same_file(path,&old[i]); }
    same_file(items_path,&old_items); same_file(maps_path,&old_maps);
    group_path(path,uuid[2],true); CHECK(remove_dir(path)==0);
    CHECK(mc_transfer_prepare_group(base,players,3,&next_items,&next_maps,&committed,error,sizeof error)); CHECK(committed);
    /* Corrupt manifest UUIDs cannot alias another target or escape the folder. */
    CHECK(mc_nbt_load_gzip(&record,journal,error,sizeof error)); CHECK(mc_nbt_root(&record,&root) && mc_nbt_find(&root,"Players",&list));
    mc_nbt_view entry,who; CHECK(mc_nbt_list_get(&list,1,&entry) && mc_nbt_find(&entry,"UUID",&who) && who.type==8 && who.size==38);
    size_t offset=(size_t)(who.data-record.data)+2; memcpy(record.data+offset,uuid[0],36);
    CHECK(mc_nbt_save_gzip(&record,journal,error,sizeof error)); CHECK(!mc_transfer_recover(base,error,sizeof error));
    for (unsigned i=0;i<3;i++) { group_path(path,uuid[i],false); same_file(path,&old[i]); }
    same_file(items_path,&old_items); same_file(maps_path,&old_maps);
    memcpy(record.data+offset,uuid[1],36); memcpy(record.data+offset,"../",3);
    CHECK(mc_nbt_save_gzip(&record,journal,error,sizeof error)); CHECK(!mc_transfer_recover(base,error,sizeof error));
    for (unsigned i=0;i<3;i++) { group_path(path,uuid[i],false); same_file(path,&old[i]); }
    same_file(items_path,&old_items); same_file(maps_path,&old_maps);
    memcpy(record.data+offset,uuid[1],36); CHECK(mc_nbt_save_gzip(&record,journal,error,sizeof error)); CHECK(mc_transfer_recover(base,error,sizeof error));
    committed=false; CHECK(mc_transfer_commit_group(base,NULL,0,&old_items,NULL,&committed,error,sizeof error)); CHECK(committed);
    for (unsigned i=0;i<3;i++) { group_path(path,uuid[i],false); same_file(path,&next[i]); }
    same_file(items_path,&old_items); same_file(maps_path,&next_maps);
    CHECK(mc_transfer_commit_group(base,players,MC_TRANSFER_MAX_PLAYERS,&next_items,&old_maps,&committed,error,sizeof error)); CHECK(committed);
    for (unsigned i=0;i<MC_TRANSFER_MAX_PLAYERS;i++) {
        group_path(path,uuid[i],false); same_file(path,players[i].snapshot); CHECK(remove(path)==0);
        group_path(path,uuid[i],true); CHECK(remove(path)==0);
    }
    same_file(items_path,&next_items); same_file(maps_path,&old_maps);
    CHECK(remove(items_path)==0); CHECK(remove(maps_path)==0); CHECK(remove(maps_stage)==0);
    CHECK(remove("test-transfer-group.c919.pending-items.dat")==0); CHECK(remove_dir(directory)==0);
    CHECK(stat(journal,&info)==-1 && errno==ENOENT);
    for (unsigned i=0;i<3;i++) { mc_nbt_free(&old[i]); mc_nbt_free(&next[i]); }
    mc_nbt_free(&old_items); mc_nbt_free(&next_items); mc_nbt_free(&old_maps); mc_nbt_free(&next_maps); mc_nbt_free(&record); mc_nbt_free(&invalid);
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
    test_group(); printf("transfer: %u checks passed\n",checks); return 0;
}
