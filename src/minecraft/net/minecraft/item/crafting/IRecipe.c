#include "item/crafting/IRecipe.h"
#include <limits.h>
#include <string.h>
typedef struct { MCObject object; int32_t capacity; IRecipe items[]; } RecipeArray;
struct RecipeList { MCObject object; int32_t size; uint32_t modCount; RecipeArray *array; };
struct ItemStackList { MCObject object; int32_t size; ItemStackArray *array; };
static void trace_array(MCObject *o,MCObjectVisitor v,void *ctx) {
    RecipeArray *a=(RecipeArray *)o;
    for (int32_t i=0;i<a->capacity;i++) a->items[i].instance=v(a->items[i].instance,ctx);
}
static void trace_recipes(MCObject *o,MCObjectVisitor v,void *ctx) { RecipeList *l=(RecipeList *)o; l->array=(RecipeArray *)v((MCObject *)l->array,ctx); }
static void trace_items(MCObject *o,MCObjectVisitor v,void *ctx) { ItemStackList *l=(ItemStackList *)o; l->array=(ItemStackArray *)v((MCObject *)l->array,ctx); }
static const MCObjectClass array_class={"RecipeArray (native ArrayList storage)",MCObjectHeap_plainClone,trace_array,NULL};
static const MCObjectClass recipe_class={"RecipeList (native ArrayList storage)",MCObjectHeap_plainClone,trace_recipes,NULL};
static const MCObjectClass item_class={"ItemStackList (native ArrayList storage)",MCObjectHeap_plainClone,trace_items,NULL};
RecipeList *RecipeList_new(MCObjectHeap *h) { return (RecipeList *)MCObjectHeap_alloc(h,sizeof(RecipeList),&recipe_class); }
ItemStackList *ItemStackList_new(MCObjectHeap *h) { return (ItemStackList *)MCObjectHeap_alloc(h,sizeof(ItemStackList),&item_class); }
int32_t RecipeList_size(const RecipeList *l) { return l->size; }
uint32_t RecipeList_modCount(const RecipeList *l) { return l->modCount; }
int32_t ItemStackList_size(const ItemStackList *l) { return l->size; }
static bool valid(MCObject *o,int32_t size,int32_t index) { if (index<0 || index>=size) { MCObjectHeap_fail(o->heap); return false; } return true; }
IRecipe RecipeList_get(RecipeList *l,int32_t i) { IRecipe empty={0}; return valid((MCObject *)l,l->size,i)?l->array->items[i]:empty; }
ItemStack *ItemStackList_get(ItemStackList *l,int32_t i) { return valid((MCObject *)l,l->size,i)?l->array->items[i]:NULL; }
static int32_t next_capacity(MCObject *o,int32_t size) {
    if (size==INT32_MAX) { MCObjectHeap_fail(o->heap); return 0; }
    int32_t cap=size<8?8:size<=INT32_MAX/2?size*2:INT32_MAX; return cap;
}
bool RecipeList_add(RecipeList *l,IRecipe value) {
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,l->object.heap)) return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)l)&&MCObjectRootScope_pin(&scope,value.instance);
    if (ok && (!l->array || l->size==l->array->capacity)) {
        int32_t cap=next_capacity((MCObject *)l,l->size);
        if (!cap || (size_t)cap>(SIZE_MAX-sizeof(RecipeArray))/sizeof(IRecipe)) { MCObjectHeap_fail(l->object.heap); ok=false; }
        else {
            RecipeArray *a=(RecipeArray *)MCObjectHeap_alloc(l->object.heap,sizeof(*a)+(size_t)cap*sizeof(IRecipe),&array_class);
            if (!a) ok=false;
            else { a->capacity=cap; if (l->size) memcpy(a->items,l->array->items,(size_t)l->size*sizeof(IRecipe)); l->array=a; }
        }
    }
    if (ok) { l->array->items[l->size++]=value; l->modCount++; MCObjectHeap_touch(l->object.heap); }
    MCObjectRootScope_end(&scope); return ok;
}
bool ItemStackList_add(ItemStackList *l,ItemStack *value) {
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,l->object.heap)) return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)l)&&MCObjectRootScope_pin(&scope,(MCObject *)value);
    if (ok && (!l->array || l->size==l->array->length)) {
        int32_t cap=next_capacity((MCObject *)l,l->size); ItemStackArray *a=cap?ItemStackArray_new(l->object.heap,cap):NULL;
        if (!a) ok=false;
        else { if (l->size) memcpy(a->items,l->array->items,(size_t)l->size*sizeof(ItemStack *)); l->array=a; }
    }
    if (ok) { l->array->items[l->size++]=value; MCObjectHeap_touch(l->object.heap); }
    MCObjectRootScope_end(&scope); return ok;
}
bool RecipeList_set(RecipeList *l,int32_t i,IRecipe v) {
    if (!valid((MCObject *)l,l->size,i)) return false;
    if (v.instance && v.instance->heap!=l->object.heap) { MCObjectHeap_fail(l->object.heap); return false; }
    l->array->items[i]=v; MCObjectHeap_touch(l->object.heap); return true;
}
bool ItemStackList_set(ItemStackList *l,int32_t i,ItemStack *v) {
    if (!valid((MCObject *)l,l->size,i)) return false;
    if (v && v->object.heap!=l->object.heap) { MCObjectHeap_fail(l->object.heap); return false; }
    l->array->items[i]=v; MCObjectHeap_touch(l->object.heap); return true;
}
IRecipe RecipeList_remove(RecipeList *l,int32_t i) {
    IRecipe out=RecipeList_get(l,i); if (MCObjectHeap_failed(l->object.heap)) return out;
    memmove(&l->array->items[i],&l->array->items[i+1],(size_t)(l->size-i-1)*sizeof(IRecipe)); l->array->items[--l->size]=(IRecipe){0}; l->modCount++; MCObjectHeap_touch(l->object.heap); return out;
}
ItemStack *ItemStackList_remove(ItemStackList *l,int32_t i) {
    ItemStack *out=ItemStackList_get(l,i); if (MCObjectHeap_failed(l->object.heap)) return out;
    memmove(&l->array->items[i],&l->array->items[i+1],(size_t)(l->size-i-1)*sizeof(ItemStack *)); l->array->items[--l->size]=NULL; MCObjectHeap_touch(l->object.heap); return out;
}
void RecipeList_clear(RecipeList *l) { if (l->size) memset(l->array->items,0,(size_t)l->size*sizeof(IRecipe)); l->size=0; l->modCount++; MCObjectHeap_touch(l->object.heap); }
void ItemStackList_clear(ItemStackList *l) { if (l->size) memset(l->array->items,0,(size_t)l->size*sizeof(ItemStack *)); l->size=0; MCObjectHeap_touch(l->object.heap); }
