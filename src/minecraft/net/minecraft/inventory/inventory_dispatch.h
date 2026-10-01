#ifndef C919_INVENTORY_DISPATCH_H
#define C919_INVENTORY_DISPATCH_H
#include "inventory/IInventory.h"
#include "inventory/InventoryCrafting.h"
#include "inventory/InventoryCraftResult.h"
#include "entity/player/InventoryPlayer.h"
IInventory mc_IInventory_crafting(InventoryCrafting *inventory);
IInventory mc_IInventory_result(InventoryCraftResult *inventory);
IInventory mc_IInventory_player(InventoryPlayer *inventory);
#endif
