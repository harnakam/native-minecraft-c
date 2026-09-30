#ifndef C919_WORLD_H
#define C919_WORLD_H
#include <stdbool.h>
#include <stdint.h>
#define MC_CHUNK_HEIGHT 256
#define MC_CHUNK_BLOCKS 65536
#define MC_MAX_CHUNKS 1024
typedef struct {
    int32_t x, z;
    uint16_t *blocks;
    unsigned revision;
} mc_chunk;
typedef struct {
    mc_chunk chunks[MC_MAX_CHUNKS];
    int count;
    uint32_t seed;
} mc_world;
void mc_world_init(mc_world *world, uint32_t seed);
void mc_world_free(mc_world *world);
mc_chunk *mc_world_chunk(mc_world *world, int32_t x, int32_t z, bool create);
void mc_world_unload(mc_world *world, int32_t x, int32_t z);
uint16_t mc_world_get(const mc_world *world, int x, int y, int z);
bool mc_world_set(mc_world *world, int x, int y, int z, uint16_t state);
void mc_world_generate(mc_world *world, int32_t cx, int32_t cz);
int mc_world_surface(const mc_world *world, int x, int z);
bool mc_world_save(const mc_world *world, const char *path, char *error, unsigned error_size);
bool mc_world_load(mc_world *world, const char *path, char *error, unsigned error_size);
bool mc_world_solid(uint16_t state);
int mc_floor_div16(int value);
#endif
