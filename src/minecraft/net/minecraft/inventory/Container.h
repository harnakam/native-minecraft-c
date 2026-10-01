#ifndef C919_SOURCE_CONTAINER_H
#define C919_SOURCE_CONTAINER_H
#include "inventory/Slot.h"
#include "entity/player/InventoryPlayer.h"

typedef struct Container Container;
typedef struct ContainerList ContainerList;
typedef struct ContainerIdentitySet ContainerIdentitySet;
typedef struct {
    bool (*updateCraftingInventory)(MCObject *target,Container *container,ContainerList *stacks);
    bool (*sendSlotContents)(MCObject *target,Container *container,int32_t index,ItemStack *stack);
} ICraftingMethods;
typedef struct { MCObject *target; const ICraftingMethods *methods; } ICrafting;
typedef bool (*ContainerDrop)(MCObject *player,ItemStack *stack,bool scatter);
typedef struct {
    ItemStack *(*transferStackInSlot)(Container *,InventoryPlayer *,int32_t);
    bool (*canMergeSlot)(Container *,const ItemStack *,const Slot *);
    bool (*canDragIntoSlot)(Container *,const Slot *);
    bool (*canInteractWith)(Container *,InventoryPlayer *);
    bool (*onCraftMatrixChanged)(Container *,IInventory);
    bool (*onContainerClosed)(Container *,InventoryPlayer *);
    void (*retrySlotClick)(Container *,int32_t,int32_t,bool,InventoryPlayer *);
    bool (*enchantItem)(Container *,InventoryPlayer *,int32_t);
    void (*updateProgressBar)(Container *,int32_t,int32_t);
} ContainerOverrides;
struct Container {
    MCObject object;
    ContainerList *inventoryItemStacks,*inventorySlots;
    int32_t windowId;
    int16_t transactionID;
    int32_t dragMode,dragEvent;
    ContainerIdentitySet *dragSlots;
    ContainerList *crafters;
    ContainerIdentitySet *playerList;
    const ContainerOverrides *overrides;
    ContainerDrop drop;
    unsigned nativeClickDepth;
};
/* Container is abstract. Concrete classes allocate a first-member Container,
   then invoke construct and trace; immutable C dispatch and drop are adapters. */
bool Container_construct(Container *,const ContainerOverrides *,ContainerDrop);
void Container_trace(Container *,MCObjectVisitor,void *);
int32_t ContainerList_size(const ContainerList *);
MCObject *ContainerList_get(ContainerList *,int32_t);
int32_t ContainerIdentitySet_size(const ContainerIdentitySet *);
MCObject *ContainerIdentitySet_getAt(ContainerIdentitySet *,int32_t);
Slot *Container_addSlotToContainer(Container *,Slot *);
bool Container_onCraftGuiOpened(Container *,ICrafting);
bool Container_removeCraftingFromCrafters(Container *,ICrafting);
ContainerList *Container_getInventory(Container *);
bool Container_detectAndSendChanges(Container *);
bool Container_enchantItem(Container *,InventoryPlayer *,int32_t);
Slot *Container_getSlotFromInventory(Container *,IInventory,int32_t);
Slot *Container_getSlot(Container *,int32_t);
ItemStack *Container_transferStackInSlot(Container *,InventoryPlayer *,int32_t);
ItemStack *Container_slotClick(Container *,int32_t,int32_t,int32_t,InventoryPlayer *);
bool Container_canMergeSlot(Container *,const ItemStack *,const Slot *);
void Container_retrySlotClick(Container *,int32_t,int32_t,bool,InventoryPlayer *);
bool Container_onContainerClosed(Container *,InventoryPlayer *);
bool Container_onContainerClosedBase(Container *,InventoryPlayer *);
bool Container_onCraftMatrixChanged(Container *,IInventory);
bool Container_putStackInSlot(Container *,int32_t,ItemStack *);
bool Container_putStacksInSlots(Container *,const ItemStackArray *);
void Container_updateProgressBar(Container *,int32_t,int32_t);
int16_t Container_getNextTransactionID(Container *,InventoryPlayer *);
bool Container_getCanCraft(Container *,const MCObject *);
bool Container_setCanCraft(Container *,MCObject *,bool);
bool Container_canInteractWith(Container *,InventoryPlayer *);
bool Container_mergeItemStack(Container *,ItemStack *,int32_t,int32_t,bool);
int32_t Container_extractDragMode(int32_t);
int32_t Container_getDragEvent(int32_t);
int32_t Container_func_94534_d(int32_t,int32_t);
bool Container_isValidDragMode(int32_t,const InventoryPlayer *);
void Container_resetDrag(Container *);
bool Container_canAddItemToSlot(Slot *,const ItemStack *,bool);
void Container_computeStackSize(const ContainerIdentitySet *,int32_t,ItemStack *,int32_t);
bool Container_canDragIntoSlot(Container *,const Slot *);
int32_t Container_calcRedstoneFromInventory(IInventory);
/* Java collection storage and stable identity hashing are native dependencies.
   Lists retain nullable references; sets compare identity and iterate buckets.
   Every required listener/drop/abstract callback failure marks the heap.
   TileEntity calcRedstone overload waits for its actual class dependency. */
#endif
