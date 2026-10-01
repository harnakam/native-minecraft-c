#include "item/ItemArmor.h"
#include "nbt/NBTTagCompound.h"

ItemArmorMaterial ItemArmor_getArmorMaterial(const Item *item) {
    int32_t id=ItemStack_registryId(item);
    if (id>=298 && id<=301) return ITEMARMOR_LEATHER;
    if (id>=302 && id<=305) return ITEMARMOR_CHAIN;
    if (id>=306 && id<=309) return ITEMARMOR_IRON;
    if (id>=310 && id<=313) return ITEMARMOR_DIAMOND;
    if (id>=314 && id<=317) return ITEMARMOR_GOLD;
    return ITEMARMOR_NONE;
}
bool ItemArmor_isInstance(const Item *item) { return ItemArmor_getArmorMaterial(item)!=ITEMARMOR_NONE; }
static bool begin(MCObjectRootScope *scope,MCObjectHeap *heap,ItemStack *stack) {
    if (!stack) { MCObjectHeap_fail(heap); return false; }
    if (!MCObjectRootScope_begin(scope,heap)) return false;
    if (!MCObjectRootScope_pin(scope,(MCObject *)stack)) { MCObjectRootScope_end(scope); return false; }
    return true;
}
bool ItemArmor_hasColor(MCObjectHeap *heap,const Item *item,ItemStack *stack) {
    if (ItemArmor_getArmorMaterial(item)!=ITEMARMOR_LEATHER) return false;
    MCObjectRootScope scope={0}; if (!begin(&scope,heap,stack)) return false;
    NBTTagCompound *tag=ItemStack_getTagCompound(stack);
    bool out=tag && NBTTagCompound_hasKeyType_ascii(tag,"display",10)
        && NBTTagCompound_hasKeyType_ascii(NBTTagCompound_getCompoundTag_ascii(tag,"display"),"color",3);
    MCObjectRootScope_end(&scope); return out && !MCObjectHeap_failed(heap);
}
int32_t ItemArmor_getColor(MCObjectHeap *heap,const Item *item,ItemStack *stack) {
    if (ItemArmor_getArmorMaterial(item)!=ITEMARMOR_LEATHER) return -1;
    MCObjectRootScope scope={0}; if (!begin(&scope,heap,stack)) return 0;
    int32_t out=10511680; NBTTagCompound *tag=ItemStack_getTagCompound(stack);
    if (tag) {
        NBTTagCompound *display=NBTTagCompound_getCompoundTag_ascii(tag,"display");
        if (display && NBTTagCompound_hasKeyType_ascii(display,"color",3)) out=NBTTagCompound_getInteger_ascii(display,"color");
    }
    MCObjectRootScope_end(&scope); return out;
}
bool ItemArmor_removeColor(MCObjectHeap *heap,const Item *item,ItemStack *stack) {
    if (ItemArmor_getArmorMaterial(item)!=ITEMARMOR_LEATHER) return !MCObjectHeap_failed(heap);
    MCObjectRootScope scope={0}; if (!begin(&scope,heap,stack)) return false;
    NBTTagCompound *tag=ItemStack_getTagCompound(stack);
    if (tag) {
        NBTTagCompound *display=NBTTagCompound_getCompoundTag_ascii(tag,"display");
        if (display && NBTTagCompound_hasKey_ascii(display,"color")) NBTTagCompound_removeTag_ascii(display,"color");
    }
    MCObjectRootScope_end(&scope); return !MCObjectHeap_failed(heap);
}
bool ItemArmor_setColor(MCObjectHeap *heap,const Item *item,ItemStack *stack,int32_t color) {
    if (ItemArmor_getArmorMaterial(item)!=ITEMARMOR_LEATHER) { MCObjectHeap_fail(heap); return false; }
    MCObjectRootScope scope={0}; if (!begin(&scope,heap,stack)) return false;
    NBTTagCompound *tag=ItemStack_getTagCompound(stack);
    if (!tag) { tag=NBTTagCompound_new(heap); if (tag) ItemStack_setTagCompound(stack,tag); }
    NBTTagCompound *display=tag?NBTTagCompound_getCompoundTag_ascii(tag,"display"):NULL;
    if (display) {
        if (!NBTTagCompound_hasKeyType_ascii(tag,"display",10)) NBTTagCompound_setTag_ascii(tag,"display",(NBTBase *)display);
        NBTTagCompound_setInteger_ascii(display,"color",color);
    }
    MCObjectRootScope_end(&scope); return !MCObjectHeap_failed(heap);
}
int32_t ItemArmor_getColorFromItemStack(MCObjectHeap *heap,const Item *item,ItemStack *stack,int32_t renderPass) {
    if (renderPass>0) return 16777215;
    int32_t color=ItemArmor_getColor(heap,item,stack);
    return color<0?16777215:color;
}
