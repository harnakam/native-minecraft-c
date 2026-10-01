#ifndef C919_NATIVE_NBT_TAG_BYTE_H
#define C919_NATIVE_NBT_TAG_BYTE_H
#include "nbt/NBTPrimitive.h"
typedef struct NBTTagByte NBTTagByte;
NBTTagByte *NBTTagByte_new(MCObjectHeap *heap,int8_t value);
int64_t NBTTagByte_getLong(const NBTTagByte *tag);
int32_t NBTTagByte_getInt(const NBTTagByte *tag);
int16_t NBTTagByte_getShort(const NBTTagByte *tag);
int8_t NBTTagByte_getByte(const NBTTagByte *tag);
double NBTTagByte_getDouble(const NBTTagByte *tag);
float NBTTagByte_getFloat(const NBTTagByte *tag);
#endif
