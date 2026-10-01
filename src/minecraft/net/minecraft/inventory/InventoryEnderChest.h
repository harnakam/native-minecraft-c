#ifndef C919_INVENTORY_ENDER_CHEST_H
#define C919_INVENTORY_ENDER_CHEST_H
#include "inventory/InventoryBasic.h"
typedef struct {
    bool (*canBeUsed)(MCObject *context,MCObject *chest,MCObject *player,bool *);
    bool (*openChest)(MCObject *context,MCObject *chest);
    bool (*closeChest)(MCObject *context,MCObject *chest);
} InventoryEnderChestDependencies;
typedef struct InventoryEnderChest {
    InventoryBasic basic;
    MCObject *associatedChest;
    const InventoryEnderChestDependencies *chestDependencies;
} InventoryEnderChest;
InventoryEnderChest *InventoryEnderChest_new(MCObjectHeap *,const InventoryBasicDependencies *,const InventoryEnderChestDependencies *,MCObject *context);
bool InventoryEnderChest_isInstance(const MCObject *);
bool InventoryEnderChest_setChestTileEntity(InventoryEnderChest *,MCObject *);
ItemStackNBTResult InventoryEnderChest_loadInventoryFromNBT(InventoryEnderChest *,NBTTagList *);
NBTTagList *InventoryEnderChest_saveInventoryToNBT(InventoryEnderChest *);
bool InventoryEnderChest_isUseableByPlayer(InventoryEnderChest *,MCObject *);
bool InventoryEnderChest_openInventory(InventoryEnderChest *,MCObject *);
bool InventoryEnderChest_closeInventory(InventoryEnderChest *,MCObject *);
IInventory InventoryEnderChest_asIInventory(InventoryEnderChest *);
/* Actual inherited inventory is basic, not a second contents owner. Required
   TileEntityEnderChest behavior is dispatched only on nonnull associatedChest. */
#endif
