#ifndef C919_NATIVE_NBT_STRING_H
#define C919_NATIVE_NBT_STRING_H
#include "util/MCObjectHeap.h"
/* Java String storage: immutable UTF-16 code units, including NUL and lone
   surrogates. UTF-8 conversion is an explicit boundary, not its representation. */
typedef struct NBTString NBTString;
bool NBTString_isInstance(const MCObject *);
NBTString *NBTString_fromUTF16(MCObjectHeap *heap,const uint16_t *units,size_t length);
NBTString *NBTString_fromASCII(MCObjectHeap *heap,const char *text);
NBTString *NBTString_literalASCII(MCObjectHeap *heap,const char *text);
NBTString *NBTString_fromUTF8(MCObjectHeap *heap,const char *text);
size_t NBTString_length(const NBTString *string);
const uint16_t *NBTString_units(const NBTString *string);
bool NBTString_equals(const NBTString *a,const NBTString *b);
bool NBTString_equalsASCII(const NBTString *string,const char *text);
int32_t NBTString_hashCode(const NBTString *string);
bool NBTString_toUTF8(const NBTString *string,char *output,size_t capacity);
#endif
