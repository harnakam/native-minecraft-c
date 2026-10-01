#ifndef C919_NATIVE_JAVA_STRING_H
#define C919_NATIVE_JAVA_STRING_H
#include "nbt/NBTString.h"
/* Native Java8 UTF-16 comparison/concatenation dependency. No host locale,
   Unicode replacement or UTF-8 normalization is applied. */
uint16_t NativeJavaString_upper(uint16_t);
uint16_t NativeJavaString_lower(uint16_t);
bool NativeJavaString_equalsIgnoreCase(const NBTString *,const NBTString *,bool *);
NBTString *NativeJavaString_concat(MCObjectHeap *,const char *prefix,NBTString *value,const char *suffix);
#endif
