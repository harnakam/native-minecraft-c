#ifndef C919_NATIVE_NBT_TAG_BYTE_ARRAY_H
#define C919_NATIVE_NBT_TAG_BYTE_ARRAY_H
#include "nbt/NBTBase.h"
typedef struct NBTByteArrayStorage NBTByteArrayStorage;
typedef struct NBTTagByteArray NBTTagByteArray;
NBTByteArrayStorage *NBTByteArrayStorage_new(MCObjectHeap *heap,const int8_t *data,int32_t length);
int32_t NBTByteArrayStorage_length(const NBTByteArrayStorage *array);
int8_t *NBTByteArrayStorage_data(NBTByteArrayStorage *array);
bool NBTByteArrayStorage_set(NBTByteArrayStorage *array,int32_t index,int8_t value);
/* Constructor retains the exact storage; getter returns it. Raw data mutations
   must call MCObjectHeap_touch; set performs this automatically. */
NBTTagByteArray *NBTTagByteArray_new(MCObjectHeap *heap,NBTByteArrayStorage *array);
NBTByteArrayStorage *NBTTagByteArray_getByteArray(const NBTTagByteArray *tag);
#endif
