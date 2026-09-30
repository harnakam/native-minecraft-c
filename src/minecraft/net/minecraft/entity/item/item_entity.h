#ifndef C919_ITEM_ENTITY_H
#define C919_ITEM_ENTITY_H
#include "inventory/inventory.h"
#include "world/world.h"

#define MC_MAX_ITEM_ENTITIES 1024u
typedef struct {
    int32_t eid; /* The full signed protocol entity ID range, including zero. */
    double x, y, z, vx, vy, vz;
    int age, pickup_delay, health;
    bool on_ground;
    uint32_t ticks; /* Runtime tick counter; resets on loading, as vanilla does. */
    mc_slot item;
    char owner[17], thrower[17];
    mc_nbt original_nbt;
} mc_item_entity;
typedef struct {
    mc_item_entity *entries;
    size_t count, capacity;
    mc_nbt original_nbt;
} mc_item_entities;

void mc_item_entity_init(mc_item_entity *entity);
void mc_item_entity_free(mc_item_entity *entity);
bool mc_item_entity_valid(const mc_item_entity *entity);
bool mc_item_entity_copy(mc_item_entity *destination, const mc_item_entity *source);
void mc_item_entities_init(mc_item_entities *entities);
void mc_item_entities_free(mc_item_entities *entities);
bool mc_item_entities_copy(mc_item_entities *destination, const mc_item_entities *source);
mc_item_entity *mc_item_entities_find(mc_item_entities *entities, int32_t eid);
bool mc_item_entities_add(mc_item_entities *entities, const mc_item_entity *entity);
bool mc_item_entities_remove(mc_item_entities *entities, int32_t eid);

/* One 20 Hz step. False means expired, below the void, empty, or invalid.
   Collision uses the world's current legacy block boxes. The caller removes
   expired entries and persists/broadcasts its authoritative transaction. */
bool mc_item_entity_tick(mc_item_entity *entity, const mc_world *world);
bool mc_item_entity_pickup_eligible(const mc_item_entity *entity, const char *player_name);
bool mc_item_entity_pickup_near(const mc_item_entity *entity, double x, double y, double z);
/* Transactional main/hotbar insertion. Partial survival pickup leaves a
   remainder; creative mode consumes a remainder when inventory is full.
   A consumed entity has an empty Slot and must be removed by the caller.
   inserted + discarded + remaining equals the original count. */
bool mc_item_entity_pickup(mc_item_entity *entity, mc_inventory *inventory, bool creative,
                           unsigned *inserted, unsigned *discarded);
/* All-or-nothing merge into the larger stack. Owner/thrower stay with that
   entity, as in 1.8. Caller removes the empty source and sends changed metadata. */
bool mc_item_entity_merge(mc_item_entity *a, mc_item_entity *b);

/* Replace an initialized output with a full protocol47 packet (ID included).
   Send spawn, metadata, then velocity when beginning to track an item. */
bool mc_item_entity_spawn(const mc_item_entity *entity, mc_buf *output);
bool mc_item_entity_metadata(const mc_item_entity *entity, mc_buf *output);
bool mc_item_entity_velocity(const mc_item_entity *entity, mc_buf *output);
/* Version1 compound with an Entities compound list. Standard EntityItem fields
   and C919EntityId are updated while unknown root/entity/Item fields survive.
   The entire encoded snapshot must fit the shared 2 MiB NBT limit. Outputs
   are unchanged on failure; all destination objects must be initialized. */
bool mc_item_entities_encode(const mc_item_entities *entities, mc_nbt *output);
bool mc_item_entities_decode(const mc_nbt *input, mc_item_entities *output);
#endif
