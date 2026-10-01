#include "nbt/NBTInternal.h"
static const MCObjectClass tagClass={"NBTTagLong",MCObjectHeap_plainClone,NULL,NULL};
NBTTagLong *NBTTagLong_new(MCObjectHeap *heap,int64_t value) {
    NBTTagLong *tag=(NBTTagLong*)MCObjectHeap_alloc(heap,sizeof(*tag),&tagClass);
    if (tag) { tag->base.base.type=4; tag->data=value; }
    return tag;
}
int64_t NBTTagLong_getLong(const NBTTagLong *tag) { return NBTPrimitive_getLong((const NBTPrimitive*)tag); }
int32_t NBTTagLong_getInt(const NBTTagLong *tag) { return NBTPrimitive_getInt((const NBTPrimitive*)tag); }
int16_t NBTTagLong_getShort(const NBTTagLong *tag) { return NBTPrimitive_getShort((const NBTPrimitive*)tag); }
int8_t NBTTagLong_getByte(const NBTTagLong *tag) { return NBTPrimitive_getByte((const NBTPrimitive*)tag); }
double NBTTagLong_getDouble(const NBTTagLong *tag) { return NBTPrimitive_getDouble((const NBTPrimitive*)tag); }
float NBTTagLong_getFloat(const NBTTagLong *tag) { return NBTPrimitive_getFloat((const NBTPrimitive*)tag); }
