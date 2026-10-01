#include "item/ItemStack.h"
#include "item/item.h"
#include "nbt/NBTTagCompound.h"
#include <limits.h>
#include <string.h>

/* Immutable identity adapter, not an Item subclass implementation. */
struct Item { unsigned char canonical_identity; };
static const Item registry[2268]={{0}};
const Item *ItemStack_registryItem(int32_t id) {
    if (id<0 || id>=2268 || !mc_item_valid((int16_t)id)) return NULL;
    return &registry[id];
}
int32_t ItemStack_registryId(const Item *item) {
    if (!item) return 0;
    /* Source identity-map lookup returns -1 for an unregistered Item. Pointer
       subtraction is undefined for foreign identities; equality is sufficient
       for this immutable native registry adapter. */
    for (int32_t id=0;id<2268;id++)
        if (item==&registry[id]) return mc_item_valid((int16_t)id)?id:-1;
    return -1;
}
bool ItemStack_registryIsKnownItem(const Item *item) { return item && ItemStack_registryId(item)>=0; }
const char *ItemStack_registryResourceName(const Item *item) { return item?mc_item_resource_name((int16_t)ItemStack_registryId(item)):NULL; }
bool ItemStack_registryIsEditableBook(const Item *item) { return item && ItemStack_registryId(item)==387; }
const Item *ItemStack_registryContainerItem(const Item *item) {
    int32_t id=ItemStack_registryId(item); return id==326 || id==327 || id==335?ItemStack_registryItem(325):NULL;
}
static int32_t max_damage(int32_t id) {
    if (id>=268 && id<=271) return 59;
    if (id>=272 && id<=275) return 131;
    if (id>=276 && id<=279) return 1561;
    if (id>=283 && id<=286) return 32;
    if (id>=298 && id<=317) {
        static const int32_t armor[]={55,80,75,65,165,240,225,195,165,240,225,195,363,528,495,429,77,112,105,91};
        return armor[id-298];
    }
    switch (id) {
        case 256: case 257: case 258: case 267: case 292: return 250;
        case 259: case 346: return 64; case 261: return 384;
        case 290: return 59; case 291: return 131; case 293: return 1561;
        case 294: return 32; case 359: return 238; case 398: return 25;
        default: return 0;
    }
}
static int32_t subtract(int32_t a,int32_t b) { uint32_t bits=(uint32_t)a-(uint32_t)b; int32_t out; memcpy(&out,&bits,sizeof(out)); return out; }
static void stack_trace(MCObject *object,MCObjectVisitor visit,void *ctx) {
    ItemStack *s=(ItemStack *)object;
    s->stackTagCompound=(NBTTagCompound *)visit((MCObject *)s->stackTagCompound,ctx);
    s->itemFrame=(EntityItemFrame *)visit((MCObject *)s->itemFrame,ctx);
}
static void array_trace(MCObject *object,MCObjectVisitor visit,void *ctx) {
    ItemStackArray *a=(ItemStackArray *)object;
    for (int32_t i=0;i<a->length;i++) a->items[i]=(ItemStack *)visit((MCObject *)a->items[i],ctx);
}
static const MCObjectClass stack_class={"ItemStack",MCObjectHeap_plainClone,stack_trace,NULL};
static const MCObjectClass array_class={"ItemStack[]",MCObjectHeap_plainClone,array_trace,NULL};
bool ItemStack_isInstance(const MCObject *object) {return object&&object->klass==&stack_class;}
bool ItemStackArray_isInstance(const MCObject *object) {return object&&object->klass==&array_class;}
ItemStackArray *ItemStackArray_new(MCObjectHeap *heap,int32_t length) {
    if (length<0 || (size_t)length>(SIZE_MAX-sizeof(ItemStackArray))/sizeof(ItemStack *)) { MCObjectHeap_fail(heap); return NULL; }
    ItemStackArray *a=(ItemStackArray *)MCObjectHeap_alloc(heap,sizeof(*a)+(size_t)length*sizeof(*a->items),&array_class);
    if (a) a->length=length;
    return a;
}
ItemStack *ItemStack_new(MCObjectHeap *heap,const Item *item,int32_t count,int32_t meta) {
    ItemStack *s=(ItemStack *)MCObjectHeap_alloc(heap,sizeof(*s),&stack_class);
    if (s) { s->item=item; s->stackSize=count; s->itemDamage=meta<0?0:meta; }
    return s;
}
ItemStack *ItemStack_new_item(MCObjectHeap *heap,const Item *item) { return ItemStack_new(heap,item,1,0); }
ItemStack *ItemStack_new_item_amount(MCObjectHeap *heap,const Item *item,int32_t count) { return ItemStack_new(heap,item,count,0); }
ItemStack *ItemStack_splitStack(MCObjectHeap *heap,ItemStack *s,int32_t count) {
    if (!s || s->object.heap!=heap) { MCObjectHeap_fail(heap); return NULL; }
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,heap)) return NULL;
    if (!MCObjectRootScope_pin(&scope,(MCObject *)s)) { MCObjectRootScope_end(&scope); return NULL; }
    ItemStack *out=ItemStack_new(heap,s->item,count,s->itemDamage);
    if (out && s->stackTagCompound) {
        out->stackTagCompound=(NBTTagCompound *)NBTBase_copy(heap,(NBTBase *)s->stackTagCompound);
        if (!out->stackTagCompound) out=NULL;
    }
    if (out) { s->stackSize=subtract(s->stackSize,count); MCObjectHeap_touch(heap); }
    MCObjectRootScope_end(&scope); return out;
}
ItemStack *ItemStack_copy(MCObjectHeap *heap,const ItemStack *s) {
    if (!s || s->object.heap!=heap) { MCObjectHeap_fail(heap); return NULL; }
    ItemStack *out=ItemStack_new(heap,s->item,s->stackSize,s->itemDamage);
    if (out && s->stackTagCompound) {
        out->stackTagCompound=(NBTTagCompound *)NBTBase_copy(heap,(NBTBase *)s->stackTagCompound);
        if (!out->stackTagCompound) return NULL;
    }
    return out;
}
ItemStack *ItemStack_copyItemStack(MCObjectHeap *heap,const ItemStack *s) { return s?ItemStack_copy(heap,s):NULL; }
const Item *ItemStack_getItem(const ItemStack *s) { return s->item; }
void ItemStack_setItem(ItemStack *s,const Item *item) { s->item=item; MCObjectHeap_touch(s->object.heap); }
int32_t ItemStack_getItemDamage(const ItemStack *s) { return s->itemDamage; }
int32_t ItemStack_getMetadata(const ItemStack *s) { return s->itemDamage; }
void ItemStack_setItemDamage(ItemStack *s,int32_t meta) { s->itemDamage=meta<0?0:meta; MCObjectHeap_touch(s->object.heap); }
int32_t ItemStack_getMaxStackSize(const ItemStack *s) {
    if (!s->item) { MCObjectHeap_fail(s->object.heap); return 0; }
    return (int32_t)mc_item_stack_limit((int16_t)ItemStack_registryId(s->item));
}
int32_t ItemStack_getMaxDamage(const ItemStack *s) {
    if (!s->item) { MCObjectHeap_fail(s->object.heap); return 0; }
    return max_damage(ItemStack_registryId(s->item));
}
bool ItemStack_getHasSubtypes(const ItemStack *s) {
    if (!s->item) { MCObjectHeap_fail(s->object.heap); return false; }
    return mc_item_has_subtypes((int16_t)ItemStack_registryId(s->item));
}
bool ItemStack_isItemStackDamageable(const ItemStack *s) {
    return s->item && max_damage(ItemStack_registryId(s->item))>0 && (!s->stackTagCompound || !NBTTagCompound_getBoolean_ascii(s->stackTagCompound,"Unbreakable"));
}
bool ItemStack_isItemDamaged(const ItemStack *s) { return ItemStack_isItemStackDamageable(s)&&s->itemDamage>0; }
bool ItemStack_isStackable(const ItemStack *s) { return ItemStack_getMaxStackSize(s)>1 && (!ItemStack_isItemStackDamageable(s)||!ItemStack_isItemDamaged(s)); }
bool ItemStack_hasTagCompound(const ItemStack *s) { return s->stackTagCompound!=NULL; }
NBTTagCompound *ItemStack_getTagCompound(const ItemStack *s) { return s->stackTagCompound; }
bool ItemStack_setTagCompound(ItemStack *s,NBTTagCompound *tag) {
    if (tag && ((MCObject *)tag)->heap!=s->object.heap) { MCObjectHeap_fail(s->object.heap); return false; }
    s->stackTagCompound=tag; MCObjectHeap_touch(s->object.heap); return true;
}
bool ItemStack_areItemStackTagsEqual(const ItemStack *a,const ItemStack *b) {
    if (!a && !b) return true;
    if (!a || !b) return false;
    if (!a->stackTagCompound && b->stackTagCompound) return false;
    return !a->stackTagCompound || NBTBase_equals((NBTBase *)a->stackTagCompound,(NBTBase *)b->stackTagCompound);
}
bool ItemStack_getIsItemStackEqual(const ItemStack *a,const ItemStack *b) {
    if (!b) { MCObjectHeap_fail(a->object.heap); return false; }
    return a->stackSize==b->stackSize && a->item==b->item && a->itemDamage==b->itemDamage && ItemStack_areItemStackTagsEqual(a,b);
}
bool ItemStack_areItemStacksEqual(const ItemStack *a,const ItemStack *b) { return !a&&!b?true:(!a||!b?false:ItemStack_getIsItemStackEqual(a,b)); }
bool ItemStack_isItemEqual(const ItemStack *a,const ItemStack *b) { return b && a->item==b->item && a->itemDamage==b->itemDamage; }
bool ItemStack_areItemsEqual(const ItemStack *a,const ItemStack *b) { return !a&&!b?true:(!a||!b?false:ItemStack_isItemEqual(a,b)); }
NBTTagCompound *ItemStack_getSubCompound(ItemStack *s,const NBTString *key,bool create) {
    if (s->stackTagCompound && NBTTagCompound_hasKeyType(s->stackTagCompound,key,10)) return NBTTagCompound_getCompoundTag(s->stackTagCompound,key);
    if (!create) return NULL;
    NBTTagCompound *c=NBTTagCompound_new(s->object.heap);
    return c && ItemStack_setTagInfo(s,key,(NBTBase *)c)?c:NULL;
}
NBTTagList *ItemStack_getEnchantmentTagList(const ItemStack *s) { return s->stackTagCompound?NBTTagCompound_getTagList_ascii(s->stackTagCompound,"ench",10):NULL; }
bool ItemStack_setTagInfo(ItemStack *s,const NBTString *key,NBTBase *value) {
    if (!s->stackTagCompound) { s->stackTagCompound=NBTTagCompound_new(s->object.heap); MCObjectHeap_touch(s->object.heap); }
    return s->stackTagCompound && NBTTagCompound_setTag(s->stackTagCompound,(NBTString *)key,value);
}
bool ItemStack_setTagInfo_ascii(ItemStack *s,const char *key,NBTBase *value) {
    NBTString *k=NBTString_fromASCII(s->object.heap,key); return k && ItemStack_setTagInfo(s,k,value);
}
bool ItemStack_hasDisplayName(const ItemStack *s) {
    return s->stackTagCompound && NBTTagCompound_hasKeyType_ascii(s->stackTagCompound,"display",10) && NBTTagCompound_hasKeyType_ascii(NBTTagCompound_getCompoundTag_ascii(s->stackTagCompound,"display"),"Name",8);
}
NBTString *ItemStack_getDisplayName(ItemStack *s,ItemStackDisplayNameDispatch dispatch,MCObject *context) {
    if (!s->item || !dispatch || (context && context->heap!=s->object.heap)) { MCObjectHeap_fail(s->object.heap); return NULL; }
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,s->object.heap)) return NULL;
    if (!MCObjectRootScope_pin(&scope,(MCObject *)s) || !MCObjectRootScope_pin(&scope,context)) { MCObjectRootScope_end(&scope); return NULL; }
    NBTString *name=dispatch(context,s);
    if (!name) MCObjectHeap_fail(s->object.heap);
    else if (ItemStack_hasDisplayName(s)) name=NBTTagCompound_getString_ascii(NBTTagCompound_getCompoundTag_ascii(s->stackTagCompound,"display"),"Name");
    MCObjectRootScope_end(&scope); return name;
}
bool ItemStack_setStackDisplayName(ItemStack *s,const NBTString *name) {
    if (!s->stackTagCompound) { s->stackTagCompound=NBTTagCompound_new(s->object.heap); MCObjectHeap_touch(s->object.heap); }
    if (!s->stackTagCompound) return false;
    if (!NBTTagCompound_hasKeyType_ascii(s->stackTagCompound,"display",10)) {
        NBTTagCompound *d=NBTTagCompound_new(s->object.heap);
        if (!d || !NBTTagCompound_setTag_ascii(s->stackTagCompound,"display",(NBTBase *)d)) return false;
    }
    return NBTTagCompound_setString_ascii(NBTTagCompound_getCompoundTag_ascii(s->stackTagCompound,"display"),"Name",(NBTString *)name);
}
bool ItemStack_clearCustomName(ItemStack *s) {
    if (s->stackTagCompound && NBTTagCompound_hasKeyType_ascii(s->stackTagCompound,"display",10)) {
        NBTTagCompound *d=NBTTagCompound_getCompoundTag_ascii(s->stackTagCompound,"display");
        if (!NBTTagCompound_removeTag_ascii(d,"Name")) return false;
        if (NBTBase_hasNoTags((NBTBase *)d)) {
            if (!NBTTagCompound_removeTag_ascii(s->stackTagCompound,"display")) return false;
            if (NBTBase_hasNoTags((NBTBase *)s->stackTagCompound)) return ItemStack_setTagCompound(s,NULL);
        }
    }
    return !MCObjectHeap_failed(s->object.heap);
}
int32_t ItemStack_getRepairCost(const ItemStack *s) { return s->stackTagCompound&&NBTTagCompound_hasKeyType_ascii(s->stackTagCompound,"RepairCost",3)?NBTTagCompound_getInteger_ascii(s->stackTagCompound,"RepairCost"):0; }
bool ItemStack_setRepairCost(ItemStack *s,int32_t cost) {
    if (!s->stackTagCompound) { s->stackTagCompound=NBTTagCompound_new(s->object.heap); MCObjectHeap_touch(s->object.heap); }
    return s->stackTagCompound && NBTTagCompound_setInteger_ascii(s->stackTagCompound,"RepairCost",cost);
}
bool ItemStack_isItemEnchanted(const ItemStack *s) { return s->stackTagCompound && NBTTagCompound_hasKeyType_ascii(s->stackTagCompound,"ench",9); }
static int digit(uint16_t ch) {
    static const uint16_t starts[]={0x30,0x660,0x6f0,0x7c0,0x966,0x9e6,0xa66,0xae6,0xb66,0xbe6,0xc66,0xce6,0xd66,0xe50,0xed0,0xf20,0x1040,0x1090,0x17e0,0x1810,0x1946,0x19d0,0x1a80,0x1a90,0x1b50,0x1bb0,0x1c40,0x1c50,0xa620,0xa8d0,0xa900,0xa9d0,0xaa50,0xabf0,0xff10};
    for (size_t i=0;i<sizeof(starts)/sizeof(*starts);i++) if (ch>=starts[i] && ch<starts[i]+10) return ch-starts[i];
    return -1;
}
static const Item *resolve_name(const NBTString *name) {
    size_t n=NBTString_length(name),colon=n; const uint16_t *u=NBTString_units(name);
    for (size_t i=0;i<n;i++) if (u[i]==':') { colon=i; break; }
    bool domain=true;
    if (colon>1 && colon<n) {
        const char *expected="minecraft";
        domain=colon==strlen(expected);
        for (size_t i=0;domain&&i<colon;i++) { uint16_t ch=u[i]; if (ch>='A'&&ch<='Z') ch+=(uint16_t)('a'-'A'); domain=ch==(uint16_t)expected[i]; }
    }
    size_t start=colon<n?colon+1:0;
    if (domain) for (int id=1;id<2268;id++) {
        const Item *item=ItemStack_registryItem(id); const char *resource=ItemStack_registryResourceName(item);
        if (!resource) continue;
        const char *path=resource+10; size_t length=strlen(path); if (length!=n-start) continue;
        bool equal=true; for (size_t i=0;i<length;i++) if (u[start+i]!=(uint16_t)(unsigned char)path[i]) { equal=false; break; }
        if (equal) return item;
    }
    size_t pos=0; bool negative=false; if (n && (u[0]=='-'||u[0]=='+')) { negative=u[0]=='-'; pos=1; }
    if (pos==n) return NULL;
    uint32_t value=0,limit=negative?UINT32_C(2147483648):INT32_MAX;
    for (;pos<n;pos++) { int d=digit(u[pos]); if (d<0 || value>(limit-(uint32_t)d)/10) return NULL; value=value*10+(uint32_t)d; }
    return negative?NULL:ItemStack_registryItem((int32_t)value);
}
NBTTagCompound *ItemStack_writeToNBT(ItemStack *s,NBTTagCompound *out) {
    if (!out || ((MCObject *)out)->heap!=s->object.heap) { MCObjectHeap_fail(s->object.heap); return NULL; }
    const char *name=ItemStack_registryResourceName(s->item);
    uint8_t count_bits=(uint8_t)s->stackSize; int8_t count; memcpy(&count,&count_bits,sizeof(count));
    uint16_t damage_bits=(uint16_t)s->itemDamage; int16_t damage; memcpy(&damage,&damage_bits,sizeof(damage));
    if (!NBTTagCompound_setString_ascii(out,"id",NBTString_fromASCII(s->object.heap,name?name:"minecraft:air")) || !NBTTagCompound_setByte_ascii(out,"Count",count) || !NBTTagCompound_setShort_ascii(out,"Damage",damage)) return NULL;
    if (s->stackTagCompound && !NBTTagCompound_setTag_ascii(out,"tag",(NBTBase *)s->stackTagCompound)) return NULL;
    return out;
}
ItemStackNBTResult ItemStack_readFromNBT(ItemStack *s,NBTTagCompound *in) {
    if (!in || ((MCObject *)in)->heap!=s->object.heap) { MCObjectHeap_fail(s->object.heap); return ITEMSTACK_NBT_FAILURE; }
    s->item=NBTTagCompound_hasKeyType_ascii(in,"id",8)?resolve_name(NBTTagCompound_getString_ascii(in,"id")):ItemStack_registryItem(NBTTagCompound_getShort_ascii(in,"id"));
    s->stackSize=NBTTagCompound_getByte_ascii(in,"Count"); s->itemDamage=NBTTagCompound_getShort_ascii(in,"Damage");
    if (s->itemDamage<0) s->itemDamage=0;
    MCObjectHeap_touch(s->object.heap);
    if (NBTTagCompound_hasKeyType_ascii(in,"tag",10)) {
        s->stackTagCompound=NBTTagCompound_getCompoundTag_ascii(in,"tag");
        if (s->item && ItemStack_registryId(s->item)==397 && NBTTagCompound_hasKeyType_ascii(s->stackTagCompound,"SkullOwner",8) && NBTString_length(NBTTagCompound_getString_ascii(s->stackTagCompound,"SkullOwner"))>0) return ITEMSTACK_NBT_UNSUPPORTED_PROFILE;
    }
    return MCObjectHeap_failed(s->object.heap)?ITEMSTACK_NBT_FAILURE:ITEMSTACK_NBT_OK;
}
ItemStack *ItemStack_loadItemStackFromNBT(MCObjectHeap *heap,NBTTagCompound *in,ItemStackNBTResult *status) {
    ItemStack *s=ItemStack_new(heap,NULL,0,0); ItemStackNBTResult result=s?ItemStack_readFromNBT(s,in):ITEMSTACK_NBT_FAILURE;
    if (status) *status=result;
    return result==ITEMSTACK_NBT_OK && s->item?s:NULL;
}
