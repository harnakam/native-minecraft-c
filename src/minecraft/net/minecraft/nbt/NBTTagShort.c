#include "nbt/NBTInternal.h"
static const MCObjectClass tagClass={"NBTTagShort",MCObjectHeap_plainClone,NULL,NULL};
NBTTagShort *NBTTagShort_new(MCObjectHeap *heap,int16_t value) {
    NBTTagShort *tag=(NBTTagShort*)MCObjectHeap_alloc(heap,sizeof(*tag),&tagClass);
    if (tag) { tag->base.base.type=2; tag->data=value; }
    return tag;
}
int64_t NBTTagShort_getLong(const NBTTagShort *tag) { return NBTPrimitive_getLong((const NBTPrimitive*)tag); }
int32_t NBTTagShort_getInt(const NBTTagShort *tag) { return NBTPrimitive_getInt((const NBTPrimitive*)tag); }
int16_t NBTTagShort_getShort(const NBTTagShort *tag) { return NBTPrimitive_getShort((const NBTPrimitive*)tag); }
int8_t NBTTagShort_getByte(const NBTTagShort *tag) { return NBTPrimitive_getByte((const NBTPrimitive*)tag); }
double NBTTagShort_getDouble(const NBTTagShort *tag) { return NBTPrimitive_getDouble((const NBTPrimitive*)tag); }
float NBTTagShort_getFloat(const NBTTagShort *tag) { return NBTPrimitive_getFloat((const NBTPrimitive*)tag); }
