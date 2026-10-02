#include "nbt/NBTInternal.h"
#include <limits.h>
#include <string.h>
static void storageTrace(MCObject *object,MCObjectVisitor visit,void *context) {
    NBTRefStorage *s=(NBTRefStorage*)object;
    for (int32_t i=0;i<s->capacity;i++) s->data[i]=(NBTBase*)visit((MCObject*)s->data[i],context);
}
static const MCObjectClass storageClass={"NBTTagList.ArrayListStorage",MCObjectHeap_plainClone,storageTrace,NULL};
static void trace(MCObject *object,MCObjectVisitor visit,void *context) {
    NBTTagList *list=(NBTTagList*)object;list->storage=(NBTRefStorage*)visit((MCObject*)list->storage,context);
}
static const MCObjectClass tagClass={"NBTTagList",MCObjectHeap_plainClone,trace,NULL};
static bool list_identity(const MCObject *o,void *c) { return o==c; }
bool NBTTagList_isInstance(const MCObject *o) {
    if(!o || o->klass!=&tagClass ||
       MCObjectHeap_findObject(o->heap,&tagClass,list_identity,(void *)o)!=o ||
       MCObjectHeap_objectSize(o)<sizeof(NBTTagList)) return false;
    const NBTTagList *l=(const NBTTagList *)o;
    if(l->base.type!=9 || l->count<0) return false;
    if(!l->storage) return l->count==0;
    const MCObject *s=(const MCObject *)l->storage;
    if(s->heap!=o->heap || s->klass!=&storageClass ||
       MCObjectHeap_findObject(o->heap,&storageClass,list_identity,(void *)s)!=s ||
       MCObjectHeap_objectSize(s)<sizeof(NBTRefStorage)) return false;
    return l->storage->capacity>=l->count && l->storage->capacity>=0 &&
       (size_t)l->storage->capacity<=(MCObjectHeap_objectSize(s)-sizeof(NBTRefStorage))/sizeof(NBTBase *);
}
NBTTagList *NBTTagList_new(MCObjectHeap *heap) {
    NBTTagList *list=(NBTTagList*)MCObjectHeap_alloc(heap,sizeof(*list),&tagClass);
    if (list) list->base.type=9;
    return list;
}
bool nbt_listReserve(NBTTagList *list,int32_t need) {
    if (!list || need<0) return false;
    int32_t old=list->storage?list->storage->capacity:0;
    if (need<=old) return true;
    int64_t capacity=old?(int64_t)old+old/2:10;
    if (capacity<need) capacity=need;
    if (capacity>INT32_MAX || (uint64_t)capacity>(SIZE_MAX-sizeof(NBTRefStorage))/sizeof(NBTBase*)) {
        MCObjectHeap_fail(list->base.object.heap);return false;
    }
    NBTRefStorage *s=(NBTRefStorage*)MCObjectHeap_alloc(list->base.object.heap,sizeof(*s)+(size_t)capacity*sizeof(NBTBase*),&storageClass);
    if (!s) return false;
    s->capacity=(int32_t)capacity;
    if (list->count) memcpy(s->data,list->storage->data,(size_t)list->count*sizeof(NBTBase*));
    list->storage=s;MCObjectHeap_touch(list->base.object.heap);return true;
}
bool NBTTagList_appendTag(NBTTagList *list,NBTBase *tag) {
    if (!list || !tag) { if(list)MCObjectHeap_fail(list->base.object.heap);return false; }
    if (tag->type==0) return true;
    if (list->tagType && list->tagType!=tag->type) return true;
    if (!nbt_sameHeap(list->base.object.heap,(MCObject*)tag)) return false;
    if (!list->tagType) { list->tagType=tag->type;MCObjectHeap_touch(list->base.object.heap); }
    if (list->count==INT32_MAX || !nbt_listReserve(list,list->count+1)) return false;
    list->storage->data[list->count++]=tag;MCObjectHeap_touch(list->base.object.heap);return true;
}
bool NBTTagList_set(NBTTagList *list,int32_t index,NBTBase *tag) {
    if (!list || !tag) { if(list)MCObjectHeap_fail(list->base.object.heap);return false; }
    if (tag->type==0 || index<0 || index>=list->count) return true;
    if (list->tagType && list->tagType!=tag->type) return true;
    if (!nbt_sameHeap(list->base.object.heap,(MCObject*)tag)) return false;
    if (!list->tagType) list->tagType=tag->type;
    list->storage->data[index]=tag;MCObjectHeap_touch(list->base.object.heap);return true;
}
NBTBase *NBTTagList_removeTag(NBTTagList *list,int32_t index) {
    if (!list || index<0 || index>=list->count) { if(list)MCObjectHeap_fail(list->base.object.heap);return NULL; }
    NBTBase *removed=list->storage->data[index];
    size_t move=(size_t)(list->count-index-1);
    if(move)memmove(list->storage->data+index,list->storage->data+index+1,move*sizeof(NBTBase*));
    list->storage->data[--list->count]=NULL;MCObjectHeap_touch(list->base.object.heap);return removed;
}
NBTBase *NBTTagList_get(NBTTagList *list,int32_t index) {
    if (!list) return NULL;
    return index>=0 && index<list->count?list->storage->data[index]:(NBTBase*)NBTTagEnd_new(list->base.object.heap);
}
NBTTagCompound *NBTTagList_getCompoundTagAt(NBTTagList *list,int32_t index) {
    if (!list) return NULL;
    NBTBase *tag=index>=0 && index<list->count?list->storage->data[index]:NULL;
    return tag && tag->type==10?(NBTTagCompound*)tag:NBTTagCompound_new(list->base.object.heap);
}
NBTIntArrayStorage *NBTTagList_getIntArrayAt(NBTTagList *list,int32_t index) {
    if (!list) return NULL;
    NBTBase *tag=index>=0 && index<list->count?list->storage->data[index]:NULL;
    return tag && tag->type==11?((NBTTagIntArray*)tag)->data:NBTIntArrayStorage_new(list->base.object.heap,NULL,0);
}
double NBTTagList_getDoubleAt(const NBTTagList *list,int32_t index) {
    NBTBase *tag=list && index>=0 && index<list->count?list->storage->data[index]:NULL;
    return tag && tag->type==6?((NBTTagDouble*)tag)->data:0;
}
float NBTTagList_getFloatAt(const NBTTagList *list,int32_t index) {
    NBTBase *tag=list && index>=0 && index<list->count?list->storage->data[index]:NULL;
    return tag && tag->type==5?((NBTTagFloat*)tag)->data:0;
}
int32_t NBTTagList_tagCount(const NBTTagList *list) { return list?list->count:0; }
int32_t NBTTagList_getTagType(const NBTTagList *list) { return list?list->tagType:0; }
