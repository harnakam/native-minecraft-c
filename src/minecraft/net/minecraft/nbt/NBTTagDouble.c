#include "nbt/NBTInternal.h"
static const MCObjectClass tagClass={"NBTTagDouble",MCObjectHeap_plainClone,NULL,NULL};
NBTTagDouble *NBTTagDouble_new(MCObjectHeap *heap,double value) {
    NBTTagDouble *tag=(NBTTagDouble*)MCObjectHeap_alloc(heap,sizeof(*tag),&tagClass);
    if (tag) { tag->base.base.type=6; tag->data=value; }
    return tag;
}
int64_t NBTTagDouble_getLong(const NBTTagDouble *tag) { return NBTPrimitive_getLong((const NBTPrimitive*)tag); }
int32_t NBTTagDouble_getInt(const NBTTagDouble *tag) { return NBTPrimitive_getInt((const NBTPrimitive*)tag); }
int16_t NBTTagDouble_getShort(const NBTTagDouble *tag) { return NBTPrimitive_getShort((const NBTPrimitive*)tag); }
int8_t NBTTagDouble_getByte(const NBTTagDouble *tag) { return NBTPrimitive_getByte((const NBTPrimitive*)tag); }
double NBTTagDouble_getDouble(const NBTTagDouble *tag) { return NBTPrimitive_getDouble((const NBTPrimitive*)tag); }
float NBTTagDouble_getFloat(const NBTTagDouble *tag) { return NBTPrimitive_getFloat((const NBTPrimitive*)tag); }
