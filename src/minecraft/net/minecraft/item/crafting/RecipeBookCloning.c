#include "item/crafting/RecipeBookCloning.h"
#include "nbt/NBTTagCompound.h"
#include <string.h>
static const MCObjectClass klass={"RecipeBookCloning",MCObjectHeap_plainClone,NULL,NULL};
RecipeBookCloning *RecipeBookCloning_new(MCObjectHeap *heap) { return (RecipeBookCloning *)MCObjectHeap_alloc(heap,sizeof(RecipeBookCloning),&klass); }
static ItemStack *scan(InventoryCrafting *grid,int32_t *blank) {
    ItemStack *written=NULL; *blank=0;
    for (int32_t i=0;i<InventoryCrafting_getSizeInventory(grid);i++) {
        ItemStack *s=InventoryCrafting_getStackInSlot(grid,i);
        if (!s) continue;
        if (s->item==ItemStack_registryItem(387)) { if (written) return NULL; written=s; }
        else { if (s->item!=ItemStack_registryItem(386)) return NULL; ++*blank; }
    }
    return written;
}
bool RecipeBookCloning_matches(InventoryCrafting *grid,const MCObject *world) { (void)world; int32_t blank; ItemStack *written=scan(grid,&blank); return written && blank>0; }
ItemStack *RecipeBookCloning_getCraftingResult(InventoryCrafting *grid,ItemStackDisplayNameDispatch display,MCObject *context) {
    MCObjectHeap *heap=grid->object.heap; MCObjectRootScope scope={0};
    if (!MCObjectRootScope_begin(&scope,heap)) return NULL;
    if (!MCObjectRootScope_pin(&scope,(MCObject *)grid) || !MCObjectRootScope_pin(&scope,context)) { MCObjectRootScope_end(&scope); return NULL; }
    int32_t blank; ItemStack *written=scan(grid,&blank),*out=NULL;
    if (written && blank>=1) {
        /* ItemEditableBook.getGeneration dereferences tag, even when absent. */
        if (!written->stackTagCompound) MCObjectHeap_fail(heap);
        else if (NBTTagCompound_getInteger_ascii(written->stackTagCompound,"generation")<2) {
            out=ItemStack_new_item_amount(heap,ItemStack_registryItem(387),blank);
            if (out && MCObjectRootScope_pin(&scope,(MCObject *)out)) {
                NBTTagCompound *tag=(NBTTagCompound *)NBTBase_copy(heap,(NBTBase *)written->stackTagCompound);
                int32_t generation=NBTTagCompound_getInteger_ascii(written->stackTagCompound,"generation");
                uint32_t bits=(uint32_t)generation+1; int32_t next; memcpy(&next,&bits,sizeof(next));
                if (!tag || !ItemStack_setTagCompound(out,tag) || !NBTTagCompound_setInteger_ascii(out->stackTagCompound,"generation",next)) out=NULL;
                else if (ItemStack_hasDisplayName(written)) { NBTString *name=ItemStack_getDisplayName(written,display,context); if (!name || !ItemStack_setStackDisplayName(out,name)) out=NULL; }
            } else out=NULL;
        }
    }
    MCObjectRootScope_end(&scope); return MCObjectHeap_failed(heap)?NULL:out;
}
int32_t RecipeBookCloning_getRecipeSize(void) { return 9; }
ItemStack *RecipeBookCloning_getRecipeOutput(void) { return NULL; }
ItemStackArray *RecipeBookCloning_getRemainingItems(InventoryCrafting *grid) {
    ItemStackArray *out=ItemStackArray_new(grid->object.heap,InventoryCrafting_getSizeInventory(grid));
    if (!out) return NULL;
    for (int32_t i=0;i<out->length;i++) {
        ItemStack *s=InventoryCrafting_getStackInSlot(grid,i);
        if (s && ItemStack_registryIsEditableBook(s->item)) { out->items[i]=s; MCObjectHeap_touch(grid->object.heap); break; }
    }
    return out;
}
static bool m(MCObject *o,InventoryCrafting *g,MCObject *w) { (void)o; return RecipeBookCloning_matches(g,w); }
static ItemStack *c(MCObject *o,InventoryCrafting *g,ItemStackDisplayNameDispatch d,MCObject *ctx) { (void)o; return RecipeBookCloning_getCraftingResult(g,d,ctx); }
static int32_t s(const MCObject *o) { (void)o; return RecipeBookCloning_getRecipeSize(); }
static ItemStack *p(MCObject *o) { (void)o; return RecipeBookCloning_getRecipeOutput(); }
static ItemStackArray *l(MCObject *o,InventoryCrafting *g) { (void)o; return RecipeBookCloning_getRemainingItems(g); }
static const IRecipeMethods methods={m,c,s,p,l};
IRecipe RecipeBookCloning_asRecipe(RecipeBookCloning *r) { return (IRecipe){(MCObject *)r,&methods,IRECIPE_OTHER}; }
