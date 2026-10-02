#include "NativeMapData.h"
#include "world/map.h"
#include "util/MCGameplayPlayer.h"
#include "nbt/NBTTagCompound.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char key[MC_MAP_DECORATION_KEY+1]; mc_map_icon icon; } decoration;
typedef struct { MCObject object; MCGameplayPlayer *player; mc_MapInfo info; } source_info;
typedef struct { MCGameplayPlayer *player; int32_t hash; source_info *info; } source_key;
typedef struct { NBTString *key; mc_map_icon icon; } source_decoration;
struct mc_MapData_tracking {
    size_t viewers,decorations;
    mc_MapInfo info[MC_MAP_MAX_VIEWERS];
    decoration markers[MC_MAX_MAP_ICONS];
    size_t source_viewers,source_keys,source_decorations;
    source_info *source_info[MC_MAP_MAX_VIEWERS];
    source_key source_key[MC_MAP_MAX_VIEWERS];
    source_decoration source_markers[MC_MAX_MAP_ICONS];
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
/* Original updateDecorations arithmetic; only its Vec4b wire view is native. */
static bool icon_value(const mc_map_info *map,int *type,double x,double z,double yaw,int64_t time,mc_map_icon *icon) {
    float dx=(float)(x-map->center_x)/(float)(1u<<map->scale),dz=(float)(z-map->center_z)/(float)(1u<<map->scale);
    uint8_t xb=(uint8_t)java_int((double)(dx*2.0f)+.5),zb=(uint8_t)java_int((double)(dz*2.0f)+.5),direction;
    if (dx>=-63 && dz>=-63 && dx<=63 && dz<=63) {
        yaw+=yaw<0 ? -8 : 8; direction=(uint8_t)java_int(yaw*16/360);
        if (map->dimension<0) {
            uint32_t t=(uint32_t)(time/10);
            direction=(uint8_t)(((t*t*UINT32_C(34187121)+t*121u)>>15)&15u);
        }
    } else {
        /* The target's positive near-range comparisons also remove NaN. */
        if (!(fabsf(dx)<320 && fabsf(dz)<320)) {
            return false;
        }
        *type=6; direction=0;
        if (dx<=-63) xb=128;
        if (dz<=-63) zb=128;
        if (dx>=63) xb=127;
        if (dz>=63) zb=127;
    }
    icon->type=(uint8_t)*type&15u; icon->direction=direction&15u;
    memcpy(&icon->x,&xb,1); memcpy(&icon->z,&zb,1); return true;
}
bool mc_MapData_updateDecorations(mc_map_info *map,int type,const char *id,double x,double z,double yaw,int64_t time) {
    if (!mc_map_info_valid(map) || !id || strlen(id)>MC_MAP_DECORATION_KEY || !tracking(map)) return false;
    mc_MapData_tracking *data=map->tracking; size_t index=0;
    while (index<data->decorations && strcmp(data->markers[index].key,id)) index++;
    mc_map_icon icon;
    if (!icon_value(map,&type,x,z,yaw,time,&icon)) {
        if (index<data->decorations) { memmove(&data->markers[index],&data->markers[index+1],(data->decorations-index-1)*sizeof(data->markers[0])); data->decorations--; }
        sync_icons(map); return true;
    }
    if (index==data->decorations) {
        if (data->decorations==MC_MAX_MAP_ICONS) return false;
        strcpy(data->markers[index].key,id); data->decorations++;
    }
    data->markers[index].icon=icon; sync_icons(map); return true;
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
    if (!map || x>=128 || z>=128) return;
    map->dirty=true;
    if (!map->tracking) return;
    size_t legacy=map->tracking->viewers;
    for (size_t i=0;i<legacy+map->tracking->source_viewers;i++) {
        mc_MapInfo *info=i<legacy ? &map->tracking->info[i] : &map->tracking->source_info[i-legacy]->info;
        if (info->dirty) {
            if (x<info->min_x) info->min_x=(uint8_t)x;
            if (z<info->min_z) info->min_z=(uint8_t)z;
            if (x>info->max_x) info->max_x=(uint8_t)x;
            if (z>info->max_z) info->max_z=(uint8_t)z;
        } else { info->dirty=true; info->min_x=info->max_x=(uint8_t)x; info->min_z=info->max_z=(uint8_t)z; }
    }
}

void NativeMapData_traceReferences(mc_maps *maps,MCObjectVisitor visit,void *context) {
    for (size_t m=0;m<maps->count;m++) {
        mc_MapData_tracking *t=maps->entries[m].tracking;
        if (!t) continue;
        for (size_t i=0;i<t->source_viewers;i++)
            t->source_info[i]=(source_info *)visit((MCObject *)t->source_info[i],context);
        for (size_t i=0;i<t->source_keys;i++) {
            t->source_key[i].player=(MCGameplayPlayer *)visit((MCObject *)t->source_key[i].player,context);
            t->source_key[i].info=(source_info *)visit((MCObject *)t->source_key[i].info,context);
        }
        for (size_t i=0;i<t->source_decorations;i++)
            t->source_markers[i].key=(NBTString *)visit((MCObject *)t->source_markers[i].key,context);
    }
}
static bool source_view(mc_map_info *map,MCGameplayWorld *world) {
    if (!MCGameplayWorld_isInstance((MCObject *)world) || !mc_map_info_valid(map)) return false;
    return mc_maps_find(&world->maps,map->id)==map;
}
static size_t source_key_index(mc_MapData_tracking *t,const MCGameplayPlayer *p) {
    size_t i=0; int32_t hash=p ? p->living.entity.entityId : 0;
    /* Source Entity.equals/hashCode uses entityId; HashMap retains its original
       node hash and key reference even if a public entity ID later changes. */
    while (i<t->source_keys && !(t->source_key[i].hash==hash &&
        (t->source_key[i].player==p || (p && t->source_key[i].player && t->source_key[i].player->living.entity.entityId==p->living.entity.entityId)))) ++i;
    return i;
}
static source_info *find_source_info(mc_map_info *map,MCGameplayPlayer *p) {
    if (!map->tracking) return NULL;
    size_t i=source_key_index(map->tracking,p);
    return i<map->tracking->source_keys ? map->tracking->source_key[i].info : NULL;
}
static void source_info_trace(MCObject *object,MCObjectVisitor visit,void *context) {
    source_info *info=(source_info *)object;
    info->player=(MCGameplayPlayer *)visit((MCObject *)info->player,context);
}
static const MCObjectClass source_info_class={"native.MapInfoSourceView",MCObjectHeap_plainClone,source_info_trace,NULL};
mc_MapInfo *NativeMapData_getMapInfo(mc_map_info *map,MCGameplayWorld *owner,MCGameplayPlayer *p) {
    MCObjectHeap *h=owner ? owner->object.heap : p ? p->living.entity.object.heap : NULL;
    if (!source_view(map,owner) || (p && (!MCGameplayPlayer_isInstance((MCObject *)p) || p->living.entity.object.heap!=h)) || !tracking(map)) {
        MCObjectHeap_fail(h); return NULL;
    }
    source_info *known=find_source_info(map,p);
    if (known) return &known->info;
    mc_MapData_tracking *t=map->tracking;
    if (t->source_viewers==MC_MAP_MAX_VIEWERS || t->source_keys==MC_MAP_MAX_VIEWERS) { MCObjectHeap_fail(h); return NULL; }
    source_info *info=(source_info *)MCObjectHeap_alloc(h,sizeof(*info),&source_info_class);
    if (!info) return NULL;
    info->player=p; info->info.entity_id=p ? p->living.entity.entityId : 0;
    info->info.dirty=true; info->info.max_x=info->info.max_z=127;
    t->source_info[t->source_viewers++]=info;
    t->source_key[t->source_keys++]=(source_key){p,p ? p->living.entity.entityId : 0,info};
    MCObjectHeap_touch(h); return &info->info;
}
static size_t key_index(const mc_MapData_tracking *t,const NBTString *key) {
    size_t i=0;
    while (i<t->source_decorations && !NBTString_equals(t->source_markers[i].key,key)) ++i;
    return i;
}
static void source_sync(mc_map_info *map) {
    mc_MapData_tracking *t=map->tracking;
    map->icon_count=t->source_decorations;
    for (size_t i=0;i<map->icon_count;i++) map->icons[i]=t->source_markers[i].icon;
}
static void source_remove(mc_map_info *map,const NBTString *key) {
    mc_MapData_tracking *t=map->tracking; size_t i=key_index(t,key);
    if (i<t->source_decorations) {
        memmove(&t->source_markers[i],&t->source_markers[i+1],(t->source_decorations-i-1)*sizeof(source_decoration));
        --t->source_decorations;
    }
    source_sync(map);
}
static bool source_decorate(mc_map_info *map,MCGameplayWorld *world,int type,NBTString *key,double x,double z,double rot) {
    MCObjectHeap *h=world->object.heap;
    if (key&&(!NBTString_isInstance((MCObject *)key)||((MCObject *)key)->heap!=h)) return false;
    mc_MapData_tracking *t=map->tracking; size_t i=key_index(t,key); mc_map_icon icon;
    MCObjectHeap_touch(h);
    int64_t time=0;
    if(map->dimension<0) {
        float dx=(float)(x-map->center_x)/(float)(1u<<map->scale),dz=(float)(z-map->center_z)/(float)(1u<<map->scale);
        if(dx>=-63&&dz>=-63&&dx<=63&&dz<=63) {
            WorldInfo *info=World_getWorldInfo(world);
            if(!info){MCObjectHeap_fail(h);return false;}
            time=WorldInfo_getWorldTime(info);if(MCObjectHeap_failed(h))return false;
        }
    }
    if (!icon_value(map,&type,x,z,rot,time,&icon)) { source_remove(map,key); return true; }
    if (i==t->source_decorations) {
        if (i==MC_MAX_MAP_ICONS) return false;
        t->source_markers[i].key=key; ++t->source_decorations;
    }
    t->source_markers[i].icon=icon; source_sync(map); return true;
}
static bool valid_source_player(MCGameplayPlayer *p,MCObjectHeap *h) {
    return MCGameplayPlayer_isInstance((MCObject *)p) && p->living.entity.object.heap==h &&
        MCGameplayWorld_isInstance((MCObject *)((MCGameplayWorld *)(p->living.entity.worldObj))) && ((MCGameplayWorld *)(p->living.entity.worldObj))->object.heap==h &&
        p->inventory && p->inventory->object.heap==h && p->inventory->mainInventory && p->inventory->armorInventory;
}
bool NativeMapData_updateVisiblePlayers(mc_map_info *map,MCGameplayWorld *owner,MCGameplayPlayer *p,ItemStack *stack) {
    MCObjectHeap *h=owner ? owner->object.heap : NULL; MCObjectRootScope scope={0};
    if (!source_view(map,owner) || !ItemStack_isInstance((MCObject *)stack) || stack->object.heap!=h ||
        !MCObjectRootScope_begin(&scope,h)) { MCObjectHeap_fail(h); return false; }
    /* Original registration precedes inventory/name reads and frame access. */
    bool ok=NativeMapData_getMapInfo(map,owner,p)!=NULL;
    if (ok) ok=valid_source_player(p,h);
    if (ok) {
        MCObjectHeap_touch(h);
        if (!InventoryPlayer_hasItemStack(p->inventory,stack)) {
            NBTString *name=EntityPlayer_getName(p);
            if(MCObjectHeap_failed(h))ok=false;else source_remove(map,name);
        }
        mc_MapData_tracking *t=map->tracking;
        for (size_t i=0;i<t->source_viewers && ok;i++) {
            MCGameplayPlayer *viewer=t->source_info[i]->player;
            if (!valid_source_player(viewer,h)) { ok=false; break; }
            if (!viewer->living.entity.isDead && (InventoryPlayer_hasItemStack(viewer->inventory,stack) || stack->itemFrame)) {
                if (!stack->itemFrame && viewer->living.entity.dimension==map->dimension) {
                    MCGameplayWorld *world=(MCGameplayWorld *)viewer->living.entity.worldObj;
                    NBTString *name=EntityPlayer_getName(viewer);
                    ok=!MCObjectHeap_failed(h)&&source_decorate(map,world,0,name,viewer->living.entity.posX,viewer->living.entity.posZ,(double)viewer->living.entity.rotationYaw);
                }
            } else {
                /* Source ArrayList.remove then for-loop increment skips the
                   shifted successor, including consecutive dead players. */
                size_t k=source_key_index(t,viewer);
                if (k<t->source_keys) {
                    memmove(&t->source_key[k],&t->source_key[k+1],(t->source_keys-k-1)*sizeof(source_key));
                    --t->source_keys;
                }
                memmove(&t->source_info[i],&t->source_info[i+1],(t->source_viewers-i-1)*sizeof(*t->source_info));
                --t->source_viewers;
            }
        }
        if (ok && stack->itemFrame) ok=false; /* Required unported EntityItemFrame. */
    }
    if (ok && ItemStack_hasTagCompound(stack) && NBTTagCompound_hasKeyType_ascii(stack->stackTagCompound,"Decorations",9)) {
        NBTTagList *list=NBTTagCompound_getTagList_ascii(stack->stackTagCompound,"Decorations",10);
        if (!list) ok=false;
        for (int32_t i=0;ok && i<NBTTagList_tagCount(list);i++) {
            NBTTagCompound *entry=NBTTagList_getCompoundTagAt(list,i);
            NBTString *key=entry ? NBTTagCompound_getString_ascii(entry,"id") : NULL;
            if (!key) { ok=false; break; }
            if (key_index(map->tracking,key)==map->tracking->source_decorations)
                ok=source_decorate(map,((MCGameplayWorld *)(p->living.entity.worldObj)),NBTTagCompound_getByte_ascii(entry,"type"),key,
                    NBTTagCompound_getDouble_ascii(entry,"x"),NBTTagCompound_getDouble_ascii(entry,"z"),NBTTagCompound_getDouble_ascii(entry,"rot"));
        }
    }
    ok=ok && !MCObjectHeap_failed(h);
    if (!ok) MCObjectHeap_fail(h);
    MCObjectRootScope_end(&scope); return ok;
}
int NativeMapData_getMapPacket(mc_map_info *map,ItemStack *stack,MCGameplayWorld *world,MCGameplayPlayer *p,mc_buf *packet) {
    MCObjectHeap *h=world ? world->object.heap : NULL;
    if (!source_view(map,world) || !ItemStack_isInstance((MCObject *)stack) || stack->object.heap!=h ||
        (p && (!MCGameplayPlayer_isInstance((MCObject *)p) || p->living.entity.object.heap!=h)) || !packet || MCObjectHeap_failed(h)) {
        MCObjectHeap_fail(h); return -1;
    }
    source_info *s=find_source_info(map,p); if (!s) return 0;
    mc_MapInfo *info=&s->info; int32_t counter; memcpy(&counter,&info->packet_counter,sizeof(counter));
    bool send=info->dirty || counter%5==0;
    if (!send) { ++info->packet_counter; MCObjectHeap_touch(h); return 0; }
    mc_map_info wire=*map; wire.id=ItemStack_getMetadata(stack);
    unsigned x=info->dirty ? info->min_x : 0,z=info->dirty ? info->min_z : 0;
    unsigned w=info->dirty ? (unsigned)info->max_x+1-x : 0,height=info->dirty ? (unsigned)info->max_z+1-z : 0;
    if (!mc_map_packet(&wire,x,z,w,height,packet)) { MCObjectHeap_fail(h); return -1; }
    if (info->dirty) info->dirty=false; else ++info->packet_counter;
    MCObjectHeap_touch(h); return 1;
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
