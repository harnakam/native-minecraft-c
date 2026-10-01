#ifndef C919_NATIVE_SLOT_H
#define C919_NATIVE_SLOT_H
#include "inventory/IInventory.h"
typedef struct Slot Slot;
/* Native virtual dispatch for subclass overrides. NULL entries invoke the
   corresponding original base body, including its intentionally empty hooks. */
typedef struct {
    void (*onCraftingAmount)(Slot *slot,ItemStack *stack,int32_t amount);
    void (*onCrafting)(Slot *slot,ItemStack *stack);
    bool (*onPickupFromSlot)(Slot *slot,MCObject *player,ItemStack *stack);
    bool (*isItemValid)(const Slot *slot,const ItemStack *stack);
    int32_t (*getSlotStackLimit)(const Slot *slot);
    int32_t (*getItemStackLimit)(const Slot *slot,const ItemStack *stack);
    ItemStack *(*decrStackSize)(Slot *slot,int32_t count);
    bool (*canTakeStack)(const Slot *slot,const MCObject *player);
    bool (*canBeHovered)(const Slot *slot);
    const char *(*getSlotTexture)(const Slot *slot);
} SlotOverrides;
struct Slot {
    MCObject object;
    int32_t slotIndex;
    IInventory inventory;
    int32_t slotNumber,xDisplayPosition,yDisplayPosition;
    const SlotOverrides *overrides;
};
Slot *Slot_new(MCObjectHeap *heap,IInventory inventory,int32_t index,int32_t x,int32_t y);
/* Used by translated subclasses whose first member is Slot. */
bool Slot_construct(Slot *slot,IInventory inventory,int32_t index,int32_t x,int32_t y,
                    const SlotOverrides *overrides);
void Slot_trace(Slot *slot,MCObjectVisitor visitor,void *context);
void Slot_onSlotChange(Slot *slot,ItemStack *before,ItemStack *after);
void Slot_onCraftingAmount(Slot *slot,ItemStack *stack,int32_t amount);
void Slot_onCrafting(Slot *slot,ItemStack *stack);
bool Slot_onPickupFromSlot(Slot *slot,MCObject *player,ItemStack *stack);
bool Slot_isItemValid(const Slot *slot,const ItemStack *stack);
ItemStack *Slot_getStack(Slot *slot);
bool Slot_getHasStack(Slot *slot);
bool Slot_putStack(Slot *slot,ItemStack *stack);
void Slot_onSlotChanged(Slot *slot);
int32_t Slot_getSlotStackLimit(const Slot *slot);
int32_t Slot_getItemStackLimit(const Slot *slot,const ItemStack *stack);
const char *Slot_getSlotTexture(const Slot *slot);
ItemStack *Slot_decrStackSize(Slot *slot,int32_t count);
ItemStack *Slot_decrStackSizeBase(Slot *slot,int32_t count);
bool Slot_isHere(const Slot *slot,IInventory inventory,int32_t index);
bool Slot_canTakeStack(const Slot *slot,const MCObject *player);
bool Slot_canBeHovered(const Slot *slot);
#endif
