#include "inventory/InventoryBasic.h"
#include <limits.h>
#include <string.h>
typedef struct {MCObject object;int32_t capacity;MCObject *items[];} ListenerArray;
struct InventoryBasicListeners {MCObject object;int32_t size;ListenerArray *elements;};
static void array_trace(MCObject *o,MCObjectVisitor v,void *ctx){ListenerArray *a=(ListenerArray *)o;for(int32_t i=0;i<a->capacity;i++)a->items[i]=v(a->items[i],ctx);}
static const MCObjectClass listener_array_class={"native.InventoryBasic.listenerArray",MCObjectHeap_plainClone,array_trace,NULL};
static void list_trace(MCObject *o,MCObjectVisitor v,void *ctx){InventoryBasicListeners *l=(InventoryBasicListeners *)o;l->elements=(ListenerArray *)v((MCObject *)l->elements,ctx);}
static const MCObjectClass listener_list_class={"native.InventoryBasic.ArrayList",MCObjectHeap_plainClone,list_trace,NULL};
extern const MCObjectClass c919_inventoryenderchest_class;
void InventoryBasic_traceFields(InventoryBasic *b,MCObjectVisitor v,void *ctx){b->inventoryTitle=(NBTString *)v((MCObject *)b->inventoryTitle,ctx);b->inventoryContents=(ItemStackArray *)v((MCObject *)b->inventoryContents,ctx);b->changeListeners=(InventoryBasicListeners *)v((MCObject *)b->changeListeners,ctx);b->context=v(b->context,ctx);}
static void trace(MCObject *o,MCObjectVisitor v,void *ctx){InventoryBasic_traceFields((InventoryBasic *)o,v,ctx);}
const MCObjectClass c919_inventorybasic_class={"InventoryBasic",MCObjectHeap_plainClone,trace,NULL};
bool InventoryBasic_isInstance(const MCObject *o){return o&&(o->klass==&c919_inventorybasic_class||o->klass==&c919_inventoryenderchest_class)&&MCObjectHeap_objectSize(o)>=sizeof(InventoryBasic);}
static bool valid(const InventoryBasic *b){if(!InventoryBasic_isInstance((const MCObject *)b)){MCObjectHeap_fail(b?b->object.heap:NULL);return false;}return !MCObjectHeap_failed(b->object.heap);}
static bool effect(InventoryBasic *b,bool ok){if(!ok)MCObjectHeap_fail(b->object.heap);return ok&&!MCObjectHeap_failed(b->object.heap);}
static bool begin(InventoryBasic *b,MCObjectRootScope *s){if(!valid(b)||!MCObjectRootScope_begin(s,b->object.heap))return false;if(MCObjectRootScope_pin(s,(MCObject *)b))return true;MCObjectRootScope_end(s);return false;}
static bool ref(InventoryBasic *b,MCObject *o){return effect(b,!o||o->heap==b->object.heap);}
static bool contents(InventoryBasic *b){
    ItemStackArray *a=b->inventoryContents;
    return effect(b,ItemStackArray_isInstance((MCObject *)a)&&a->object.heap==b->object.heap&&a->length>=0&&
        (size_t)a->length<=(MCObjectHeap_objectSize((MCObject *)a)-sizeof *a)/sizeof *a->items);
}
static bool index(InventoryBasic *b,int32_t i){return contents(b)&&effect(b,i>=0&&i<b->inventoryContents->length);}
static bool stack(InventoryBasic *b,ItemStack *s){return effect(b,!s||(ItemStack_isInstance((MCObject *)s)&&s->object.heap==b->object.heap));}
static bool listeners(InventoryBasic *b){
    InventoryBasicListeners *l=b->changeListeners;
    if(!l||l->object.klass!=&listener_list_class||l->object.heap!=b->object.heap||MCObjectHeap_objectSize((MCObject *)l)<sizeof *l)return effect(b,false);
    ListenerArray *a=l->elements;
    return effect(b,a&&a->object.klass==&listener_array_class&&a->object.heap==b->object.heap&&MCObjectHeap_objectSize((MCObject *)a)>=sizeof *a&&a->capacity>=0&&
        (size_t)a->capacity<=(MCObjectHeap_objectSize((MCObject *)a)-sizeof *a)/sizeof *a->items&&l->size>=0&&l->size<=a->capacity);
}
static ListenerArray *new_array(MCObjectHeap *heap,int32_t n){
    if(n<0||(size_t)n>(SIZE_MAX-sizeof(ListenerArray))/sizeof(MCObject *)){MCObjectHeap_fail(heap);return NULL;}
    ListenerArray *a=(ListenerArray *)MCObjectHeap_alloc(heap,sizeof *a+(size_t)n*sizeof *a->items,&listener_array_class);if(a)a->capacity=n;return a;
}
bool InventoryBasic_construct(InventoryBasic *b,NBTString *title,bool custom,int32_t count,const InventoryBasicDependencies *deps,MCObject *ctx){
    MCObjectRootScope scope={0};if(!begin(b,&scope))return false;bool ok=false;
    if((title&&(!NBTString_isInstance((MCObject *)title)||((MCObject *)title)->heap!=b->object.heap))||!ref(b,ctx))goto done;
    if(!MCObjectRootScope_pin(&scope,(MCObject *)title)||!MCObjectRootScope_pin(&scope,ctx))goto done;
    b->dependencies=deps;b->context=ctx;b->inventoryTitle=title;b->hasCustomName=custom;b->slotsCount=count;MCObjectHeap_touch(b->object.heap);
    b->inventoryContents=ItemStackArray_new(b->object.heap,count);ok=b->inventoryContents!=NULL;
done:if(!ok)MCObjectHeap_fail(b->object.heap);MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(b->object.heap);
}
InventoryBasic *InventoryBasic_new(MCObjectHeap *heap,NBTString *title,bool custom,int32_t count,const InventoryBasicDependencies *deps,MCObject *ctx) {
    InventoryBasic *b=(InventoryBasic *)MCObjectHeap_alloc(heap,sizeof *b,&c919_inventorybasic_class);return b&&InventoryBasic_construct(b,title,custom,count,deps,ctx)?b:NULL;
}
InventoryBasic *InventoryBasic_new_chat(MCObjectHeap *heap,MCObject *title,int32_t count,const InventoryBasicDependencies *d,MCObject *ctx){
    InventoryBasic *b=(InventoryBasic *)MCObjectHeap_alloc(heap,sizeof *b,&c919_inventorybasic_class);if(!b)return NULL;
    MCObjectRootScope scope={0};if(!begin(b,&scope))return NULL;bool ok=false;
    if(!title||!ref(b,title)||!ref(b,ctx)||!d||!d->getUnformattedText||!MCObjectRootScope_pin(&scope,title)||!MCObjectRootScope_pin(&scope,ctx))goto done;
    NBTString *name=d->getUnformattedText(ctx,title);if(MCObjectHeap_failed(heap))goto done;
    ok=InventoryBasic_construct(b,name,true,count,d,ctx);
done:if(!ok)MCObjectHeap_fail(heap);MCObjectRootScope_end(&scope);return ok?b:NULL;
}
bool InventoryBasic_addInventoryChangeListener(InventoryBasic *b,MCObject *listener){
    MCObjectRootScope scope={0};if(!begin(b,&scope))return false;bool ok=false;
    if(!ref(b,listener)||!MCObjectRootScope_pin(&scope,listener))goto done;
    if(!b->changeListeners){
        InventoryBasicListeners *l=(InventoryBasicListeners *)MCObjectHeap_alloc(b->object.heap,sizeof *l,&listener_list_class);if(!l)goto done;
        l->elements=new_array(b->object.heap,0);if(!l->elements)goto done;b->changeListeners=l;MCObjectHeap_touch(b->object.heap);
    }
    if(!listeners(b))goto done;
    InventoryBasicListeners *l=b->changeListeners;
    if(l->size==l->elements->capacity){
        int64_t capacity=l->elements->capacity?(int64_t)l->elements->capacity+l->elements->capacity/2:10;
        if(capacity<=l->size)capacity=(int64_t)l->size+1;
        if(capacity>INT32_MAX)goto done;
        ListenerArray *a=new_array(b->object.heap,(int32_t)capacity);if(!a)goto done;
        memcpy(a->items,l->elements->items,(size_t)l->size*sizeof *a->items);l->elements=a;
    }
    l->elements->items[l->size++]=listener;MCObjectHeap_touch(b->object.heap);ok=true;
done:if(!ok)MCObjectHeap_fail(b->object.heap);MCObjectRootScope_end(&scope);return ok;
}
bool InventoryBasic_removeInventoryChangeListener(InventoryBasic *b,MCObject *listener){
    MCObjectRootScope scope={0};if(!begin(b,&scope))return false;bool ok=false;
    if(!ref(b,listener)||!MCObjectRootScope_pin(&scope,listener)||!listeners(b))goto done;
    for(int32_t i=0;;i++){
        if(!listeners(b))goto done;
        if(i>=b->changeListeners->size)break;
        MCObject *candidate=b->changeListeners->elements->items[i];bool same=candidate==NULL;
        if(listener){const InventoryBasicDependencies *d=b->dependencies;if(!ref(b,b->context)||!d||!d->listenerEquals||!effect(b,d->listenerEquals(b->context,listener,candidate,&same)))goto done;}
        if(same){
            if(!listeners(b)||!effect(b,i<b->changeListeners->size))goto done;
            InventoryBasicListeners *l=b->changeListeners;
            memmove(l->elements->items+i,l->elements->items+i+1,(size_t)(l->size-i-1)*sizeof *l->elements->items);
            l->elements->items[--l->size]=NULL;MCObjectHeap_touch(b->object.heap);break;
        }
    }
    ok=true;
done:if(!ok)MCObjectHeap_fail(b->object.heap);MCObjectRootScope_end(&scope);return ok;
}
ItemStack *InventoryBasic_getStackInSlot(InventoryBasic *b,int32_t i){if(!valid(b)||i<0)return NULL;if(!contents(b)||i>=b->inventoryContents->length)return NULL;ItemStack *s=b->inventoryContents->items[i];return stack(b,s)?s:NULL;}
bool InventoryBasic_markDirty(InventoryBasic *b){
    MCObjectRootScope scope={0};if(!begin(b,&scope))return false;bool ok=true;
    if(b->changeListeners)for(int32_t i=0;;i++){
        if(!listeners(b)){ok=false;break;}if(i>=b->changeListeners->size)break;
        MCObject *l=b->changeListeners->elements->items[i];const InventoryBasicDependencies *d=b->dependencies;
        if(!l||!ref(b,l)||!ref(b,b->context)||!d||!d->onInventoryChanged||!effect(b,d->onInventoryChanged(b->context,l,b))){ok=false;break;}
    }
    if(!ok)MCObjectHeap_fail(b->object.heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(b->object.heap);
}
ItemStack *InventoryBasic_decrStackSize(InventoryBasic *b,int32_t i,int32_t count){
    MCObjectRootScope scope={0};if(!begin(b,&scope))return NULL;ItemStack *result=NULL;
    if(!index(b,i)||!stack(b,b->inventoryContents->items[i]))goto done;
    if(b->inventoryContents->items[i]){
        if(b->inventoryContents->items[i]->stackSize<=count){result=b->inventoryContents->items[i];b->inventoryContents->items[i]=NULL;MCObjectHeap_touch(b->object.heap);}
        else{result=ItemStack_splitStack(b->object.heap,b->inventoryContents->items[i],count);if(!result)goto done;if(b->inventoryContents->items[i]->stackSize==0)b->inventoryContents->items[i]=NULL;MCObjectHeap_touch(b->object.heap);}
        if(!InventoryBasic_markDirty(b))result=NULL;
    }
done:MCObjectRootScope_end(&scope);return MCObjectHeap_failed(b->object.heap)?NULL:result;
}
ItemStack *InventoryBasic_removeStackFromSlot(InventoryBasic *b,int32_t i){if(!valid(b)||!index(b,i)||!stack(b,b->inventoryContents->items[i]))return NULL;ItemStack *s=b->inventoryContents->items[i];if(s){b->inventoryContents->items[i]=NULL;MCObjectHeap_touch(b->object.heap);}return s;}
bool InventoryBasic_setInventorySlotContents(InventoryBasic *b,int32_t i,ItemStack *s){
    MCObjectRootScope scope={0};if(!begin(b,&scope))return false;bool ok=index(b,i)&&stack(b,s);
    if(ok){b->inventoryContents->items[i]=s;MCObjectHeap_touch(b->object.heap);if(s&&s->stackSize>InventoryBasic_getInventoryStackLimit(b)){s->stackSize=InventoryBasic_getInventoryStackLimit(b);MCObjectHeap_touch(b->object.heap);}ok=InventoryBasic_markDirty(b);}
    MCObjectRootScope_end(&scope);return ok;
}
static int32_t signed32(uint32_t n){return n<=INT32_MAX?(int32_t)n:-1-(int32_t)(UINT32_MAX-n);}
ItemStack *InventoryBasic_func_174894_a(InventoryBasic *b,ItemStack *input){
    MCObjectRootScope scope={0};if(!begin(b,&scope))return NULL;ItemStack *result=NULL;
    if(!input||!stack(b,input)||!MCObjectRootScope_pin(&scope,(MCObject *)input)){MCObjectHeap_fail(b->object.heap);goto done;}
    ItemStack *copy=ItemStack_copy(b->object.heap,input);if(!copy)goto done;
    for(int32_t i=0;i<b->slotsCount;i++){
        ItemStack *current=InventoryBasic_getStackInSlot(b,i);if(MCObjectHeap_failed(b->object.heap))goto done;
        if(!current){if(!InventoryBasic_setInventorySlotContents(b,i,copy)||!InventoryBasic_markDirty(b))goto done;goto done;}
        if(ItemStack_areItemsEqual(current,copy)){
            int32_t limit=InventoryBasic_getInventoryStackLimit(b),max=ItemStack_getMaxStackSize(current);if(max<limit)limit=max;
            int32_t space=signed32((uint32_t)limit-(uint32_t)current->stackSize),amount=copy->stackSize<space?copy->stackSize:space;
            if(amount>0){current->stackSize=signed32((uint32_t)current->stackSize+(uint32_t)amount);copy->stackSize=signed32((uint32_t)copy->stackSize-(uint32_t)amount);MCObjectHeap_touch(b->object.heap);if(copy->stackSize<=0){(void)InventoryBasic_markDirty(b);goto done;}}
        }
        if(MCObjectHeap_failed(b->object.heap))goto done;
    }
    if(copy->stackSize!=input->stackSize&&!InventoryBasic_markDirty(b))goto done;
    result=copy;
done:MCObjectRootScope_end(&scope);return MCObjectHeap_failed(b->object.heap)?NULL:result;
}
int32_t InventoryBasic_getSizeInventory(const InventoryBasic *b){return valid(b)?b->slotsCount:0;}
NBTString *InventoryBasic_getName(const InventoryBasic *b){return valid(b)?b->inventoryTitle:NULL;}
bool InventoryBasic_hasCustomName(const InventoryBasic *b){return valid(b)&&b->hasCustomName;}
bool InventoryBasic_setCustomName(InventoryBasic *b,NBTString *name){if(!valid(b)||!effect(b,!name||(NBTString_isInstance((MCObject *)name)&&((MCObject *)name)->heap==b->object.heap)))return false;b->hasCustomName=true;b->inventoryTitle=name;MCObjectHeap_touch(b->object.heap);return true;}
MCObject *InventoryBasic_getDisplayName(InventoryBasic *b){
    MCObjectRootScope scope={0};if(!begin(b,&scope))return NULL;MCObject *out=NULL;const InventoryBasicDependencies *d=b->dependencies;
    if(ref(b,b->context)&&d){if(InventoryBasic_hasCustomName(b)){if(d->newChatComponentText)out=d->newChatComponentText(b->context,InventoryBasic_getName(b));}else if(d->newChatComponentTranslation)out=d->newChatComponentTranslation(b->context,InventoryBasic_getName(b));}
    if(!out||!ref(b,out))MCObjectHeap_fail(b->object.heap);
    MCObjectRootScope_end(&scope);return MCObjectHeap_failed(b->object.heap)?NULL:out;
}
int32_t InventoryBasic_getInventoryStackLimit(const InventoryBasic *b){return valid(b)?64:0;}
bool InventoryBasic_isUseableByPlayer(const InventoryBasic *b,const MCObject *p){(void)p;return valid(b);}
void InventoryBasic_openInventory(InventoryBasic *b,MCObject *p){(void)b;(void)p;}
void InventoryBasic_closeInventory(InventoryBasic *b,MCObject *p){(void)b;(void)p;}
bool InventoryBasic_isItemValidForSlot(const InventoryBasic *b,int32_t i,const ItemStack *s){(void)i;(void)s;return valid(b);}
int32_t InventoryBasic_getField(const InventoryBasic *b,int32_t id){(void)b;(void)id;return 0;}
void InventoryBasic_setField(InventoryBasic *b,int32_t id,int32_t value){(void)b;(void)id;(void)value;}
int32_t InventoryBasic_getFieldCount(const InventoryBasic *b){(void)b;return 0;}
void InventoryBasic_clear(InventoryBasic *b){if(!valid(b)||!contents(b))return;for(int32_t i=0;i<b->inventoryContents->length;i++)b->inventoryContents->items[i]=NULL;MCObjectHeap_touch(b->object.heap);}
static ItemStack *get(MCObject *o,int32_t i){return InventoryBasic_getStackInSlot((InventoryBasic *)o,i);}
static bool set(MCObject *o,int32_t i,ItemStack *s){return InventoryBasic_setInventorySlotContents((InventoryBasic *)o,i,s);}
static ItemStack *decr(MCObject *o,int32_t i,int32_t n){return InventoryBasic_decrStackSize((InventoryBasic *)o,i,n);}
static void dirty(MCObject *o){(void)InventoryBasic_markDirty((InventoryBasic *)o);}
static int32_t limit(const MCObject *o){return InventoryBasic_getInventoryStackLimit((const InventoryBasic *)o);}
static int32_t size(const MCObject *o){return InventoryBasic_getSizeInventory((const InventoryBasic *)o);}
static const IInventoryMethods methods={get,set,decr,dirty,limit,size};
IInventory InventoryBasic_asIInventory(InventoryBasic *b){return (IInventory){(MCObject *)b,&methods};}
