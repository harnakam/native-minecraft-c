#ifndef C919_NATIVE_I_INVENTORY_H
#define C919_NATIVE_I_INVENTORY_H
#include "util/MCObjectHeap.h"
#include <stdint.h>
typedef struct ItemStack ItemStack;
/* C dispatch adapter for the original interface. Instance is the inventory
   object's identity; the table is immutable process storage, not Java state. */
typedef struct {
    ItemStack *(*getStackInSlot)(MCObject *instance,int32_t index);
    bool (*setInventorySlotContents)(MCObject *instance,int32_t index,ItemStack *stack);
    ItemStack *(*decrStackSize)(MCObject *instance,int32_t index,int32_t count);
    void (*markDirty)(MCObject *instance);
    int32_t (*getInventoryStackLimit)(const MCObject *instance);
    int32_t (*getSizeInventory)(const MCObject *instance);
} IInventoryMethods;
typedef struct { MCObject *instance; const IInventoryMethods *methods; } IInventory;
#endif
