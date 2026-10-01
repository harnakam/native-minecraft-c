#include "nbt/NBTInternal.h"
#include <string.h>
static const MCObjectClass storageClass={"NBTIntArrayStorage",MCObjectHeap_plainClone,NULL,NULL};
NBTIntArrayStorage *NBTIntArrayStorage_new(MCObjectHeap *heap,const int32_t *data,int32_t length) {
    if (length<0 || (size_t)length>(SIZE_MAX-sizeof(NBTIntArrayStorage))/sizeof(int32_t)) { MCObjectHeap_fail(heap);return NULL; }
    NBTIntArrayStorage *s=(NBTIntArrayStorage*)MCObjectHeap_alloc(heap,sizeof(*s)+(size_t)length*sizeof(int32_t),&storageClass);
    if (!s) return NULL;
    s->length=length;
    if(length && data)memcpy(s->data,data,(size_t)length*sizeof(int32_t));
    return s;
}
int32_t NBTIntArrayStorage_length(const NBTIntArrayStorage *s) { return s?s->length:0; }
int32_t *NBTIntArrayStorage_data(NBTIntArrayStorage *s) { return s?s->data:NULL; }
bool NBTIntArrayStorage_set(NBTIntArrayStorage *s,int32_t i,int32_t value) {
    if(!s || i<0 || i>=s->length) { if(s)MCObjectHeap_fail(s->object.heap);return false; }
    s->data[i]=value;MCObjectHeap_touch(s->object.heap);return true;
}
static void trace(MCObject *object,MCObjectVisitor visit,void *context) {
    NBTTagIntArray *tag=(NBTTagIntArray*)object;tag->data=(NBTIntArrayStorage*)visit((MCObject*)tag->data,context);
}
static const MCObjectClass tagClass={"NBTTagIntArray",MCObjectHeap_plainClone,trace,NULL};
NBTTagIntArray *NBTTagIntArray_new(MCObjectHeap *heap,NBTIntArrayStorage *array) {
    if(!nbt_sameHeap(heap,(MCObject*)array))return NULL;
    NBTTagIntArray *tag=(NBTTagIntArray*)MCObjectHeap_alloc(heap,sizeof(*tag),&tagClass);
    if(tag) { tag->base.type=11;tag->data=array; }
    return tag;
}
NBTIntArrayStorage *NBTTagIntArray_getIntArray(const NBTTagIntArray *tag) { return tag?tag->data:NULL; }
