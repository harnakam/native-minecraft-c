#ifndef C919_NATIVE_NBT_INTERNAL_H
#define C919_NATIVE_NBT_INTERNAL_H
#include "nbt/NBTBase.h"
#include "nbt/NBTPrimitive.h"
#include "nbt/NBTTagEnd.h"
#include "nbt/NBTTagByte.h"
#include "nbt/NBTTagShort.h"
#include "nbt/NBTTagInt.h"
#include "nbt/NBTTagLong.h"
#include "nbt/NBTTagFloat.h"
#include "nbt/NBTTagDouble.h"
#include "nbt/NBTTagString.h"
#include "nbt/NBTTagByteArray.h"
#include "nbt/NBTTagIntArray.h"
#include "nbt/NBTTagList.h"
#include "nbt/NBTTagCompound.h"
struct NBTString { MCObject object; size_t length; bool literal; uint16_t units[]; };
struct NBTTagEnd { NBTBase base; };
struct NBTTagByte { NBTPrimitive base; int8_t data; };
struct NBTTagShort { NBTPrimitive base; int16_t data; };
struct NBTTagInt { NBTPrimitive base; int32_t data; };
struct NBTTagLong { NBTPrimitive base; int64_t data; };
struct NBTTagFloat { NBTPrimitive base; float data; };
struct NBTTagDouble { NBTPrimitive base; double data; };
struct NBTTagString { NBTBase base; NBTString *data; };
struct NBTByteArrayStorage { MCObject object; int32_t length; int8_t data[]; };
struct NBTIntArrayStorage { MCObject object; int32_t length; int32_t data[]; };
struct NBTTagByteArray { NBTBase base; NBTByteArrayStorage *data; };
struct NBTTagIntArray { NBTBase base; NBTIntArrayStorage *data; };
typedef struct NBTRefStorage { MCObject object; int32_t capacity; NBTBase *data[]; } NBTRefStorage;
struct NBTTagList { NBTBase base; NBTRefStorage *storage; int32_t count; int8_t tagType; };
typedef struct NBTCompoundEntry {
    MCObject object; NBTString *key; NBTBase *value;
    struct NBTCompoundEntry *next,*previous; uint32_t hash;
    struct NBTCompoundEntry *left,*right,*parent; int32_t height;
} NBTCompoundEntry;
typedef struct NBTBucketStorage { MCObject object; int32_t capacity; NBTCompoundEntry *data[]; } NBTBucketStorage;
struct NBTCompoundKeySet { MCObject object; NBTTagCompound *owner; };
struct NBTTagCompound {
    NBTBase base; NBTBucketStorage *storage; NBTCompoundKeySet *keySet;
    int32_t count; uint32_t modification; NBTCompoundEntry *index;
};
int32_t nbt_i32(uint32_t bits);
int16_t nbt_i16(uint16_t bits);
int8_t nbt_i8(uint8_t bits);
int32_t nbt_javaDoubleInt(double value);
int64_t nbt_javaDoubleLong(double value);
int32_t nbt_floorFloat(float value);
int32_t nbt_floorDouble(double value);
bool nbt_sameHeap(MCObjectHeap *heap,const MCObject *object);
NBTString *nbt_readUTF(MCObjectHeap *heap,mc_buf *input);
bool nbt_writeUTF(mc_buf *output,const NBTString *text);
bool nbt_listReserve(NBTTagList *list,int32_t capacity);
NBTCompoundEntry *nbt_compoundFind(const NBTTagCompound *tag,const NBTString *key);
NBTCompoundEntry *nbt_compoundFindASCII(const NBTTagCompound *tag,const char *key);
bool nbt_compoundClear(NBTTagCompound *tag);
#endif
