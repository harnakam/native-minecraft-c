#ifndef C919_CONTAINER_H
#define C919_CONTAINER_H
#include "inventory.h"

typedef enum { MC_CONTAINER_PLAYER, MC_CONTAINER_WORKBENCH } mc_container_kind;
typedef struct {
    mc_container_kind kind;
    /* Workbench result and nine inputs. Player slots and the cursor are owned
       by mc_inventory; mapped getters never introduce another owning copy. */
    mc_slot slots[10];
    bool drag_active;
    unsigned drag_mode;
    uint64_t drag_slots;
} mc_container;
void mc_container_init(mc_container *container, mc_container_kind kind);
void mc_container_free(mc_container *container);
bool mc_container_copy(mc_container *destination, const mc_container *source);
unsigned mc_container_slot_count(const mc_container *container);
mc_slot *mc_container_get(mc_inventory *player, mc_container *container, int index);
const mc_slot *mc_container_const_get(const mc_inventory *player, const mc_container *container, int index);
void mc_container_reset_drag(mc_container *container);
/* Non-output engine used by crafting. Both owners and outputs remain unchanged
   on failure. Output actions use mc_container_click in crafting/crafting.h. */
bool mc_container_inventory_click_result(mc_inventory *player, mc_container *container,
    int index, int button, int mode, mc_slot *returned, mc_slot *dropped);
#endif
