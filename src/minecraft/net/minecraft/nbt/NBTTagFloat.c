#include "nbt/NBTInternal.h"
static const MCObjectClass tagClass={"NBTTagFloat",MCObjectHeap_plainClone,NULL,NULL};
NBTTagFloat *NBTTagFloat_new(MCObjectHeap *heap,float value) {
    NBTTagFloat *tag=(NBTTagFloat*)MCObjectHeap_alloc(heap,sizeof(*tag),&tagClass);
    if (tag) { tag->base.base.type=5; tag->data=value; }
    return tag;
}
int64_t NBTTagFloat_getLong(const NBTTagFloat *tag) { return NBTPrimitive_getLong((const NBTPrimitive*)tag); }
int32_t NBTTagFloat_getInt(const NBTTagFloat *tag) { return NBTPrimitive_getInt((const NBTPrimitive*)tag); }
int16_t NBTTagFloat_getShort(const NBTTagFloat *tag) { return NBTPrimitive_getShort((const NBTPrimitive*)tag); }
int8_t NBTTagFloat_getByte(const NBTTagFloat *tag) { return NBTPrimitive_getByte((const NBTPrimitive*)tag); }
double NBTTagFloat_getDouble(const NBTTagFloat *tag) { return NBTPrimitive_getDouble((const NBTPrimitive*)tag); }
float NBTTagFloat_getFloat(const NBTTagFloat *tag) { return NBTPrimitive_getFloat((const NBTPrimitive*)tag); }
