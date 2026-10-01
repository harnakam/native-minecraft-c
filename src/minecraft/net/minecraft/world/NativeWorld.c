#include "NativeWorld.h"
#include <limits.h>
#include "block/block.h"
#include <stdlib.h>
#include <string.h>

int mc_floor_div16(int value) { return value / 16 - (value % 16 < 0); }
static bool valid_chunk(int32_t x, int32_t z) {
    return x >= -1875000 && x < 1875000 && z >= -1875000 && z < 1875000;
}
static bool valid_block(int x, int y, int z) {
    return x >= -30000000 && x < 30000000 && z >= -30000000 && z < 30000000 && y >= 0 && y < 256;
}
static size_t block_index(int x, int y, int z) {
    return ((size_t)y << 8) | ((size_t)(z & 15) << 4) | (size_t)(x & 15);
}
void mc_world_init(mc_world *w, uint32_t seed) { memset(w, 0, sizeof(*w)); w->seed = seed; }
void mc_world_free(mc_world *w) {
    for (int i = 0; i < w->count; ++i) free(w->chunks[i].blocks);
    memset(w, 0, sizeof(*w));
}
mc_chunk *mc_world_chunk(mc_world *w, int32_t x, int32_t z, bool create) {
    if (!valid_chunk(x, z)) return NULL;
    for (int i = 0; i < w->count; ++i) if (w->chunks[i].x == x && w->chunks[i].z == z) return &w->chunks[i];
    if (!create || w->count == MC_MAX_CHUNKS) return NULL;
    uint16_t *blocks = calloc(MC_CHUNK_BLOCKS, sizeof(*blocks));
    if (!blocks) return NULL;
    mc_chunk *c = &w->chunks[w->count++];
    memset(c,0,sizeof(*c));
    c->x = x; c->z = z; c->blocks = blocks; c->revision = 1;
    return c;
}
void mc_world_unload(mc_world *w, int32_t x, int32_t z) {
    for (int i = 0; i < w->count; ++i) if (w->chunks[i].x == x && w->chunks[i].z == z) {
        free(w->chunks[i].blocks);
        memmove(&w->chunks[i], &w->chunks[i+1], (size_t)(w->count-i-1) * sizeof(mc_chunk));
        --w->count; return;
    }
}
uint16_t mc_world_get(const mc_world *w, int x, int y, int z) {
    if (!valid_block(x, y, z)) return 0;
    int cx = mc_floor_div16(x), cz = mc_floor_div16(z);
    for (int i = 0; i < w->count; ++i) if (w->chunks[i].x == cx && w->chunks[i].z == cz)
        return w->chunks[i].blocks[block_index(x, y, z)];
    return 0;
}
bool mc_world_set(mc_world *w, int x, int y, int z, uint16_t state) {
    if (!valid_block(x, y, z)||!mc_block_valid(state)) return false;
    mc_chunk *c = mc_world_chunk(w, mc_floor_div16(x), mc_floor_div16(z), true);
    if (!c) return false;
    size_t index = block_index(x, y, z);
    if (c->blocks[index] != state) {
        c->blocks[index] = state;++c->revision;
        if(state>>4)c->sectionMask|=(uint16_t)(1u<<(y>>4));
        int32_t height=0;
        for(int scan=255;scan>=0;scan--) {
            int32_t opacity;
            if(!mc_block_light_opacity(c->blocks[block_index(x,scan,z)],&opacity))return false;
            if(opacity>0){height=scan+1;break;}
        }
        c->heightMap[((z&15)<<4)|(x&15)]=(uint16_t)height;
    }
    return true;
}
bool mc_world_solid(uint16_t state) {
    unsigned id = state >> 4;
    return id != 0 && id != 8 && id != 9 && id != 10 && id != 11 && id != 31 && id != 32 && id != 37 && id != 38;
}
static uint32_t terrain_hash(uint32_t x, uint32_t z, uint32_t seed) {
    uint32_t h = x * 0x9e3779b9u ^ z * 0x85ebca6bu ^ seed;
    h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; return h ^ (h >> 16);
}
static int height_at(int x, int z, uint32_t seed) {
    int cx = mc_floor_div16(x), cz = mc_floor_div16(z), lx = x & 15, lz = z & 15;
    int h00 = 11 + (int)(terrain_hash((uint32_t)cx, (uint32_t)cz, seed) % 9);
    int h10 = 11 + (int)(terrain_hash((uint32_t)cx+1, (uint32_t)cz, seed) % 9);
    int h01 = 11 + (int)(terrain_hash((uint32_t)cx, (uint32_t)cz+1, seed) % 9);
    int h11 = 11 + (int)(terrain_hash((uint32_t)cx+1, (uint32_t)cz+1, seed) % 9);
    return ((h00*(16-lx)+h10*lx)*(16-lz)+(h01*(16-lx)+h11*lx)*lz)/256;
}
void mc_world_generate(mc_world *w, int32_t cx, int32_t cz) {
    mc_chunk *c = mc_world_chunk(w, cx, cz, true);
    if (!c) return;
    memset(c->blocks, 0, MC_CHUNK_BLOCKS * sizeof(uint16_t));
    for (int z = 0; z < 16; ++z) for (int x = 0; x < 16; ++x) {
        int h = height_at(cx*16+x, cz*16+z, w->seed);
        for (int y = 0; y <= h; ++y) {
            unsigned id = y == 0 ? 7u : y == h ? 2u : y >= h-3 ? 3u : 1u;
            c->blocks[block_index(x,y,z)] = (uint16_t)(id << 4);
        }
    }
    /* Trees are generated from absolute candidate positions, including neighbours,
       so chunk generation order cannot change their canopy boundaries. */
    for (int tz = cz*16-2; tz < cz*16+18; ++tz) for (int tx = cx*16-2; tx < cx*16+18; ++tx) {
        if ((tx & 7) != 3 || (tz & 7) != 3 || terrain_hash((uint32_t)tx,(uint32_t)tz,w->seed+71) % 5) continue;
        int top = height_at(tx,tz,w->seed)+5;
        for (int y = top-2; y <= top+1; ++y) for (int dz = -2; dz <= 2; ++dz) for (int dx = -2; dx <= 2; ++dx) {
            int x = tx+dx-cx*16, z = tz+dz-cz*16;
            if (x < 0 || x >= 16 || z < 0 || z >= 16 || (y == top+1 && abs(dx)+abs(dz)>1)) continue;
            size_t i = block_index(x,y,z); if (!c->blocks[i]) c->blocks[i] = 18 << 4;
        }
        if (tx >= cx*16 && tx < cx*16+16 && tz >= cz*16 && tz < cz*16+16)
            for (int y = top-4; y <= top; ++y) c->blocks[block_index(tx-cx*16,y,tz-cz*16)] = 17 << 4;
    }
    (void)mc_world_refresh_chunk_metadata(c,true);
    ++c->revision;
}
int mc_world_surface(const mc_world *w, int x, int z) {
    for (int y = 255; y >= 0; --y) if (mc_world_solid(mc_world_get(w,x,y,z))) return y;
    return -1;
}

/* Metadata is real store state used by Source World read leaves. Legacy native
   saves lack allocated air-only sections; their import explicitly reconstructs
   those section identities from blocks rather than claiming Source Chunk saves. */
bool mc_world_refresh_chunk_metadata(mc_chunk *chunk,bool reconstructSectionMask) {
    if(!chunk||!chunk->blocks)return false;
    if(reconstructSectionMask)chunk->sectionMask=0;
    for(int z=0;z<16;z++)for(int x=0;x<16;x++) {
        int32_t height=0;
        for(int y=255;y>=0;y--) {
            uint16_t state=chunk->blocks[block_index(x,y,z)];int32_t opacity;
            if(!mc_block_light_opacity(state,&opacity))return false;
            if(reconstructSectionMask&&(state>>4))chunk->sectionMask|=(uint16_t)(1u<<(y>>4));
            if(!height&&opacity>0)height=y+1;
        }
        chunk->heightMap[(z<<4)|x]=(uint16_t)height;
    }
    return true;
}
