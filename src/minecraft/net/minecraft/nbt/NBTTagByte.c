#include "nbt/NBTInternal.h"
static const MCObjectClass tagClass={"NBTTagByte",MCObjectHeap_plainClone,NULL,NULL};
NBTTagByte *NBTTagByte_new(MCObjectHeap *heap,int8_t value) {
    NBTTagByte *tag=(NBTTagByte*)MCObjectHeap_alloc(heap,sizeof(*tag),&tagClass);
    if (tag) { tag->base.base.type=1; tag->data=value; }
    return tag;
}
int64_t NBTTagByte_getLong(const NBTTagByte *tag) { return NBTPrimitive_getLong((const NBTPrimitive*)tag); }
int32_t NBTTagByte_getInt(const NBTTagByte *tag) { return NBTPrimitive_getInt((const NBTPrimitive*)tag); }
int16_t NBTTagByte_getShort(const NBTTagByte *tag) { return NBTPrimitive_getShort((const NBTPrimitive*)tag); }
int8_t NBTTagByte_getByte(const NBTTagByte *tag) { return NBTPrimitive_getByte((const NBTPrimitive*)tag); }
double NBTTagByte_getDouble(const NBTTagByte *tag) { return NBTPrimitive_getDouble((const NBTPrimitive*)tag); }
float NBTTagByte_getFloat(const NBTTagByte *tag) { return NBTPrimitive_getFloat((const NBTPrimitive*)tag); }
