#ifndef C919_NATIVE_NBT_PRIMITIVE_H
#define C919_NATIVE_NBT_PRIMITIVE_H
#include "nbt/NBTBase.h"
/* Native counterpart of nested NBTBase.NBTPrimitive. */
typedef struct NBTPrimitive { NBTBase base; } NBTPrimitive;
int64_t NBTPrimitive_getLong(const NBTPrimitive *tag);
int32_t NBTPrimitive_getInt(const NBTPrimitive *tag);
int16_t NBTPrimitive_getShort(const NBTPrimitive *tag);
int8_t NBTPrimitive_getByte(const NBTPrimitive *tag);
double NBTPrimitive_getDouble(const NBTPrimitive *tag);
float NBTPrimitive_getFloat(const NBTPrimitive *tag);
#endif
