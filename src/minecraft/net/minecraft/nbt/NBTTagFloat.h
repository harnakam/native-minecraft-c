#ifndef C919_NATIVE_NBT_TAG_FLOAT_H
#define C919_NATIVE_NBT_TAG_FLOAT_H
#include "nbt/NBTPrimitive.h"
typedef struct NBTTagFloat NBTTagFloat;
NBTTagFloat *NBTTagFloat_new(MCObjectHeap *heap,float value);
int64_t NBTTagFloat_getLong(const NBTTagFloat *tag);
int32_t NBTTagFloat_getInt(const NBTTagFloat *tag);
int16_t NBTTagFloat_getShort(const NBTTagFloat *tag);
int8_t NBTTagFloat_getByte(const NBTTagFloat *tag);
double NBTTagFloat_getDouble(const NBTTagFloat *tag);
float NBTTagFloat_getFloat(const NBTTagFloat *tag);
#endif
