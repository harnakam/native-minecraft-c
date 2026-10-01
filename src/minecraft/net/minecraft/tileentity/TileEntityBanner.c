#include "tileentity/TileEntityBanner.h"
#include "nbt/NBTTagCompound.h"
static NBTTagCompound *sub(ItemStack *s) {NBTString *k=NBTString_literalASCII(s->object.heap,"BlockEntityTag");return k?ItemStack_getSubCompound(s,k,false):NULL;}
int32_t TileEntityBanner_getBaseColor(ItemStack *s) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,s->object.heap))return 0;
    int32_t out=0;if(MCObjectRootScope_pin(&scope,(MCObject *)s)) {NBTTagCompound *c=sub(s);out=c&&NBTTagCompound_hasKey_ascii(c,"Base")?NBTTagCompound_getInteger_ascii(c,"Base"):ItemStack_getMetadata(s);}
    MCObjectRootScope_end(&scope);return out;
}
int32_t TileEntityBanner_getPatterns(ItemStack *s) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,s->object.heap))return 0;
    int32_t out=0;if(MCObjectRootScope_pin(&scope,(MCObject *)s)) {NBTTagCompound *c=sub(s);if(c&&NBTTagCompound_hasKey_ascii(c,"Patterns")){NBTTagList *l=NBTTagCompound_getTagList_ascii(c,"Patterns",10);if(l)out=NBTTagList_tagCount(l);}}
    MCObjectRootScope_end(&scope);return out;
}
