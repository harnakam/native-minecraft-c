#include "transfer.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(path) _mkdir(path)
#else
#include <fcntl.h>
#include <unistd.h>
#define make_dir(path) mkdir(path,0700)
#endif

typedef struct { char journal[4096],player_stage[4096],items_stage[4096],maps_stage[4096],items[4096],maps[4096],player[4096],directory[4096]; } transfer_paths;
static bool fail(char *error,size_t size,const char *reason) { if (error && size) snprintf(error,size,"%s",reason); return false; }
static bool uuid_valid(const char *uuid) {
    if (!uuid || strlen(uuid)!=36) return false;
    for (unsigned i=0;i<36;i++) {
        if (i==8 || i==13 || i==18 || i==23) { if (uuid[i]!='-') return false; }
        else if (!((uuid[i]>='0' && uuid[i]<='9') || (uuid[i]>='a' && uuid[i]<='f'))) return false;
    }
    return true;
}
static bool paths_for(const char *base,const char *uuid,transfer_paths *paths,char *error,size_t size) {
    if (!base || !*base || strlen(base)>3900 || (uuid && !uuid_valid(uuid))) return fail(error,size,"Invalid transfer path or player UUID");
    snprintf(paths->journal,sizeof paths->journal,"%s.transfer.dat",base);
    snprintf(paths->player_stage,sizeof paths->player_stage,"%s.pending-player.dat",base);
    snprintf(paths->items_stage,sizeof paths->items_stage,"%s.pending-items.dat",base);
    snprintf(paths->maps_stage,sizeof paths->maps_stage,"%s.pending-maps.dat",base);
    snprintf(paths->items,sizeof paths->items,"%s.items.dat",base);
    snprintf(paths->maps,sizeof paths->maps,"%s.maps.dat",base);
    snprintf(paths->directory,sizeof paths->directory,"%s.players",base);
    if (uuid) snprintf(paths->player,sizeof paths->player,"%s.players/%s.dat",base,uuid); else paths->player[0]=0;
    return true;
}
static bool root_valid(const mc_nbt *data) {
    mc_nbt_view root; return data && data->size<=MC_NBT_MAX_BYTES && mc_nbt_root(data,&root) && root.type==10;
}
static uint32_t checksum(const mc_nbt *data) { return (uint32_t)crc32(0,data->data,(uInt)data->size); }
static bool sync_parent(const char *path,char *error,size_t size) {
#ifdef _WIN32
    (void)path; (void)error; (void)size;
    /* mc_nbt_save_gzip uses MoveFileEx WRITE_THROUGH after committing the file. */
    return true;
#else
    char parent[4096]; size_t length=strlen(path); if (length>=sizeof parent) return fail(error,size,"Transfer parent path is too long");
    memcpy(parent,path,length+1); char *slash=strrchr(parent,'/');
    if (!slash) strcpy(parent,"."); else if (slash==parent) slash[1]=0; else *slash=0;
    int descriptor=open(parent,O_RDONLY); if (descriptor<0) return fail(error,size,"Cannot open transfer parent directory");
    bool ok=fsync(descriptor)==0; (void)close(descriptor);
    return ok || fail(error,size,"Cannot synchronize transfer parent directory");
#endif
}
static void name(mc_buf *buffer,int type,const char *field) {
    mc_put_u8(buffer,(uint8_t)type); mc_put_i16(buffer,(int16_t)strlen(field)); mc_put_bytes(buffer,field,strlen(field));
}
static void integer(mc_buf *buffer,const char *field,uint32_t value) { name(buffer,3,field); mc_put_i32(buffer,(int32_t)value); }
static bool manifest(const char *uuid,const mc_nbt *player,const mc_nbt *items,const mc_nbt *maps,mc_nbt *output) {
    mc_buf buffer; mc_buf_init(&buffer); name(&buffer,10,""); integer(&buffer,"Version",maps ? 2 : 1);
    const char *who=uuid ? uuid : ""; name(&buffer,8,"Player"); mc_put_i16(&buffer,(int16_t)strlen(who)); mc_put_bytes(&buffer,who,strlen(who));
    integer(&buffer,"ItemsSize",(uint32_t)items->size); integer(&buffer,"ItemsCRC",checksum(items));
    if (uuid) { integer(&buffer,"PlayerSize",(uint32_t)player->size); integer(&buffer,"PlayerCRC",checksum(player)); }
    if (maps) { integer(&buffer,"MapsSize",(uint32_t)maps->size); integer(&buffer,"MapsCRC",checksum(maps)); }
    mc_put_u8(&buffer,0); bool ok=!buffer.failed && mc_nbt_read(&buffer,output) && buffer.pos==buffer.len;
    mc_buf_free(&buffer); return ok;
}
static bool get_int(const mc_nbt_view *root,const char *field,uint32_t *out) {
    mc_nbt_view value; int64_t number;
    if (!mc_nbt_find(root,field,&value) || value.type!=3 || !mc_nbt_get_integer(&value,&number)) return false;
    *out=(uint32_t)number; return true;
}
static bool checked_payload(const mc_nbt_view *root,const char *prefix,const char *path,mc_nbt *output,char *error,size_t size) {
    char field[32]; uint32_t expected_size,expected_crc;
    snprintf(field,sizeof field,"%sSize",prefix); if (!get_int(root,field,&expected_size)) return fail(error,size,"Invalid transfer payload length");
    snprintf(field,sizeof field,"%sCRC",prefix); if (!get_int(root,field,&expected_crc)) return fail(error,size,"Invalid transfer payload checksum");
    if (!expected_size || expected_size>MC_NBT_MAX_BYTES) return fail(error,size,"Invalid transfer payload length");
    if (!mc_nbt_load_gzip(output,path,error,size)) return false;
    return (root_valid(output) && output->size==expected_size && checksum(output)==expected_crc) || fail(error,size,"Transfer staging does not match committed journal; original targets were preserved");
}
typedef struct {
    char uuid[37],stage[4096],target[4096];
    uint32_t size,crc;
    mc_nbt snapshot;
} group_player;
static void free_group(group_player *players,size_t count) {
    if (players) for (size_t i=0;i<count;i++) mc_nbt_free(&players[i].snapshot);
    free(players);
}
static void group_player_paths(const char *base,group_player *player) {
    /* paths_for checked the base length; UUID is exactly 36 ASCII bytes. */
    snprintf(player->stage,sizeof player->stage,"%.3900s.pending-player.%.36s.dat",base,player->uuid);
    snprintf(player->target,sizeof player->target,"%.3900s.players/%.36s.dat",base,player->uuid);
}
static void string_field(mc_buf *buffer,const char *field,const char *value) {
    name(buffer,8,field); mc_put_i16(buffer,(int16_t)strlen(value)); mc_put_bytes(buffer,value,strlen(value));
}
static bool group_manifest(const mc_transfer_player *players,size_t count,const mc_nbt *items,const mc_nbt *maps,mc_nbt *output) {
    mc_buf buffer; mc_buf_init(&buffer); name(&buffer,10,""); integer(&buffer,"Version",3);
    name(&buffer,9,"Players"); mc_put_u8(&buffer,10); mc_put_i32(&buffer,(int32_t)count);
    for (size_t i=0;i<count;i++) {
        string_field(&buffer,"UUID",players[i].uuid);
        integer(&buffer,"Size",(uint32_t)players[i].snapshot->size); integer(&buffer,"CRC",checksum(players[i].snapshot));
        mc_put_u8(&buffer,0);
    }
    integer(&buffer,"ItemsSize",(uint32_t)items->size); integer(&buffer,"ItemsCRC",checksum(items));
    name(&buffer,1,"HasMaps"); mc_put_u8(&buffer,maps ? 1 : 0);
    if (maps) { integer(&buffer,"MapsSize",(uint32_t)maps->size); integer(&buffer,"MapsCRC",checksum(maps)); }
    mc_put_u8(&buffer,0); bool ok=!buffer.failed && mc_nbt_read(&buffer,output) && buffer.pos==buffer.len;
    mc_buf_free(&buffer); return ok;
}
bool mc_transfer_prepare_group(const char *base,const mc_transfer_player *players,size_t count,const mc_nbt *items,const mc_nbt *maps,bool *committed,char *error,size_t size) {
    if (!committed) return fail(error,size,"Transfer commit result is required");
    *committed=false;
    transfer_paths paths;
    if (!paths_for(base,NULL,&paths,error,size) || count>MC_TRANSFER_MAX_PLAYERS || (count && !players) ||
        !root_valid(items) || (maps && !root_valid(maps))) return fail(error,size,"Invalid group transfer arguments");
    /* Invalid or duplicate late entries must cause no staging writes. */
    for (size_t i=0;i<count;i++) {
        if (!uuid_valid(players[i].uuid) || !root_valid(players[i].snapshot)) return fail(error,size,"Invalid group transfer player");
        for (size_t j=0;j<i;j++) if (!strcmp(players[i].uuid,players[j].uuid)) return fail(error,size,"Duplicate group transfer player UUID");
    }
    struct stat info;
    if (!stat(paths.journal,&info)) return fail(error,size,"A committed transfer already requires recovery");
    if (errno!=ENOENT) return fail(error,size,"Cannot inspect transfer journal");
    if (!sync_parent(paths.journal,error,size)) return false;
    if (count && make_dir(paths.directory) && errno!=EEXIST) return fail(error,size,"Cannot create transfer player directory");
    mc_nbt record; mc_nbt_init(&record);
    bool ok=group_manifest(players,count,items,maps,&record);
    if (!ok) fail(error,size,"Cannot encode group transfer journal");
    for (size_t i=0;ok && i<count;i++) {
        group_player entry; memset(&entry,0,sizeof entry); memcpy(entry.uuid,players[i].uuid,37); group_player_paths(base,&entry);
        ok=mc_nbt_save_gzip(players[i].snapshot,entry.stage,error,size) && sync_parent(entry.stage,error,size);
    }
    if (ok) ok=mc_nbt_save_gzip(items,paths.items_stage,error,size) && sync_parent(paths.items_stage,error,size);
    if (ok && maps) ok=mc_nbt_save_gzip(maps,paths.maps_stage,error,size) && sync_parent(paths.maps_stage,error,size);
    if (ok) {
        ok=mc_nbt_save_gzip(&record,paths.journal,error,size);
        if (ok) { *committed=true; ok=sync_parent(paths.journal,error,size); }
    }
    mc_nbt_free(&record); return ok;
}
static bool recover_group(const char *base,const transfer_paths *paths,const mc_nbt_view *root,char *error,size_t size) {
    mc_nbt_view list,flag; int64_t maps_flag;
    if (!mc_nbt_find(root,"Players",&list) || list.type!=9 || list.size<5 || list.data[0]!=10 ||
        !mc_nbt_find(root,"HasMaps",&flag) || flag.type!=1 || !mc_nbt_get_integer(&flag,&maps_flag) ||
        (maps_flag!=0 && maps_flag!=1)) return fail(error,size,"Invalid committed group transfer journal; data was preserved");
    uint32_t count=((uint32_t)list.data[1]<<24)|((uint32_t)list.data[2]<<16)|((uint32_t)list.data[3]<<8)|list.data[4];
    if (count>MC_TRANSFER_MAX_PLAYERS) return fail(error,size,"Invalid committed group transfer player count; data was preserved");
    group_player *players=count ? calloc(count,sizeof *players) : NULL;
    if (count && !players) return fail(error,size,"Cannot allocate group transfer recovery snapshots");
    mc_nbt items,maps; mc_nbt_init(&items); mc_nbt_init(&maps); bool ok=true;
    for (size_t i=0;ok && i<count;i++) {
        mc_nbt_view entry,who;
        ok=mc_nbt_list_get(&list,i,&entry) && mc_nbt_find(&entry,"UUID",&who) && who.type==8 &&
            mc_nbt_get_string(&who,players[i].uuid,sizeof players[i].uuid) && uuid_valid(players[i].uuid) &&
            get_int(&entry,"Size",&players[i].size) && players[i].size && players[i].size<=MC_NBT_MAX_BYTES &&
            get_int(&entry,"CRC",&players[i].crc);
        for (size_t j=0;ok && j<i;j++) ok=strcmp(players[i].uuid,players[j].uuid)!=0;
        if (ok) group_player_paths(base,&players[i]);
        else fail(error,size,"Invalid committed group transfer player; data was preserved");
    }
    /* Keep the validated owned bytes: checkpointing must not reload a stage
       after an earlier target has been written. */
    for (size_t i=0;ok && i<count;i++) {
        ok=mc_nbt_load_gzip(&players[i].snapshot,players[i].stage,error,size);
        if (ok && (!root_valid(&players[i].snapshot) || players[i].snapshot.size!=players[i].size ||
            checksum(&players[i].snapshot)!=players[i].crc)) ok=fail(error,size,"Group staging does not match committed journal; original targets were preserved");
    }
    if (ok) ok=checked_payload(root,"Items",paths->items_stage,&items,error,size);
    if (ok && maps_flag) ok=checked_payload(root,"Maps",paths->maps_stage,&maps,error,size);
    /* Every snapshot has now been validated, before even player 1 changes. */
    if (ok && count && make_dir(paths->directory) && errno!=EEXIST) ok=fail(error,size,"Cannot create transfer player directory");
    for (size_t i=0;ok && i<count;i++) ok=mc_nbt_save_gzip(&players[i].snapshot,players[i].target,error,size) && sync_parent(players[i].target,error,size);
    if (ok) ok=mc_nbt_save_gzip(&items,paths->items,error,size) && sync_parent(paths->items,error,size);
    if (ok && maps_flag) ok=mc_nbt_save_gzip(&maps,paths->maps,error,size) && sync_parent(paths->maps,error,size);
    if (ok) {
        if (remove(paths->journal)) ok=fail(error,size,"Cannot retire completed transfer journal");
        else ok=sync_parent(paths->journal,error,size);
    }
    free_group(players,count); mc_nbt_free(&items); mc_nbt_free(&maps); return ok;
}
bool mc_transfer_prepare_all(const char *base,const char *uuid,const mc_nbt *player,const mc_nbt *items,const mc_nbt *maps,bool *committed,char *error,size_t size) {
    if (!committed) return fail(error,size,"Transfer commit result is required");
    *committed=false;
    transfer_paths paths;
    if (!paths_for(base,uuid,&paths,error,size) || !root_valid(items) || (uuid && !root_valid(player)) || (!uuid && player) || (maps && !root_valid(maps))) return fail(error,size,"Invalid transfer arguments");
    struct stat info;
    if (!stat(paths.journal,&info)) return fail(error,size,"A committed transfer already requires recovery");
    if (errno!=ENOENT) return fail(error,size,"Cannot inspect transfer journal");
    if (!sync_parent(paths.journal,error,size)) return false;
    if (uuid && make_dir(paths.directory) && errno!=EEXIST) return fail(error,size,"Cannot create transfer player directory");
    mc_nbt record; mc_nbt_init(&record);
    bool ok=manifest(uuid,player,items,maps,&record);
    if (!ok) fail(error,size,"Cannot encode transfer journal");
    if (ok) ok=mc_nbt_save_gzip(items,paths.items_stage,error,size) && sync_parent(paths.items_stage,error,size);
    if (ok && uuid) ok=mc_nbt_save_gzip(player,paths.player_stage,error,size) && sync_parent(paths.player_stage,error,size);
    if (ok && maps) ok=mc_nbt_save_gzip(maps,paths.maps_stage,error,size) && sync_parent(paths.maps_stage,error,size);
    if (ok) {
        ok=mc_nbt_save_gzip(&record,paths.journal,error,size);
        if (ok) { *committed=true; ok=sync_parent(paths.journal,error,size); }
    }
    mc_nbt_free(&record); return ok;
}
bool mc_transfer_prepare(const char *base,const char *uuid,const mc_nbt *player,const mc_nbt *items,bool *committed,char *error,size_t size) {
    return mc_transfer_prepare_all(base,uuid,player,items,NULL,committed,error,size);
}
bool mc_transfer_recover(const char *base,char *error,size_t size) {
    transfer_paths paths; if (!paths_for(base,NULL,&paths,error,size)) return false;
    struct stat info;
    if (stat(paths.journal,&info)) return errno==ENOENT || fail(error,size,"Cannot inspect transfer journal");
    mc_nbt record,player,items,maps; mc_nbt_init(&record); mc_nbt_init(&player); mc_nbt_init(&items); mc_nbt_init(&maps);
    mc_nbt_view root,who; uint32_t version; char uuid[37];
    bool ok=mc_nbt_load_gzip(&record,paths.journal,error,size) && mc_nbt_root(&record,&root) && root.type==10 &&
        get_int(&root,"Version",&version) && (version==1 || version==2 || version==3);
    if (ok && version==3) {
        ok=recover_group(base,&paths,&root,error,size);
        mc_nbt_free(&record); mc_nbt_free(&player); mc_nbt_free(&items); mc_nbt_free(&maps); return ok;
    }
    if (ok) ok=mc_nbt_find(&root,"Player",&who) && mc_nbt_get_string(&who,uuid,sizeof uuid);
    if (!ok) fail(error,size,"Invalid committed transfer journal; data was preserved");
    bool has_player=ok && uuid[0];
    if (ok) ok=paths_for(base,has_player ? uuid : NULL,&paths,error,size);
    if (ok) ok=checked_payload(&root,"Items",paths.items_stage,&items,error,size);
    if (ok && has_player) ok=checked_payload(&root,"Player",paths.player_stage,&player,error,size);
    if (ok && version==2) ok=checked_payload(&root,"Maps",paths.maps_stage,&maps,error,size);
    /* Validate ALL staging snapshots before replacing any target. */
    if (ok && has_player) ok=mc_nbt_save_gzip(&player,paths.player,error,size) && sync_parent(paths.player,error,size);
    if (ok) ok=mc_nbt_save_gzip(&items,paths.items,error,size) && sync_parent(paths.items,error,size);
    if (ok && version==2) ok=mc_nbt_save_gzip(&maps,paths.maps,error,size) && sync_parent(paths.maps,error,size);
    if (ok) {
        if (remove(paths.journal)) ok=fail(error,size,"Cannot retire completed transfer journal");
        else ok=sync_parent(paths.journal,error,size);
    }
    mc_nbt_free(&record); mc_nbt_free(&player); mc_nbt_free(&items); mc_nbt_free(&maps); return ok;
}
bool mc_transfer_commit(const char *base,const char *uuid,const mc_nbt *player,const mc_nbt *items,bool *committed,char *error,size_t size) {
    if (!mc_transfer_prepare(base,uuid,player,items,committed,error,size)) return false;
    return mc_transfer_recover(base,error,size);
}
bool mc_transfer_commit_all(const char *base,const char *uuid,const mc_nbt *player,const mc_nbt *items,const mc_nbt *maps,bool *committed,char *error,size_t size) {
    if (!mc_transfer_prepare_all(base,uuid,player,items,maps,committed,error,size)) return false;
    return mc_transfer_recover(base,error,size);
}
bool mc_transfer_commit_group(const char *base,const mc_transfer_player *players,size_t count,const mc_nbt *items,const mc_nbt *maps,bool *committed,char *error,size_t size) {
    if (!mc_transfer_prepare_group(base,players,count,items,maps,committed,error,size)) return false;
    return mc_transfer_recover(base,error,size);
}
