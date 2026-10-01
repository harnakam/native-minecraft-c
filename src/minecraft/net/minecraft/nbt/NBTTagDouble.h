#ifndef C919_NATIVE_NBT_TAG_DOUBLE_H
#define C919_NATIVE_NBT_TAG_DOUBLE_H
#include "nbt/NBTPrimitive.h"
typedef struct NBTTagDouble NBTTagDouble;
NBTTagDouble *NBTTagDouble_new(MCObjectHeap *heap,double value);
int64_t NBTTagDouble_getLong(const NBTTagDouble *tag);
int32_t NBTTagDouble_getInt(const NBTTagDouble *tag);
int16_t NBTTagDouble_getShort(const NBTTagDouble *tag);
int8_t NBTTagDouble_getByte(const NBTTagDouble *tag);
double NBTTagDouble_getDouble(const NBTTagDouble *tag);
float NBTTagDouble_getFloat(const NBTTagDouble *tag);
#endif
