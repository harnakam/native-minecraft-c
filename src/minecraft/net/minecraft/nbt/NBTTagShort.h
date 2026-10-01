#ifndef C919_NATIVE_NBT_TAG_SHORT_H
#define C919_NATIVE_NBT_TAG_SHORT_H
#include "nbt/NBTPrimitive.h"
typedef struct NBTTagShort NBTTagShort;
NBTTagShort *NBTTagShort_new(MCObjectHeap *heap,int16_t value);
int64_t NBTTagShort_getLong(const NBTTagShort *tag);
int32_t NBTTagShort_getInt(const NBTTagShort *tag);
int16_t NBTTagShort_getShort(const NBTTagShort *tag);
int8_t NBTTagShort_getByte(const NBTTagShort *tag);
double NBTTagShort_getDouble(const NBTTagShort *tag);
float NBTTagShort_getFloat(const NBTTagShort *tag);
#endif
