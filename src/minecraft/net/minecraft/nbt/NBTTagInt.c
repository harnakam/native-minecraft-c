#include "nbt/NBTInternal.h"
static const MCObjectClass tagClass={"NBTTagInt",MCObjectHeap_plainClone,NULL,NULL};
NBTTagInt *NBTTagInt_new(MCObjectHeap *heap,int32_t value) {
    NBTTagInt *tag=(NBTTagInt*)MCObjectHeap_alloc(heap,sizeof(*tag),&tagClass);
    if (tag) { tag->base.base.type=3; tag->data=value; }
    return tag;
}
int64_t NBTTagInt_getLong(const NBTTagInt *tag) { return NBTPrimitive_getLong((const NBTPrimitive*)tag); }
int32_t NBTTagInt_getInt(const NBTTagInt *tag) { return NBTPrimitive_getInt((const NBTPrimitive*)tag); }
int16_t NBTTagInt_getShort(const NBTTagInt *tag) { return NBTPrimitive_getShort((const NBTPrimitive*)tag); }
int8_t NBTTagInt_getByte(const NBTTagInt *tag) { return NBTPrimitive_getByte((const NBTPrimitive*)tag); }
double NBTTagInt_getDouble(const NBTTagInt *tag) { return NBTPrimitive_getDouble((const NBTPrimitive*)tag); }
float NBTTagInt_getFloat(const NBTTagInt *tag) { return NBTPrimitive_getFloat((const NBTPrimitive*)tag); }
