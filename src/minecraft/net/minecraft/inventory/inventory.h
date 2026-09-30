#ifndef C919_INVENTORY_H
#define C919_INVENTORY_H
#include "nbt/nbt.h"
#define MC_PLAYER_INVENTORY_SIZE 45
#define MC_HOTBAR_START 36
#define MC_HOTBAR_SIZE 9
typedef struct {
    int16_t item_id;
    uint8_t count;
    int16_t damage;
    mc_nbt nbt;
} mc_slot;
/* Wire and stored counts preserve positive signed-byte values 1..127, including
   legitimate oversized creative stacks. Inventory placement uses item/slot limits. */
typedef struct {
    mc_slot slots[MC_PLAYER_INVENTORY_SIZE];
    mc_slot cursor;
    bool drag_active;
    unsigned drag_mode;
    uint64_t drag_slots;
} mc_inventory;
void mc_slot_init(mc_slot *slot);
void mc_slot_free(mc_slot *slot);
bool mc_slot_copy(mc_slot *destination, const mc_slot *source);
bool mc_slot_equal(const mc_slot *a, const mc_slot *b);
bool mc_slot_read(mc_buf *input, mc_slot *output);
bool mc_slot_write(mc_buf *output, const mc_slot *slot);
bool mc_slot_set(mc_slot *slot, int16_t item_id, uint8_t count, int16_t damage);
bool mc_slot_can_stack(const mc_slot *a, const mc_slot *b);
unsigned mc_item_stack_limit(int16_t item_id);
bool mc_item_valid(int16_t item_id);
void mc_inventory_init(mc_inventory *inventory);
void mc_inventory_free(mc_inventory *inventory);
bool mc_inventory_copy(mc_inventory *destination, const mc_inventory *source);
bool mc_inventory_accepts_slot(int slot, const mc_slot *item);
unsigned mc_inventory_slot_limit(int slot, const mc_slot *item);
/* Vanilla window 0 click semantics. No packet transport or world effects here.
   dropped receives owned items for a drop action. Returns false for invalid input.
   Callers initialize dropped and commit/broadcast accepted changes atomically. */
bool mc_inventory_click(mc_inventory *inventory, int slot, int button, int mode, mc_slot *dropped);
/* As above, with the owning vanilla transaction return value for Click Window.
   Creative clone and creative drag require caller-side creative permission.
   Slot 0 mutations are rejected until a recipe transaction supplies them. */
bool mc_inventory_click_result(mc_inventory *inventory, int slot, int button, int mode,
                               mc_slot *returned, mc_slot *dropped);
#endif
