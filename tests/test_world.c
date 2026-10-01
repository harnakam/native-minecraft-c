#include "world/NativeWorld.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
int main(void) {
    mc_world a, b;
    char error[160];
    const char *path = "world-test.c919";
    mc_world_init(&a, 919); mc_world_init(&b, 1);
    CHECK(mc_floor_div16(-1) == -1 && mc_floor_div16(-16) == -1 && mc_floor_div16(-17) == -2);
    CHECK(mc_world_set(&a, -1, 255, -17, 20 << 4));
    CHECK(mc_world_get(&a, -1, 255, -17) == (20 << 4));
    CHECK(mc_world_get(&a, 0, -1, 0) == 0);
    CHECK(!mc_world_set(&a, 0, 256, 0, 16));
    CHECK(!mc_world_set(&a, 30000001, 20, 0, 16));
    mc_world_generate(&a, 0, 0); mc_world_generate(&b, 0, 0);
    CHECK(memcmp(a.chunks[1].blocks, b.chunks[0].blocks, MC_CHUNK_BLOCKS * sizeof(uint16_t)) != 0);
    mc_world_free(&b); mc_world_init(&b, 919); mc_world_generate(&b, 0, 0);
    CHECK(memcmp(a.chunks[1].blocks, b.chunks[0].blocks, MC_CHUNK_BLOCKS * sizeof(uint16_t)) == 0);
    CHECK(mc_world_get(&a, 0, 0, 0) == (7 << 4));
    CHECK(mc_world_surface(&a, 8, 8) > 5);
    CHECK(mc_world_save(&a, path, error, sizeof(error)));
    CHECK(mc_world_load(&b, path, error, sizeof(error)));
    CHECK(b.seed == 919 && b.count == a.count);
    CHECK(mc_world_get(&b, -1, 255, -17) == (20 << 4));
    FILE *f = fopen(path, "wb"); CHECK(f != NULL); fputs("broken", f); fclose(f);
    CHECK(!mc_world_load(&b, path, error, sizeof(error)));
    CHECK(mc_world_get(&b, -1, 255, -17) == (20 << 4));
    CHECK(!mc_world_save(&b, "nonexistent-dir/world.c919", error, sizeof(error)));
    mc_world_unload(&a, -1, -2); CHECK(mc_world_get(&a, -1, 255, -17) == 0);
    mc_world_free(&a); mc_world_free(&b); remove(path);
    puts("world boundaries, deterministic terrain, persistence and corrupt-load rollback passed");
    return 0;
}
