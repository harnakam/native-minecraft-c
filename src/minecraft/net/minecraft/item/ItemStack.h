#ifndef C919_ITEM_STACK_H
#define C919_ITEM_STACK_H
#include "util/MCObjectHeap.h"

typedef struct Item Item;
typedef struct Block Block;
typedef struct EntityItemFrame EntityItemFrame;
typedef struct NBTTagCompound NBTTagCompound;
typedef struct NBTTagList NBTTagList;
typedef struct NBTBase NBTBase;
typedef struct NBTString NBTString;

/* Item is an immutable canonical registry identity adapter. Item subclasses,
   World/Entity/Stats/Enchantments and localization remain separate ports. */
const Item *ItemStack_registryItem(int32_t legacy_id);
int32_t ItemStack_registryId(const Item *item);
/* Native immutable registry identity lookup: NULL retains source ID 0;
   unregistered identities return -1 and never become a base Item fallback. */
bool ItemStack_registryIsKnownItem(const Item *item);
const char *ItemStack_registryResourceName(const Item *item);
bool ItemStack_registryIsEditableBook(const Item *item);
const Item *ItemStack_registryContainerItem(const Item *item);

typedef struct ItemStack {
    MCObject object;
    int32_t stackSize, animationsToGo;
    const Item *item;
    NBTTagCompound *stackTagCompound;
    int32_t itemDamage;
    EntityItemFrame *itemFrame;
    const Block *canDestroyCacheBlock;
    bool canDestroyCacheResult;
    const Block *canPlaceOnCacheBlock;
    bool canPlaceOnCacheResult;
} ItemStack;

/* Native managed storage for the original ItemStack[] references. Its trace
   retains each element; it is not a second value copy of an inventory. */
typedef struct ItemStackArray {
    MCObject object;
    int32_t length;
    ItemStack *items[];
} ItemStackArray;
ItemStackArray *ItemStackArray_new(MCObjectHeap *heap,int32_t length);
bool ItemStackArray_isInstance(const MCObject *);
bool ItemStack_isInstance(const MCObject *);

ItemStack *ItemStack_new(MCObjectHeap *heap,const Item *item,int32_t amount,int32_t metadata);
ItemStack *ItemStack_new_item(MCObjectHeap *heap,const Item *item);
ItemStack *ItemStack_new_item_amount(MCObjectHeap *heap,const Item *item,int32_t amount);
ItemStack *ItemStack_splitStack(MCObjectHeap *heap,ItemStack *stack,int32_t amount);
/* Original copy executes in the object's current heap. Snapshot branching
   uses MCObjectHeap_clone; wire/save decode constructs fresh references. */
ItemStack *ItemStack_copy(MCObjectHeap *heap,const ItemStack *stack);
ItemStack *ItemStack_copyItemStack(MCObjectHeap *heap,const ItemStack *stack);
const Item *ItemStack_getItem(const ItemStack *stack);
void ItemStack_setItem(ItemStack *stack,const Item *item);
int32_t ItemStack_getItemDamage(const ItemStack *stack);
int32_t ItemStack_getMetadata(const ItemStack *stack);
void ItemStack_setItemDamage(ItemStack *stack,int32_t metadata);
int32_t ItemStack_getMaxStackSize(const ItemStack *stack);
int32_t ItemStack_getMaxDamage(const ItemStack *stack);
bool ItemStack_getHasSubtypes(const ItemStack *stack);
bool ItemStack_isStackable(const ItemStack *stack);
bool ItemStack_isItemStackDamageable(const ItemStack *stack);
bool ItemStack_isItemDamaged(const ItemStack *stack);
bool ItemStack_hasTagCompound(const ItemStack *stack);
NBTTagCompound *ItemStack_getTagCompound(const ItemStack *stack);
bool ItemStack_setTagCompound(ItemStack *stack,NBTTagCompound *tag);
bool ItemStack_areItemStackTagsEqual(const ItemStack *a,const ItemStack *b);
bool ItemStack_areItemStacksEqual(const ItemStack *a,const ItemStack *b);
bool ItemStack_areItemsEqual(const ItemStack *a,const ItemStack *b);
bool ItemStack_isItemEqual(const ItemStack *stack,const ItemStack *other);
bool ItemStack_getIsItemStackEqual(const ItemStack *stack,const ItemStack *other);
NBTTagCompound *ItemStack_getSubCompound(ItemStack *stack,const NBTString *key,bool create);
NBTTagList *ItemStack_getEnchantmentTagList(const ItemStack *stack);
bool ItemStack_setTagInfo(ItemStack *stack,const NBTString *key,NBTBase *value);
bool ItemStack_setTagInfo_ascii(ItemStack *stack,const char *key,NBTBase *value);
bool ItemStack_hasDisplayName(const ItemStack *stack);
/* Actual Item subclass/localization is supplied by the explicit dependency.
   Source getDisplayName invokes it before overriding with custom Name. */
typedef NBTString *(*ItemStackDisplayNameDispatch)(MCObject *context,const ItemStack *stack);
NBTString *ItemStack_getDisplayName(ItemStack *stack,ItemStackDisplayNameDispatch,MCObject *context);
bool ItemStack_setStackDisplayName(ItemStack *stack,const NBTString *name);
bool ItemStack_clearCustomName(ItemStack *stack);
int32_t ItemStack_getRepairCost(const ItemStack *stack);
bool ItemStack_setRepairCost(ItemStack *stack,int32_t cost);
bool ItemStack_isItemEnchanted(const ItemStack *stack);

typedef enum {
    ITEMSTACK_NBT_OK,
    ITEMSTACK_NBT_FAILURE,
    ITEMSTACK_NBT_UNSUPPORTED_PROFILE
} ItemStackNBTResult;
NBTTagCompound *ItemStack_writeToNBT(ItemStack *stack,NBTTagCompound *output);
ItemStackNBTResult ItemStack_readFromNBT(ItemStack *stack,NBTTagCompound *input);
ItemStack *ItemStack_loadItemStackFromNBT(MCObjectHeap *heap,NBTTagCompound *input,ItemStackNBTResult *status);

/* Animation, use, crafting and frame method subsets are declared by
   ItemStackAnimation.h, ItemStackUse.h, ItemStackCrafting.h and ItemStackFrame.h, with their
   actual virtual Item dependencies explicit.
   Original body scope here: constructors/state/split/copy/equality, storage NBT,
   damage/stack properties and tag mutation. Combat,
   tooltip/chat/attribute/rarity/enchantment behavior and Block-cache
   queries require their actual class dependencies and are not stubbed here. */
#endif
