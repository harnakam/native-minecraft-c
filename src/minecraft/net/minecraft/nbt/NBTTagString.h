#ifndef C919_NATIVE_NBT_TAG_STRING_H
#define C919_NATIVE_NBT_TAG_STRING_H
#include "nbt/NBTBase.h"
typedef struct NBTTagString NBTTagString;
NBTTagString *NBTTagString_new(MCObjectHeap *heap,NBTString *value);
NBTString *NBTTagString_getString(const NBTTagString *tag);
#endif
