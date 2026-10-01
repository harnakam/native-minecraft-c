#include "inventory/inventory_dispatch.h"
static ItemStack *craft_get(MCObject *object,int32_t index) { return InventoryCrafting_getStackInSlot((InventoryCrafting *)object,index); }
static bool craft_set(MCObject *object,int32_t index,ItemStack *stack) { return InventoryCrafting_setInventorySlotContents((InventoryCrafting *)object,index,stack); }
static ItemStack *craft_decr(MCObject *object,int32_t index,int32_t count) { return InventoryCrafting_decrStackSize((InventoryCrafting *)object,index,count); }
static void craft_dirty(MCObject *object) { InventoryCrafting_markDirty((InventoryCrafting *)object); }
static int32_t craft_limit(const MCObject *object) { return InventoryCrafting_getInventoryStackLimit((const InventoryCrafting *)object); }
static int32_t craft_size(const MCObject *object) { return InventoryCrafting_getSizeInventory((const InventoryCrafting *)object); }
static const IInventoryMethods craft_methods={craft_get,craft_set,craft_decr,craft_dirty,craft_limit,craft_size};
IInventory mc_IInventory_crafting(InventoryCrafting *inventory) { return (IInventory){(MCObject *)inventory,&craft_methods}; }
static ItemStack *result_get(MCObject *object,int32_t index) { return InventoryCraftResult_getStackInSlot((InventoryCraftResult *)object,index); }
static bool result_set(MCObject *object,int32_t index,ItemStack *stack) { return InventoryCraftResult_setInventorySlotContents((InventoryCraftResult *)object,index,stack); }
static ItemStack *result_decr(MCObject *object,int32_t index,int32_t count) { return InventoryCraftResult_decrStackSize((InventoryCraftResult *)object,index,count); }
static void result_dirty(MCObject *object) { InventoryCraftResult_markDirty((InventoryCraftResult *)object); }
static int32_t result_limit(const MCObject *object) { return InventoryCraftResult_getInventoryStackLimit((const InventoryCraftResult *)object); }
static int32_t result_size(const MCObject *object) { return InventoryCraftResult_getSizeInventory((const InventoryCraftResult *)object); }
static const IInventoryMethods result_methods={result_get,result_set,result_decr,result_dirty,result_limit,result_size};
IInventory mc_IInventory_result(InventoryCraftResult *inventory) { return (IInventory){(MCObject *)inventory,&result_methods}; }
static ItemStack *player_get(MCObject *object,int32_t index) { return InventoryPlayer_getStackInSlot((InventoryPlayer *)object,index); }
static bool player_set(MCObject *object,int32_t index,ItemStack *stack) { return InventoryPlayer_setInventorySlotContents((InventoryPlayer *)object,index,stack); }
static ItemStack *player_decr(MCObject *object,int32_t index,int32_t count) { return InventoryPlayer_decrStackSize((InventoryPlayer *)object,index,count); }
static void player_dirty(MCObject *object) { InventoryPlayer_markDirty((InventoryPlayer *)object); }
static int32_t player_limit(const MCObject *object) { return InventoryPlayer_getInventoryStackLimit((const InventoryPlayer *)object); }
static int32_t player_size(const MCObject *object) { return InventoryPlayer_getSizeInventory((const InventoryPlayer *)object); }
static const IInventoryMethods player_methods={player_get,player_set,player_decr,player_dirty,player_limit,player_size};
IInventory mc_IInventory_player(InventoryPlayer *inventory) { return (IInventory){(MCObject *)inventory,&player_methods}; }
