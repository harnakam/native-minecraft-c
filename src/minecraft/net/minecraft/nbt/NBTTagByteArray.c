#include "nbt/NBTInternal.h"
#include <string.h>
NBTByteArrayStorage *NBTByteArrayStorage_new(MCObjectHeap *heap,const int8_t *data,int32_t length) {
    NBTByteArrayStorage *s=NativeByteArray_new(heap,length);
    if (!s) return NULL;
    if(length && data)memcpy(s->values,data,(size_t)length*sizeof(int8_t));
    return s;
}
int32_t NBTByteArrayStorage_length(const NBTByteArrayStorage *s) { return s?s->length:0; }
int8_t *NBTByteArrayStorage_data(NBTByteArrayStorage *s) { return s?s->values:NULL; }
bool NBTByteArrayStorage_set(NBTByteArrayStorage *s,int32_t i,int8_t value) {
    if(!s || i<0 || i>=s->length) { if(s)MCObjectHeap_fail(s->object.heap);return false; }
    s->values[i]=value;MCObjectHeap_touch(s->object.heap);return true;
}
static void trace(MCObject *object,MCObjectVisitor visit,void *context) {
    NBTTagByteArray *tag=(NBTTagByteArray*)object;tag->data=(NBTByteArrayStorage*)visit((MCObject*)tag->data,context);
}
static const MCObjectClass tagClass={"NBTTagByteArray",MCObjectHeap_plainClone,trace,NULL};
NBTTagByteArray *NBTTagByteArray_new(MCObjectHeap *heap,NBTByteArrayStorage *array) {
    if(!nbt_sameHeap(heap,(MCObject*)array))return NULL;
    NBTTagByteArray *tag=(NBTTagByteArray*)MCObjectHeap_alloc(heap,sizeof(*tag),&tagClass);
    if(tag) { tag->base.type=7;tag->data=array; }
    return tag;
}
NBTByteArrayStorage *NBTTagByteArray_getByteArray(const NBTTagByteArray *tag) { return tag?tag->data:NULL; }
