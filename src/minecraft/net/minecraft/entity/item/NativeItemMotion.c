#include "entity/item/NativeItemMotion.h"
#include "block/block.h"
#include <math.h>
#include <limits.h>
static int32_t java_increment(int32_t value) {
    return value == INT32_MAX ? INT32_MIN : value + 1;
}
bool NativeItemMotion_positionSupported(double x, double y, double z) {
    const double limit = 67108832.0;
    return isfinite(x) && isfinite(y) && isfinite(z) && fabs(x) <= limit && fabs(y) <= limit &&
           fabs(z) <= limit;
}
bool NativeItemMotion_validate(const EntityItem *e) {
    bool valid = e && EntityItem_isInstance((const MCObject *)e) &&
                 NativeItemMotion_positionSupported(e->posX, e->posY, e->posZ) &&
                 isfinite(e->motionX) && isfinite(e->motionY) && isfinite(e->motionZ) &&
                 fabs(e->motionX) <= 16 && fabs(e->motionY) <= 16 && fabs(e->motionZ) <= 16 &&
                 isfinite(e->width) && isfinite(e->height) && e->width > 0 && e->width <= 8 &&
                 e->height > 0 && e->height <= 8;
    if (!valid)
        MCObjectHeap_fail(e ? e->object.heap : NULL);
    return valid;
}
typedef struct {
    double low[3], high[3];
} entity_box;
static entity_box bounds(const EntityItem *e) {
    entity_box box = {{e->posX - e->width * 0.5, e->posY, e->posZ - e->width * 0.5},
                      {e->posX + e->width * 0.5, e->posY + e->height, e->posZ + e->width * 0.5}};
    return box;
}
static double collision_offset(const mc_world *world, entity_box *box, unsigned axis,
                               double delta) {
    double lo[3], hi[3];
    for (unsigned i = 0; i < 3; i++) {
        lo[i] = box->low[i];
        hi[i] = box->high[i];
    }
    if (delta < 0)
        lo[axis] += delta;
    else
        hi[axis] += delta;
    /* A fence/wall reaches half a block beyond its containing Y cell. */
    int min_x = (int)floor(lo[0]), max_x = (int)floor(hi[0]);
    int min_y = (int)floor(lo[1]) - 1, max_y = (int)floor(hi[1]);
    int min_z = (int)floor(lo[2]), max_z = (int)floor(hi[2]);
    for (int y = min_y; y <= max_y; y++)
        for (int z = min_z; z <= max_z; z++)
            for (int x = min_x; x <= max_x; x++) {
                mc_box shapes[3];
                unsigned n = mc_block_collision(mc_world_get(world, x, y, z), shapes);
                for (unsigned i = 0; i < n; i++) {
                    double low[3] = {x + shapes[i].min_x, y + shapes[i].min_y, z + shapes[i].min_z};
                    double high[3] = {x + shapes[i].max_x, y + shapes[i].max_y,
                                      z + shapes[i].max_z};
                    bool overlap = true;
                    for (unsigned j = 0; j < 3; j++)
                        if (j != axis && (box->high[j] <= low[j] || box->low[j] >= high[j]))
                            overlap = false;
                    if (!overlap)
                        continue;
                    if (delta > 0 && box->high[axis] <= low[axis]) {
                        double distance = low[axis] - box->high[axis];
                        if (distance < delta)
                            delta = distance;
                    } else if (delta < 0 && box->low[axis] >= high[axis]) {
                        double distance = high[axis] - box->low[axis];
                        if (distance > delta)
                            delta = distance;
                    }
                }
            }
    box->low[axis] += delta;
    box->high[axis] += delta;
    return delta;
}
static uint32_t random_word(uint32_t n) {
    n ^= n >> 16;
    n *= UINT32_C(0x7feb352d);
    n ^= n >> 15;
    n *= UINT32_C(0x846ca68b);
    return n ^ (n >> 16);
}
static double random_signed(uint32_t seed) {
    double a = (random_word(seed) & 0xffffffu) / 16777216.0;
    double b = (random_word(seed + 1) & 0xffffffu) / 16777216.0;
    return (a - b) * 0.2;
}
static bool full_cube(uint16_t state) {
    mc_box boxes[3];
    unsigned count = mc_block_collision(state, boxes);
    return count == 1 && mc_block_opaque(state) && boxes[0].min_x == 0 && boxes[0].min_y == 0 &&
           boxes[0].min_z == 0 && boxes[0].max_x == 1 && boxes[0].max_y == 1 && boxes[0].max_z == 1;
}
static bool push_out(EntityItem *e, const mc_world *world) {
    entity_box box = bounds(e);
    bool overlaps = false;
    for (int y = (int)floor(box.low[1]) - 1; y <= (int)floor(box.high[1]); y++)
        for (int z = (int)floor(box.low[2]); z <= (int)floor(box.high[2]); z++)
            for (int x = (int)floor(box.low[0]); x <= (int)floor(box.high[0]); x++) {
                mc_box shapes[3];
                unsigned n = mc_block_collision(mc_world_get(world, x, y, z), shapes);
                for (unsigned i = 0; i < n; i++)
                    if (box.high[0] > x + shapes[i].min_x && box.low[0] < x + shapes[i].max_x &&
                        box.high[1] > y + shapes[i].min_y && box.low[1] < y + shapes[i].max_y &&
                        box.high[2] > z + shapes[i].min_z && box.low[2] < z + shapes[i].max_z)
                        overlaps = true;
            }
    int x = (int)floor(e->posX), y = (int)floor(e->posY + 0.125), z = (int)floor(e->posZ);
    if (!overlaps && !full_cube(mc_world_get(world, x, y, z)))
        return false;
    unsigned direction = 2;
    double nearest = 10000;
    const int offsets[5][3] = {{-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
    const double distances[5] = {e->posX - x, 1 - (e->posX - x), 1 - (e->posY + 0.125 - y),
                                 e->posZ - z, 1 - (e->posZ - z)};
    for (unsigned i = 0; i < 5; i++)
        if (!full_cube(
                mc_world_get(world, x + offsets[i][0], y + offsets[i][1], z + offsets[i][2])) &&
            distances[i] < nearest) {
            nearest = distances[i];
            direction = i;
        }
    double speed =
        0.1 + (random_word((uint32_t)e->entityId + e->ticksExisted) & 0xffffffu) / 16777216.0 * 0.2;
    if (direction < 2)
        e->motionX = direction == 0 ? -speed : speed;
    else if (direction == 2)
        e->motionY = speed;
    else
        e->motionZ = direction == 3 ? -speed : speed;
    return true;
}
bool NativeItemMotion_tick(EntityItem *e, const mc_world *world) {
    if (!NativeItemMotion_validate(e))
        return false;
    if (!world || !e->health || e->age >= 6000 || e->posY < -64)
        return false;
    e->ticksExisted = java_increment(e->ticksExisted);
    if (e->delayBeforeCanPickup > 0 && e->delayBeforeCanPickup != 32767)
        e->delayBeforeCanPickup--;
    double previous_x = e->posX, previous_y = e->posY, previous_z = e->posZ;
    unsigned material =
        mc_world_get(world, (int)floor(e->posX), (int)floor(e->posY), (int)floor(e->posZ)) >> 4;
    if (material == 10 || material == 11)
        e->health = e->health > 4 ? e->health - 4 : 0;
    else if ((material == 51 && e->ticksExisted % 20 == 0) || material == 81)
        e->health = e->health > 0 ? e->health - 1 : 0;
    if (!e->health)
        return false;
    e->motionY -= 0.03999999910593033;
    bool no_clip = push_out(e, world);
    e->noClip = no_clip;
    entity_box box = bounds(e);
    double old_vy = e->motionY;
    double dy = e->motionY, dx = e->motionX, dz = e->motionZ;
    if (no_clip) {
        box.low[0] += dx;
        box.high[0] += dx;
        box.low[1] += dy;
        box.high[1] += dy;
        box.low[2] += dz;
        box.high[2] += dz;
    } else {
        dy = collision_offset(world, &box, 1, e->motionY);
        dx = collision_offset(world, &box, 0, e->motionX);
        dz = collision_offset(world, &box, 2, e->motionZ);
    }
    e->posX = (box.low[0] + box.high[0]) * 0.5;
    e->posY = box.low[1];
    e->posZ = (box.low[2] + box.high[2]) * 0.5;
    e->onGround = dy != old_vy && old_vy < 0;
    if (dx != e->motionX)
        e->motionX = 0;
    if (dz != e->motionZ)
        e->motionZ = 0;
    unsigned below =
        mc_world_get(world, (int)floor(e->posX), (int)floor(e->posY) - 1, (int)floor(e->posZ)) >> 4;
    if (dy != old_vy)
        e->motionY = below == 165 && old_vy < 0 ? -old_vy : 0;
    bool moved = (int)previous_x != (int)e->posX || (int)previous_y != (int)e->posY ||
                 (int)previous_z != (int)e->posZ;
    if (moved || e->ticksExisted % 25 == 0) {
        material =
            mc_world_get(world, (int)floor(e->posX), (int)floor(e->posY), (int)floor(e->posZ)) >> 4;
        if (material == 10 || material == 11) {
            e->motionY = 0.20000000298023224;
            e->motionX = random_signed((uint32_t)e->entityId + e->ticksExisted * 4u);
            e->motionZ = random_signed((uint32_t)e->entityId + e->ticksExisted * 4u + 2u);
        }
    }
    float friction = 0.98f;
    if (e->onGround) {
        float slipperiness = (below == 79 || below == 174) ? 0.98f : below == 165 ? 0.8f : 0.6f;
        friction = slipperiness * 0.98f;
    }
    e->motionX *= friction;
    e->motionZ *= friction;
    e->motionY *= 0.9800000190734863;
    if (e->onGround)
        e->motionY *= -0.5;
    if (e->age != -32768)
        e->age = java_increment(e->age);
    return e->age < 6000 && e->posY >= -64;
}
