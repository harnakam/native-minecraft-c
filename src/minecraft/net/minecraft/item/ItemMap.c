#include "ItemMap.h"
#include "util/MCGameplayPlayer.h"
#include <limits.h>
#include <math.h>
#include <string.h>
static int block_color(uint16_t state) {
    unsigned id=state>>4,meta=state&15;
    static const uint8_t wood[]={13,34,2,10,15,26,13,13};
    static const uint8_t slabs[]={11,2,13,11,28,11,35,14};
    if (id==1) return meta==1 || meta==2 ? 10 : meta==3 || meta==4 ? 14 : 11;
    if (id==3) return meta==2 ? 34 : 10;
    if (id==5) return meta<6 ? wood[meta] : 13;
    if (id==125 || id==126) return wood[meta&7];
    if (id==12) return meta==1 ? 15 : 2;
    if (id==17) { static const uint8_t bark[]={34,26,14,34}; return meta<4 ? wood[meta] : bark[meta&3]; }
    if (id==35 || id==95 || id==159 || id==160 || id==171) return meta ? (int)meta+14 : 8;
    if (id==43 || id==44) return slabs[meta&7];
    if (id==99 || id==100) return (meta>=1 && meta<=9) || meta==14 ? (id==99 ? 10 : 28) : meta==15 ? 3 : 2;
    if (id==162) return (meta&3)>1 ? -1 : (meta&3)==1 ? 26 : meta<4 ? 15 : 11;
    if (id==168) return meta==1 || meta==2 ? 31 : 23;
    switch (id) {
        case 0: case 20: case 27: case 28: case 50: case 55: case 65: case 66:
        case 69: case 75: case 76: case 77: case 90: case 92: case 93: case 94:
        case 102: case 123: case 124: case 131: case 132: case 140: case 143:
        case 144: case 149: case 150: case 157: case 166: return 0;
        case 2: case 165: return 1;
        case 24: case 89: case 121: case 128: case 135: case 184: case 189: return 2;
        case 26: case 30: return 3;
        case 10: case 11: case 46: case 51: case 152: return 4;
        case 79: case 174: return 5;
        case 42: case 71: case 101: case 117: case 145: case 148: case 167: return 6;
        case 6: case 18: case 31: case 37: case 38: case 39: case 40: case 59:
        case 81: case 83: case 104: case 105: case 106: case 111: case 127:
        case 141: case 142: case 161: case 175: return 7;
        case 78: case 80: return 8;
        case 82: case 97: return 9;
        case 60: case 84: case 136: case 185: case 190: return 10;
        case 4: case 7: case 13: case 14: case 15: case 16: case 21: case 23:
        case 29: case 33: case 34: case 36: case 48: case 52: case 56: case 61:
        case 62: case 67: case 70: case 73: case 74: case 98: case 109: case 118:
        case 129: case 130: case 139: case 154: case 158: return 11;
        case 8: case 9: return 12;
        case 25: case 32: case 47: case 53: case 54: case 58: case 63: case 64:
        case 68: case 72: case 85: case 96: case 107: case 146: case 151:
        case 176: case 177: case 178: case 193: case 194: case 195: case 196: case 197: return 13;
        case 155: case 156: case 169: return 14;
        case 86: case 91: case 137: case 163: case 172: case 179: case 180:
        case 181: case 182: case 187: case 192: return 15;
        case 19: case 170: return 18;
        case 103: return 19;
        case 110: return 24;
        case 88: case 164: case 186: case 191: return 26;
        case 120: return 27;
        case 45: case 108: case 115: case 116: return 28;
        case 49: case 119: case 122: case 173: return 29;
        case 41: case 147: return 30;
        case 57: case 138: return 31;
        case 22: return 32;
        case 133: return 33;
        case 134: case 183: case 188: return 34;
        case 87: case 112: case 113: case 114: case 153: return 35;
        default: return -1;
    }
}
static const mc_chunk *chunk_at(const mc_world *world,int x,int z) {
    int32_t cx=mc_floor_div16(x),cz=mc_floor_div16(z);
    for (int i=0;i<world->count;i++) if (world->chunks[i].x==cx && world->chunks[i].z==cz) return &world->chunks[i];
    return NULL;
}
static uint16_t chunk_block(const mc_chunk *chunk,int x,int y,int z) {
    return chunk->blocks[((unsigned)y<<8)|((unsigned)(z&15)<<4)|(unsigned)(x&15)];
}
static bool nonempty(const mc_world *world,const mc_chunk *chunk,uint8_t *cache) {
    if (!chunk || !chunk->blocks) return false;
    size_t index=(size_t)(chunk-world->chunks);
    if (!cache[index]) {
        cache[index]=1;
        for (unsigned i=0;i<MC_CHUNK_BLOCKS;i++) if (chunk->blocks[i]) { cache[index]=2; break; }
    }
    return cache[index]==2;
}
static bool sample(const mc_world *world,uint8_t *cache,int64_t x,int64_t z,unsigned size,unsigned *color,double *height,unsigned *depth) {
    unsigned counts[36]={0},first[36]; for (unsigned i=0;i<36;i++) first[i]=UINT_MAX;
    double sum=0; unsigned water=0,order=0;
    for (unsigned i=0;i<size;i++) for (unsigned j=0;j<size;j++) {
        int64_t bx=x+i,bz=z+j; if (bx<-30000000 || bx>=30000000 || bz<-30000000 || bz>=30000000) return false;
        const mc_chunk *chunk=chunk_at(world,(int)bx,(int)bz); if (!nonempty(world,chunk,cache)) return false;
        int y=255,c=0; uint16_t state=0;
        for (;y>=0;y--) { state=chunk_block(chunk,(int)bx,y,(int)bz); c=block_color(state); if (c<0) return false; if (c) break; }
        if (y<0) { y=1; c=0; state=0; }
        unsigned id=state>>4;
        if (id==8 || id==9 || id==10 || id==11) {
            int below=y-1;
            while (below>=0) {
                unsigned lower=chunk_block(chunk,(int)bx,below--,(int)bz)>>4; water++;
                if (below<=0 || (lower!=8 && lower!=9 && lower!=10 && lower!=11)) break;
            }
        }
        sum+=y;
        if (!counts[c]) first[c]=order;
        counts[c]++; order++;
    }
    unsigned most=0;
    for (unsigned c=1;c<36;c++) if (counts[c]>counts[most] || (counts[c]==counts[most] && first[c]<first[most])) most=c;
    *color=most; *height=sum/(size*size); *depth=water/(size*size); return true;
}
bool mc_ItemMap_survey(mc_map_info *map,const mc_world *world,double px,double pz,int dimension,bool no_sky,uint32_t tick,bool *changed) {
    if (!changed || !mc_map_info_valid(map) || !map->metadata_known || !world || world->count<0 || world->count>MC_MAX_CHUNKS || !isfinite(px) || !isfinite(pz)) return false;
    *changed=false;
    if (map->dimension!=dimension) return true;
    unsigned size=1u<<map->scale; int radius=128/(int)size;
    if (no_sky) radius/=2;
    uint8_t empty_cache[MC_MAX_CHUNKS]={0};
    double dx=trunc(floor(px-map->center_x)/(double)size)+64,dz=trunc(floor(pz-map->center_z)/(double)size)+64;
    if (!isfinite(dx) || !isfinite(dz) || dx<-256 || dx>384 || dz<-256 || dz>384) return true;
    int holder_x=(int)dx,holder_z=(int)dz;
    bool continuation=false;
    for (int x=holder_x-radius+1;x<holder_x+radius;x++) {
        if (((unsigned)x&15)!=(tick&15) && !continuation) continue;
        continuation=false;
        if (x<0 || x>=128) continue;
        double previous=0;
        for (int z=holder_z-radius-1;z<holder_z+radius;z++) {
            if (z<-1 || z>=128) continue;
            int ox=x-holder_x,oz=z-holder_z;
            int64_t bx=((int64_t)(map->center_x/(int)size)+x-64)*size;
            int64_t bz=((int64_t)(map->center_z/(int)size)+z-64)*size;
            unsigned color,depth; double height;
            if (no_sky) {
                const mc_chunk *chunk=chunk_at(world,(int)bx,(int)bz);
                if (!nonempty(world,chunk,empty_cache)) continue;
                uint32_t hash=(uint32_t)bx+(uint32_t)bz*231871u;
                hash=hash*hash*31287121u+hash*11u;
                color=((hash>>20)&1) ? 11u : 10u; height=100; depth=0;
            } else if (!sample(world,empty_cache,bx,bz,size,&color,&height,&depth)) continue;
            double slope=(height-previous)*4.0/(size+4.0)+(((x+z)&1)-0.5)*0.4;
            unsigned shade=slope>0.6 ? 2 : slope<-0.6 ? 0 : 1;
            if (color==12) { double water=depth*0.1+((x+z)&1)*0.2; shade=water<0.5 ? 2 : water>0.9 ? 0 : 1; }
            previous=height;
            if (z<0 || ox*ox+oz*oz>=radius*radius ||
                (ox*ox+oz*oz>(radius-2)*(radius-2) && !((x+z)&1))) continue;
            uint8_t pixel=(uint8_t)(color*4+shade);
            size_t index=(size_t)z*128+(unsigned)x;
            if (map->colors[index]!=pixel) { map->colors[index]=pixel; mc_MapData_updateMapData(map,(unsigned)x,(unsigned)z); *changed=true; continuation=true; }
        }
    }
    return true;
}
bool mc_ItemMap_updateMapData(mc_map_info *map,const mc_world *world,int32_t id,double x,double z,int dimension,bool no_sky,bool *changed) {
    if (!map || !changed) return false;
    *changed=false;
    if (map->dimension!=dimension) return true;
    mc_MapInfo *info=mc_MapData_getMapInfo(map,id); if (!info) return false;
    info->update_counter++;
    return mc_ItemMap_survey(map,world,x,z,dimension,no_sky,info->update_counter,changed);
}
mc_map_info *mc_ItemMap_getMapData(mc_maps *maps,mc_slot *stack,bool remote,int32_t spawn_x,int32_t spawn_z,int dimension) {
    if (!stack || stack->item_id!=358) return NULL;
    mc_map_info *known=mc_maps_find(maps,stack->damage);
    if (known || remote) return known;
    return mc_maps_resolve(maps,stack,spawn_x,spawn_z,dimension) ? mc_maps_find(maps,stack->damage) : NULL;
}
bool mc_ItemMap_onUpdate(mc_maps *maps,const mc_world *world,mc_slot *stack,const mc_map_player *viewer,
    const mc_map_player *players,size_t count,bool selected,int32_t spawn_x,int32_t spawn_z,bool no_sky,int64_t time,bool *changed) {
    if (!changed || !maps || !stack || !viewer) return false;
    *changed=false;
    size_t old_count=maps->count; int16_t old_damage=stack->damage;
    mc_map_info *map=mc_ItemMap_getMapData(maps,stack,false,spawn_x,spawn_z,viewer->dimension);
    if (!map || !mc_MapData_updateVisiblePlayers(map,stack,viewer,players,count,time)) return false;
    bool terrain=false;
    if (selected && !mc_ItemMap_updateMapData(map,world,viewer->entity_id,viewer->x,viewer->z,viewer->dimension,no_sky,&terrain)) return false;
    *changed=terrain || old_count!=maps->count || old_damage!=stack->damage; return true;
}
int mc_ItemMap_createMapDataPacket(mc_maps *maps,const mc_slot *stack,int32_t id,mc_buf *packet) {
    if (!stack || stack->item_id!=358) return -1;
    mc_map_info *map=mc_maps_find(maps,stack->damage);
    return map ? mc_MapData_getMapPacket(map,stack,id,packet) : -1;
}
int mc_ItemMap_createMapDataPacket_at(mc_maps *maps,mc_slot *stack,int32_t id,int32_t spawn_x,int32_t spawn_z,int dimension,mc_buf *packet) {
    mc_map_info *map=mc_ItemMap_getMapData(maps,stack,false,spawn_x,spawn_z,dimension);
    return map ? mc_MapData_getMapPacket(map,stack,id,packet) : -1;
}
bool ItemMap_updateMapData(MCGameplayWorld *world,MCObject *viewer,mc_map_info *map,bool *changed) {
    MCObjectHeap *h=world ? world->object.heap : NULL;
    if (!changed || !MCGameplayWorld_isInstance((MCObject *)world) || !map ||
        (viewer && viewer->heap!=h)) { MCObjectHeap_fail(h); return false; }
    *changed=false;
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,h)) return false;
    /* Source dimension and no-sky getters precede MapInfo allocation. The
       subsequent dense terrain survey remains the explicit native adapter. */
    WorldProvider *provider=world->provider;bool ok=false;
    if(!WorldProvider_isInstance((MCObject *)provider)||provider->object.heap!=h){MCObjectHeap_fail(h);goto done;}
    int32_t dimension=WorldProvider_getDimensionId(provider);if(MCObjectHeap_failed(h))goto done;
    if(dimension!=map->dimension||!MCGameplayPlayer_isInstance(viewer)){ok=true;goto done;}
    provider=world->provider;
    if(!WorldProvider_isInstance((MCObject *)provider)||provider->object.heap!=h){MCObjectHeap_fail(h);goto done;}
    bool no_sky=WorldProvider_getHasNoSky(provider);if(MCObjectHeap_failed(h))goto done;
    MCGameplayPlayer *p=(MCGameplayPlayer *)viewer;
    mc_MapInfo *info=NativeMapData_getMapInfo(map,world,p);
    ok=info!=NULL;
    if (ok) {
        ++info->update_counter; MCObjectHeap_touch(h);
        ok=mc_ItemMap_survey(map,world->terrain,p->living.entity.posX,p->living.entity.posZ,dimension,no_sky,info->update_counter,changed);
    }
    if (!ok) MCObjectHeap_fail(h);
