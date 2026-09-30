#include "MapData.h"
#include "world/map.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char key[MC_MAP_DECORATION_KEY+1]; mc_map_icon icon; } decoration;
struct mc_MapData_tracking {
    size_t viewers,decorations;
    mc_MapInfo info[MC_MAP_MAX_VIEWERS];
    decoration markers[MC_MAX_MAP_ICONS];
};
static bool tracking(mc_map_info *map) {
    if (!map->tracking) map->tracking=calloc(1,sizeof(*map->tracking));
    return map->tracking!=NULL;
}
void mc_MapData_free_tracking(mc_map_info *map) { free(map->tracking); map->tracking=NULL; }
bool mc_MapData_copy_tracking(mc_map_info *to,const mc_map_info *from) {
    mc_MapData_tracking *copy=NULL;
    if (from->tracking) { copy=malloc(sizeof(*copy)); if (!copy) return false; *copy=*from->tracking; }
    mc_MapData_free_tracking(to); to->tracking=copy; return true;
}
mc_MapInfo *mc_MapData_getMapInfo(mc_map_info *map,int32_t id) {
    if (!map || !tracking(map)) return NULL;
    for (size_t i=0;i<map->tracking->viewers;i++) if (map->tracking->info[i].entity_id==id) return &map->tracking->info[i];
    if (map->tracking->viewers==MC_MAP_MAX_VIEWERS) return NULL;
    mc_MapInfo *info=&map->tracking->info[map->tracking->viewers++];
    *info=(mc_MapInfo){0}; info->entity_id=id; info->dirty=true; info->max_x=127; info->max_z=127;
    return info;
}
static int32_t java_int(double number) {
    return isnan(number) ? 0 : number>=INT32_MAX ? INT32_MAX : number<=INT32_MIN ? INT32_MIN : (int32_t)number;
}
static void sync_icons(mc_map_info *map) {
    map->icon_count=map->tracking->decorations;
    for (size_t i=0;i<map->icon_count;i++) map->icons[i]=map->tracking->markers[i].icon;
}
bool mc_MapData_updateDecorations(mc_map_info *map,int type,const char *id,double x,double z,double yaw,int64_t time) {
    if (!mc_map_info_valid(map) || !id || strlen(id)>MC_MAP_DECORATION_KEY || !tracking(map)) return false;
    mc_MapData_tracking *data=map->tracking; size_t index=0;
    while (index<data->decorations && strcmp(data->markers[index].key,id)) index++;
    float dx=(float)(x-map->center_x)/(float)(1u<<map->scale),dz=(float)(z-map->center_z)/(float)(1u<<map->scale);
    uint8_t xb=(uint8_t)java_int((double)(dx*2.0f)+.5),zb=(uint8_t)java_int((double)(dz*2.0f)+.5),direction;
    if (dx>=-63 && dz>=-63 && dx<=63 && dz<=63) {
        yaw+=yaw<0 ? -8 : 8; direction=(uint8_t)java_int(yaw*16/360);
        if (map->dimension<0) {
            uint32_t t=(uint32_t)(time/10);
            direction=(uint8_t)(((t*t*UINT32_C(34187121)+t*121u)>>15)&15u);
        }
    } else {
        if (fabsf(dx)>=320 || fabsf(dz)>=320) {
            if (index<data->decorations) { memmove(&data->markers[index],&data->markers[index+1],(data->decorations-index-1)*sizeof(data->markers[0])); data->decorations--; }
            sync_icons(map); return true;
        }
        type=6; direction=0;
        if (dx<=-63) xb=128;
        if (dz<=-63) zb=128;
        if (dx>=63) xb=127;
        if (dz>=63) zb=127;
    }
    if (index==data->decorations) {
        if (data->decorations==MC_MAX_MAP_ICONS) return false;
        strcpy(data->markers[index].key,id); data->decorations++;
    }
    mc_map_icon *icon=&data->markers[index].icon; icon->type=(uint8_t)type&15u; icon->direction=direction&15u;
    memcpy(&icon->x,&xb,1); memcpy(&icon->z,&zb,1); sync_icons(map); return true;
}
static bool contains(const mc_inventory *inventory,const mc_slot *stack) {
    if (!inventory) return false;
    for (int i=5;i<MC_PLAYER_INVENTORY_SIZE;i++) if (inventory->slots[i].item_id==stack->item_id && inventory->slots[i].damage==stack->damage) return true;
    return false;
}
static const mc_map_player *player(const mc_map_player *players,size_t count,int32_t id) {
    for (size_t i=0;i<count;i++) if (players[i].entity_id==id) return &players[i];
    return NULL;
}
static double number(const mc_nbt_view *root,const char *name) {
    mc_nbt_view field; double value=0;
    if (mc_nbt_find(root,name,&field)) (void)mc_nbt_get_number(&field,&value);
    return value;
}
static uint8_t byte_value(const mc_nbt_view *root,const char *name) {
    mc_nbt_view field; int64_t integer;
    if (mc_nbt_find(root,name,&field) && mc_nbt_get_integer(&field,&integer)) return (uint8_t)integer;
    double value=number(root,name); int32_t whole=java_int(value); uint32_t floored=(uint32_t)whole;
    if (value<(double)whole) floored--;
    return (uint8_t)floored;
}
static bool visible(mc_map_info *map,const mc_slot *stack,const mc_map_player *viewer,const mc_map_player *players,size_t count,int64_t time) {
    if (!mc_MapData_getMapInfo(map,viewer->entity_id)) return false;
    if (!contains(viewer->inventory,stack)) {
        size_t i=0; while (i<map->tracking->decorations && strcmp(map->tracking->markers[i].key,viewer->name)) i++;
        if (i<map->tracking->decorations) { memmove(&map->tracking->markers[i],&map->tracking->markers[i+1],(map->tracking->decorations-i-1)*sizeof(decoration)); map->tracking->decorations--; sync_icons(map); }
    }
    for (size_t i=0;i<map->tracking->viewers;i++) {
        const mc_map_player *p=player(players,count,map->tracking->info[i].entity_id);
        if (p && p->alive && contains(p->inventory,stack)) {
            if (p->dimension==map->dimension && !mc_MapData_updateDecorations(map,0,p->name,p->x,p->z,p->yaw,time)) return false;
        } else {
            /* ArrayList removal followed by the source loop's increment. */
            memmove(&map->tracking->info[i],&map->tracking->info[i+1],(map->tracking->viewers-i-1)*sizeof(mc_MapInfo)); map->tracking->viewers--;
        }
    }
    mc_nbt_view root,list,entry,field;
    if (stack->nbt.size && mc_nbt_root(&stack->nbt,&root) && mc_nbt_find(&root,"Decorations",&list) && list.type==9) {
        for (size_t i=0;mc_nbt_list_get(&list,i,&entry);i++) {
            if (entry.type!=10) break;
            char id[MC_MAP_DECORATION_KEY+1]="";
            if (mc_nbt_find(&entry,"id",&field) && field.type==8 && !mc_nbt_get_string(&field,id,sizeof(id))) return false;
            size_t j=0; while (j<map->tracking->decorations && strcmp(map->tracking->markers[j].key,id)) j++;
            if (j==map->tracking->decorations) {
                if (!mc_MapData_updateDecorations(map,byte_value(&entry,"type"),id,number(&entry,"x"),number(&entry,"z"),number(&entry,"rot"),time)) return false;
            }
        }
    }
    return true;
}
bool mc_MapData_updateVisiblePlayers(mc_map_info *map,const mc_slot *stack,const mc_map_player *viewer,const mc_map_player *players,size_t count,int64_t time) {
    if (!map || !stack || !viewer || !viewer->name || !players || count>MC_MAP_MAX_VIEWERS) return false;
    for (size_t i=0;i<count;i++) if (!players[i].name) return false;
    mc_map_info copy={0}; if (!mc_map_info_copy(&copy,map)) return false;
    bool okay=visible(&copy,stack,viewer,players,count,time);
    if (okay) { mc_map_info_free(map); *map=copy; mc_map_info_init(&copy); }
    mc_map_info_free(&copy); return okay;
}
void mc_MapData_updateMapData(mc_map_info *map,unsigned x,unsigned z) {
    if (!map || x>=128 || z>=128 || !map->tracking) return;
    for (size_t i=0;i<map->tracking->viewers;i++) {
        mc_MapInfo *info=&map->tracking->info[i];
        if (info->dirty) {
            if (x<info->min_x) info->min_x=(uint8_t)x;
            if (z<info->min_z) info->min_z=(uint8_t)z;
            if (x>info->max_x) info->max_x=(uint8_t)x;
            if (z>info->max_z) info->max_z=(uint8_t)z;
        } else { info->dirty=true; info->min_x=info->max_x=(uint8_t)x; info->min_z=info->max_z=(uint8_t)z; }
    }
}
int mc_MapData_getMapPacket(mc_map_info *map,const mc_slot *stack,int32_t id,mc_buf *packet) {
    if (!map || !stack || !packet) return -1;
    if (!map->tracking) return 0;
    mc_MapInfo *info=NULL;
    for (size_t i=0;i<map->tracking->viewers;i++) if (map->tracking->info[i].entity_id==id) info=&map->tracking->info[i];
    if (!info) return 0;
    bool send=info->dirty || info->packet_counter%5==0;
    if (!send) { info->packet_counter++; return 0; }
    mc_map_info wire=*map; wire.id=stack->damage;
    unsigned x=info->dirty ? info->min_x : 0,z=info->dirty ? info->min_z : 0;
    unsigned w=info->dirty ? (unsigned)info->max_x+1-x : 0,h=info->dirty ? (unsigned)info->max_z+1-z : 0;
    if (!mc_map_packet(&wire,x,z,w,h,packet)) return -1;
    if (info->dirty) info->dirty=false; else info->packet_counter++;
    return 1;
}
