#ifndef C919_NATIVE_NBT_TAG_COMPOUND_H
#define C919_NATIVE_NBT_TAG_COMPOUND_H
#include "nbt/NBTBase.h"
#include "nbt/NBTTagList.h"
#include "nbt/NBTTagByteArray.h"
#include "nbt/NBTTagIntArray.h"
typedef struct NBTCompoundKeySet NBTCompoundKeySet;
NBTTagCompound *NBTTagCompound_new(MCObjectHeap *heap);
/* Exact managed-class guard for native dependency calls. */
bool NBTTagCompound_isInstance(const MCObject *object);
bool NBTTagCompound_setTag(NBTTagCompound *tag,NBTString *key,NBTBase *value);
NBTBase *NBTTagCompound_getTag(const NBTTagCompound *tag,const NBTString *key);
int8_t NBTTagCompound_getTagId(const NBTTagCompound *tag,const NBTString *key);
bool NBTTagCompound_hasKey(const NBTTagCompound *tag,const NBTString *key);
bool NBTTagCompound_hasKeyType(const NBTTagCompound *tag,const NBTString *key,int32_t type);
bool NBTTagCompound_removeTag(NBTTagCompound *tag,const NBTString *key);
bool NBTTagCompound_merge(NBTTagCompound *tag,const NBTTagCompound *other);
NBTTagCompound *NBTTagCompound_getCompoundTag(NBTTagCompound *tag,const NBTString *key);
NBTTagList *NBTTagCompound_getTagList(NBTTagCompound *tag,const NBTString *key,int32_t type);
NBTString *NBTTagCompound_getString(NBTTagCompound *tag,const NBTString *key);
NBTByteArrayStorage *NBTTagCompound_getByteArray(NBTTagCompound *tag,const NBTString *key);
NBTIntArrayStorage *NBTTagCompound_getIntArray(NBTTagCompound *tag,const NBTString *key);
bool NBTTagCompound_setString(NBTTagCompound *tag,NBTString *key,NBTString *value);
bool NBTTagCompound_setByteArray(NBTTagCompound *tag,NBTString *key,NBTByteArrayStorage *value);
bool NBTTagCompound_setIntArray(NBTTagCompound *tag,NBTString *key,NBTIntArrayStorage *value);
bool NBTTagCompound_getBoolean(const NBTTagCompound *tag,const NBTString *key);
bool NBTTagCompound_setBoolean(NBTTagCompound *tag,NBTString *key,bool value);
NBTCompoundKeySet *NBTTagCompound_getKeySet(NBTTagCompound *tag);
int32_t NBTCompoundKeySet_size(const NBTCompoundKeySet *set);
bool NBTCompoundKeySet_contains(const NBTCompoundKeySet *set,const NBTString *key);
bool NBTCompoundKeySet_remove(NBTCompoundKeySet *set,const NBTString *key);
bool NBTCompoundKeySet_clear(NBTCompoundKeySet *set);
/* Iteration borrows a live key; mutation invalidates the caller's index walk. */
NBTString *NBTCompoundKeySet_keyAt(const NBTCompoundKeySet *set,int32_t index);
NBTBase *NBTTagCompound_getTag_ascii(const NBTTagCompound *tag,const char *key);
bool NBTTagCompound_setTag_ascii(NBTTagCompound *tag,const char *key,NBTBase *value);
int8_t NBTTagCompound_getTagId_ascii(const NBTTagCompound *tag,const char *key);
bool NBTTagCompound_hasKey_ascii(const NBTTagCompound *tag,const char *key);
bool NBTTagCompound_hasKeyType_ascii(const NBTTagCompound *tag,const char *key,int32_t type);
bool NBTTagCompound_removeTag_ascii(NBTTagCompound *tag,const char *key);
NBTTagCompound *NBTTagCompound_getCompoundTag_ascii(NBTTagCompound *tag,const char *key);
NBTTagList *NBTTagCompound_getTagList_ascii(NBTTagCompound *tag,const char *key,int32_t type);
NBTString *NBTTagCompound_getString_ascii(NBTTagCompound *tag,const char *key);
bool NBTTagCompound_setString_ascii(NBTTagCompound *tag,const char *key,NBTString *value);
bool NBTTagCompound_getBoolean_ascii(const NBTTagCompound *tag,const char *key);
bool NBTTagCompound_setBoolean_ascii(NBTTagCompound *tag,const char *key,bool value);
int8_t NBTTagCompound_getByte(const NBTTagCompound *tag,const NBTString *key);
bool NBTTagCompound_setByte(NBTTagCompound *tag,NBTString *key,int8_t value);
int8_t NBTTagCompound_getByte_ascii(const NBTTagCompound *tag,const char *key);
bool NBTTagCompound_setByte_ascii(NBTTagCompound *tag,const char *key,int8_t value);
int16_t NBTTagCompound_getShort(const NBTTagCompound *tag,const NBTString *key);
bool NBTTagCompound_setShort(NBTTagCompound *tag,NBTString *key,int16_t value);
int16_t NBTTagCompound_getShort_ascii(const NBTTagCompound *tag,const char *key);
bool NBTTagCompound_setShort_ascii(NBTTagCompound *tag,const char *key,int16_t value);
int32_t NBTTagCompound_getInteger(const NBTTagCompound *tag,const NBTString *key);
bool NBTTagCompound_setInteger(NBTTagCompound *tag,NBTString *key,int32_t value);
int32_t NBTTagCompound_getInteger_ascii(const NBTTagCompound *tag,const char *key);
bool NBTTagCompound_setInteger_ascii(NBTTagCompound *tag,const char *key,int32_t value);
int64_t NBTTagCompound_getLong(const NBTTagCompound *tag,const NBTString *key);
bool NBTTagCompound_setLong(NBTTagCompound *tag,NBTString *key,int64_t value);
int64_t NBTTagCompound_getLong_ascii(const NBTTagCompound *tag,const char *key);
bool NBTTagCompound_setLong_ascii(NBTTagCompound *tag,const char *key,int64_t value);
float NBTTagCompound_getFloat(const NBTTagCompound *tag,const NBTString *key);
bool NBTTagCompound_setFloat(NBTTagCompound *tag,NBTString *key,float value);
float NBTTagCompound_getFloat_ascii(const NBTTagCompound *tag,const char *key);
bool NBTTagCompound_setFloat_ascii(NBTTagCompound *tag,const char *key,float value);
double NBTTagCompound_getDouble(const NBTTagCompound *tag,const NBTString *key);
bool NBTTagCompound_setDouble(NBTTagCompound *tag,NBTString *key,double value);
double NBTTagCompound_getDouble_ascii(const NBTTagCompound *tag,const char *key);
bool NBTTagCompound_setDouble_ascii(NBTTagCompound *tag,const char *key,double value);
#endif
