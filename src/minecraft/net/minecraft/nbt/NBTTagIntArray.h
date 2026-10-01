#ifndef C919_NATIVE_NBT_TAG_INT_ARRAY_H
#define C919_NATIVE_NBT_TAG_INT_ARRAY_H
#include "nbt/NBTBase.h"
typedef struct NBTIntArrayStorage NBTIntArrayStorage;
typedef struct NBTTagIntArray NBTTagIntArray;
NBTIntArrayStorage *NBTIntArrayStorage_new(MCObjectHeap *heap,const int32_t *data,int32_t length);
int32_t NBTIntArrayStorage_length(const NBTIntArrayStorage *array);
int32_t *NBTIntArrayStorage_data(NBTIntArrayStorage *array);
bool NBTIntArrayStorage_set(NBTIntArrayStorage *array,int32_t index,int32_t value);
NBTTagIntArray *NBTTagIntArray_new(MCObjectHeap *heap,NBTIntArrayStorage *array);
NBTIntArrayStorage *NBTTagIntArray_getIntArray(const NBTTagIntArray *tag);
#endif
