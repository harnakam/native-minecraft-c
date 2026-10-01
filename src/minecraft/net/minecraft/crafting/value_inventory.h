#ifndef C919_VALUE_INVENTORY_H
#define C919_VALUE_INVENTORY_H
#include "inventory/inventory.h"
/* Existing native value-view adapter. This preserves the current runtime
   during owner migration; it is NOT the translated InventoryCrafting/Result.
   The actual named classes own nullable ItemStack references. */
typedef struct mc_value_crafting mc_value_crafting;
typedef bool (*mc_value_notify)(void *handler,mc_value_crafting *view);
struct mc_value_crafting {
    mc_slot *stackList;
    int width,height,size;
    void *eventHandler;
    mc_value_notify notify;
};
typedef struct { mc_slot *stackResult; } mc_value_result;
bool mc_value_crafting_attach(mc_value_crafting *,mc_slot *,void *,mc_value_notify,int,int);
void mc_value_crafting_free(mc_value_crafting *);
int mc_value_crafting_getSizeInventory(const mc_value_crafting *);
int mc_value_crafting_getWidth(const mc_value_crafting *);
int mc_value_crafting_getHeight(const mc_value_crafting *);
mc_slot *mc_value_crafting_getStackInSlot(mc_value_crafting *,int);
bool mc_value_crafting_decrStackSize(mc_value_crafting *,int,int,mc_slot *);
bool mc_value_crafting_removeStackFromSlot(mc_value_crafting *,int,mc_slot *);
bool mc_value_crafting_setInventorySlotContents(mc_value_crafting *,int,const mc_slot *);
void mc_value_result_attach(mc_value_result *,mc_slot *);
void mc_value_result_free(mc_value_result *);
void mc_value_result_clear(mc_value_result *);
bool mc_value_result_decrStackSize(mc_value_result *,int,int,mc_slot *);
bool mc_value_result_setInventorySlotContents(mc_value_result *,int,const mc_slot *);
#endif
