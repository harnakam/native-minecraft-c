#ifndef C919_ITEM_H
#define C919_ITEM_H
#include <stdbool.h>
#include <stdint.h>
bool mc_item_valid(int16_t item_id);
unsigned mc_item_stack_limit(int16_t item_id);
bool mc_item_has_subtypes(int16_t item_id);
const char *mc_item_name(int16_t item_id);
const char *mc_item_resource_name(int16_t item_id);
bool mc_item_from_resource_name(const char *name, int16_t *item_id);
/* Item -> base legacy block state. Orientation, support, multi-block placement
   and tile-entity tags are world operations handled by the caller. */
bool mc_item_block_state(int16_t item_id, int16_t damage, uint16_t *state);
/* Base creative variants, in stable legacy ID/metadata order. */
unsigned mc_item_creative_count(void);
bool mc_item_creative_at(unsigned index, int16_t *item_id, int16_t *damage);
#endif
