#ifndef C919_INVENTORY_BASIC_H
#define C919_INVENTORY_BASIC_H
#include "inventory/IInventory.h"
#include "item/ItemStack.h"
#include "nbt/NBTString.h"
typedef struct InventoryBasic InventoryBasic;
typedef struct InventoryBasicListeners InventoryBasicListeners;
typedef struct {
    bool (*onInventoryChanged)(MCObject *context,MCObject *listener,InventoryBasic *);
    bool (*listenerEquals)(MCObject *context,MCObject *receiver,MCObject *other,bool *);
    NBTString *(*getUnformattedText)(MCObject *context,MCObject *chat);
    MCObject *(*newChatComponentText)(MCObject *context,NBTString *);
    MCObject *(*newChatComponentTranslation)(MCObject *context,NBTString *);
} InventoryBasicDependencies;
struct InventoryBasic {
    MCObject object;
    NBTString *inventoryTitle;
    int32_t slotsCount;
    ItemStackArray *inventoryContents;
    InventoryBasicListeners *changeListeners;
    bool hasCustomName;
    const InventoryBasicDependencies *dependencies;
    MCObject *context;
};
InventoryBasic *InventoryBasic_new(MCObjectHeap *,NBTString *title,bool customName,int32_t slotCount,const InventoryBasicDependencies *,MCObject *);
InventoryBasic *InventoryBasic_new_chat(MCObjectHeap *,MCObject *title,int32_t slotCount,const InventoryBasicDependencies *,MCObject *);
bool InventoryBasic_isInstance(const MCObject *);
/* Subclass construction/trace adapters reuse the actual first-member owner. */
bool InventoryBasic_construct(InventoryBasic *,NBTString *,bool,int32_t,const InventoryBasicDependencies *,MCObject *);
void InventoryBasic_traceFields(InventoryBasic *,MCObjectVisitor,void *);
bool InventoryBasic_addInventoryChangeListener(InventoryBasic *,MCObject *);
bool InventoryBasic_removeInventoryChangeListener(InventoryBasic *,MCObject *);
ItemStack *InventoryBasic_getStackInSlot(InventoryBasic *,int32_t);
ItemStack *InventoryBasic_decrStackSize(InventoryBasic *,int32_t,int32_t);
ItemStack *InventoryBasic_func_174894_a(InventoryBasic *,ItemStack *);
ItemStack *InventoryBasic_removeStackFromSlot(InventoryBasic *,int32_t);
bool InventoryBasic_setInventorySlotContents(InventoryBasic *,int32_t,ItemStack *);
int32_t InventoryBasic_getSizeInventory(const InventoryBasic *);
NBTString *InventoryBasic_getName(const InventoryBasic *);
bool InventoryBasic_hasCustomName(const InventoryBasic *);
bool InventoryBasic_setCustomName(InventoryBasic *,NBTString *);
MCObject *InventoryBasic_getDisplayName(InventoryBasic *);
int32_t InventoryBasic_getInventoryStackLimit(const InventoryBasic *);
bool InventoryBasic_markDirty(InventoryBasic *);
bool InventoryBasic_isUseableByPlayer(const InventoryBasic *,const MCObject *);
void InventoryBasic_openInventory(InventoryBasic *,MCObject *);
void InventoryBasic_closeInventory(InventoryBasic *,MCObject *);
bool InventoryBasic_isItemValidForSlot(const InventoryBasic *,int32_t,const ItemStack *);
int32_t InventoryBasic_getField(const InventoryBasic *,int32_t);
void InventoryBasic_setField(InventoryBasic *,int32_t,int32_t);
int32_t InventoryBasic_getFieldCount(const InventoryBasic *);
void InventoryBasic_clear(InventoryBasic *);
IInventory InventoryBasic_asIInventory(InventoryBasic *);
/* Source refs and bodies. C dispatch currently represents the concrete Basic
   and EnderChest classes, not arbitrary Java overrides. Chat components,
   listener Object.equals and ArrayList allocation are native dependencies.
   markDirty uses the original indexed live-size loop, not an iterator. */
#endif
