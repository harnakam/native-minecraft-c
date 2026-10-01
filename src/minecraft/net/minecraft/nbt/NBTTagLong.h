#ifndef C919_NATIVE_NBT_TAG_LONG_H
#define C919_NATIVE_NBT_TAG_LONG_H
#include "nbt/NBTPrimitive.h"
typedef struct NBTTagLong NBTTagLong;
NBTTagLong *NBTTagLong_new(MCObjectHeap *heap,int64_t value);
int64_t NBTTagLong_getLong(const NBTTagLong *tag);
int32_t NBTTagLong_getInt(const NBTTagLong *tag);
int16_t NBTTagLong_getShort(const NBTTagLong *tag);
int8_t NBTTagLong_getByte(const NBTTagLong *tag);
double NBTTagLong_getDouble(const NBTTagLong *tag);
float NBTTagLong_getFloat(const NBTTagLong *tag);
#endif
