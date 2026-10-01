#ifndef C919_NATIVE_NBT_TAG_LIST_H
#define C919_NATIVE_NBT_TAG_LIST_H
#include "nbt/NBTBase.h"
#include "nbt/NBTTagIntArray.h"
typedef struct NBTTagList NBTTagList;
NBTTagList *NBTTagList_new(MCObjectHeap *heap);
bool NBTTagList_appendTag(NBTTagList *list,NBTBase *tag);
bool NBTTagList_set(NBTTagList *list,int32_t index,NBTBase *tag);
NBTBase *NBTTagList_removeTag(NBTTagList *list,int32_t index);
NBTBase *NBTTagList_get(NBTTagList *list,int32_t index);
NBTTagCompound *NBTTagList_getCompoundTagAt(NBTTagList *list,int32_t index);
NBTIntArrayStorage *NBTTagList_getIntArrayAt(NBTTagList *list,int32_t index);
double NBTTagList_getDoubleAt(const NBTTagList *list,int32_t index);
float NBTTagList_getFloatAt(const NBTTagList *list,int32_t index);
int32_t NBTTagList_tagCount(const NBTTagList *list);
int32_t NBTTagList_getTagType(const NBTTagList *list);
#endif
