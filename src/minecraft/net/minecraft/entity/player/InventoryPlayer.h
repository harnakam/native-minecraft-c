#ifndef C919_INVENTORY_PLAYER_H
#define C919_INVENTORY_PLAYER_H
#include "inventory/InventoryCrafting.h"
typedef bool (*InventoryPlayerCreative)(const MCObject *player);
typedef struct InventoryPlayer {
    MCObject object;
    ItemStackArray *mainInventory,*armorInventory;
    int32_t currentItem;
    MCObject *player;
    ItemStack *itemStack;
    bool inventoryChanged;
    InventoryPlayerCreative isCreativeMode;
} InventoryPlayer;
InventoryPlayer *InventoryPlayer_new(MCObjectHeap *,MCObject *,InventoryPlayerCreative);
ItemStack *InventoryPlayer_getCurrentItem(const InventoryPlayer *);
int32_t InventoryPlayer_getHotbarSize(void);
int32_t InventoryPlayer_getFirstEmptyStack(const InventoryPlayer *);
void InventoryPlayer_changeCurrentItem(InventoryPlayer *,int32_t);
bool InventoryPlayer_addItemStackToInventory(InventoryPlayer *,ItemStack *);
ItemStack *InventoryPlayer_decrStackSize(InventoryPlayer *,int32_t,int32_t);
ItemStack *InventoryPlayer_removeStackFromSlot(InventoryPlayer *,int32_t);
bool InventoryPlayer_setInventorySlotContents(InventoryPlayer *,int32_t,ItemStack *);
ItemStack *InventoryPlayer_getStackInSlot(InventoryPlayer *,int32_t);
ItemStack *InventoryPlayer_armorItemInSlot(InventoryPlayer *,int32_t);
int32_t InventoryPlayer_getSizeInventory(const InventoryPlayer *);
int32_t InventoryPlayer_getInventoryStackLimit(const InventoryPlayer *);
const char *InventoryPlayer_getName(const InventoryPlayer *);
bool InventoryPlayer_hasCustomName(const InventoryPlayer *);
InventoryDisplayName InventoryPlayer_getDisplayName(const InventoryPlayer *);
void InventoryPlayer_markDirty(InventoryPlayer *);
bool InventoryPlayer_setItemStack(InventoryPlayer *,ItemStack *);
ItemStack *InventoryPlayer_getItemStack(const InventoryPlayer *);
bool InventoryPlayer_hasItemStack(const InventoryPlayer *,const ItemStack *);
bool InventoryPlayer_hasItem(const InventoryPlayer *,const Item *);
bool InventoryPlayer_consumeInventoryItem(InventoryPlayer *,const Item *);
NBTTagList *InventoryPlayer_writeToNBT(InventoryPlayer *,NBTTagList *);
ItemStackNBTResult InventoryPlayer_readFromNBT(InventoryPlayer *,NBTTagList *);
bool InventoryPlayer_copyInventory(InventoryPlayer *,const InventoryPlayer *);
void InventoryPlayer_clear(InventoryPlayer *);
void InventoryPlayer_openInventory(InventoryPlayer *,MCObject *);
void InventoryPlayer_closeInventory(InventoryPlayer *,MCObject *);
bool InventoryPlayer_isItemValidForSlot(const InventoryPlayer *,int32_t,const ItemStack *);
int32_t InventoryPlayer_getField(const InventoryPlayer *,int32_t);
void InventoryPlayer_setField(InventoryPlayer *,int32_t,int32_t);
int32_t InventoryPlayer_getFieldCount(const InventoryPlayer *);
/* Source subset needed by insertion, cursor, crafting and storage. Entity
   capability dispatch is native; combat/world/drop/stat/NBTUtil matching and
   setCurrentItem enchantment dependencies are unported, without stubs. */
#endif
