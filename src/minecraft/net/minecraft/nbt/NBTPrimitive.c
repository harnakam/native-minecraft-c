#include "nbt/NBTInternal.h"
#include <limits.h>
#include <math.h>
#include <string.h>
int32_t nbt_i32(uint32_t bits) { int32_t v; memcpy(&v,&bits,4); return v; }
int16_t nbt_i16(uint16_t bits) { int16_t v; memcpy(&v,&bits,2); return v; }
int8_t nbt_i8(uint8_t bits) { int8_t v; memcpy(&v,&bits,1); return v; }
bool nbt_sameHeap(MCObjectHeap *heap,const MCObject *object) {
    if (!heap || (object && object->heap!=heap)) { MCObjectHeap_fail(heap); return false; } return true;
}
int32_t nbt_javaDoubleInt(double v) {
    if (isnan(v)) return 0;
    if (v>=2147483648.0) return INT32_MAX;
    if (v<=-2147483648.0) return INT32_MIN;
    return (int32_t)v;
}
int64_t nbt_javaDoubleLong(double v) {
    if (isnan(v)) return 0;
    if (v>=9223372036854775808.0) return INT64_MAX;
    if (v<=-9223372036854775808.0) return INT64_MIN;
    return (int64_t)v;
}
int32_t nbt_floorFloat(float v) {
    int32_t i=nbt_javaDoubleInt(v);
    return v<(float)i?nbt_i32((uint32_t)i-1u):i;
}
int32_t nbt_floorDouble(double v) {
    int32_t i=nbt_javaDoubleInt(v);
    return v<(double)i?nbt_i32((uint32_t)i-1u):i;
}
int64_t NBTPrimitive_getLong(const NBTPrimitive *p) {
    if (!p) return 0;
    switch(p->base.type) {
    case 1:return ((const NBTTagByte*)p)->data;case 2:return ((const NBTTagShort*)p)->data;
    case 3:return ((const NBTTagInt*)p)->data;case 4:return ((const NBTTagLong*)p)->data;
    case 5:return nbt_javaDoubleLong(((const NBTTagFloat*)p)->data);
    case 6:return nbt_javaDoubleLong(floor(((const NBTTagDouble*)p)->data));default:return 0;
    }
}
int32_t NBTPrimitive_getInt(const NBTPrimitive *p) {
    if (!p) return 0;
    switch(p->base.type) {
    case 1:return ((const NBTTagByte*)p)->data;case 2:return ((const NBTTagShort*)p)->data;
    case 3:return ((const NBTTagInt*)p)->data;case 4:return nbt_i32((uint32_t)((const NBTTagLong*)p)->data);
    case 5:return nbt_floorFloat(((const NBTTagFloat*)p)->data);
    case 6:return nbt_floorDouble(((const NBTTagDouble*)p)->data);default:return 0;
    }
}
int16_t NBTPrimitive_getShort(const NBTPrimitive *p) { return nbt_i16((uint16_t)NBTPrimitive_getInt(p)); }
int8_t NBTPrimitive_getByte(const NBTPrimitive *p) { return nbt_i8((uint8_t)NBTPrimitive_getInt(p)); }
double NBTPrimitive_getDouble(const NBTPrimitive *p) {
    if (!p) return 0;
    switch(p->base.type) {
    case 1:return ((const NBTTagByte*)p)->data;case 2:return ((const NBTTagShort*)p)->data;
    case 3:return ((const NBTTagInt*)p)->data;case 4:return (double)((const NBTTagLong*)p)->data;
    case 5:return ((const NBTTagFloat*)p)->data;case 6:return ((const NBTTagDouble*)p)->data;default:return 0;
    }
}
float NBTPrimitive_getFloat(const NBTPrimitive *p) {
    if (!p) return 0;
    switch(p->base.type) {
    case 1:return ((const NBTTagByte*)p)->data;case 2:return ((const NBTTagShort*)p)->data;
    case 3:return (float)((const NBTTagInt*)p)->data;case 4:return (float)((const NBTTagLong*)p)->data;
    case 5:return ((const NBTTagFloat*)p)->data;case 6:return (float)((const NBTTagDouble*)p)->data;default:return 0;
    }
}