done:
    MCObjectRootScope_end(&scope); return ok && !MCObjectHeap_failed(h);
}
bool ItemMap_onUpdate(ItemStack *stack,MCGameplayWorld *world,MCObject *entity,int32_t slot,bool selected,bool *changed) {
    (void)slot;
    MCObjectHeap *h=world ? world->object.heap : NULL;
    if (!changed || !MCGameplayWorld_isInstance((MCObject *)world)) { MCObjectHeap_fail(h); return false; }
    *changed=false;
    if (world->isRemote) return !MCObjectHeap_failed(h);
    if (!ItemStack_isInstance((MCObject *)stack) || stack->object.heap!=h || (entity && entity->heap!=h)) { MCObjectHeap_fail(h); return false; }
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,h)) return false;
    bool missing=mc_maps_find(&world->maps,ItemStack_getMetadata(stack))==NULL;
    mc_map_info *map=ItemMap_getMapData(stack,world); bool ok=map!=NULL,terrain=false;
    if (ok && MCGameplayPlayer_isInstance(entity)) ok=NativeMapData_updateVisiblePlayers(map,world,(MCGameplayPlayer *)entity,stack);
    if (ok && selected) ok=ItemMap_updateMapData(world,entity,map,&terrain);
    if (ok) *changed=missing || terrain;
    if (!ok) MCObjectHeap_fail(h);
    MCObjectRootScope_end(&scope); return ok && !MCObjectHeap_failed(h);
}
int ItemMap_createMapDataPacket(ItemStack *stack,MCGameplayWorld *world,MCGameplayPlayer *p,mc_buf *packet) {
    MCObjectHeap *h=world ? world->object.heap : NULL; MCObjectRootScope scope={0};
    if (!MCGameplayWorld_isInstance((MCObject *)world) || !MCObjectRootScope_begin(&scope,h)) { MCObjectHeap_fail(h); return -1; }
    mc_map_info *map=ItemMap_getMapData(stack,world);
    int result=map ? NativeMapData_getMapPacket(map,stack,world,p,packet) : -1;
    if (result<0) MCObjectHeap_fail(h);
    MCObjectRootScope_end(&scope); return result;
}
