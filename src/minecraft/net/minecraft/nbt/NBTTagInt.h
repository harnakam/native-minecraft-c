#ifndef C919_NATIVE_NBT_TAG_INT_H
#define C919_NATIVE_NBT_TAG_INT_H
#include "nbt/NBTPrimitive.h"
typedef struct NBTTagInt NBTTagInt;
NBTTagInt *NBTTagInt_new(MCObjectHeap *heap,int32_t value);
int64_t NBTTagInt_getLong(const NBTTagInt *tag);
int32_t NBTTagInt_getInt(const NBTTagInt *tag);
int16_t NBTTagInt_getShort(const NBTTagInt *tag);
int8_t NBTTagInt_getByte(const NBTTagInt *tag);
double NBTTagInt_getDouble(const NBTTagInt *tag);
float NBTTagInt_getFloat(const NBTTagInt *tag);
#endif
