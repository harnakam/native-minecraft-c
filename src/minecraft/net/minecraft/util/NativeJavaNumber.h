#ifndef C919_NATIVE_JAVA_NUMBER_H
#define C919_NATIVE_JAVA_NUMBER_H
#include "nbt/NBTString.h"
typedef enum {NATIVE_NUMBER_OK,NATIVE_NUMBER_FORMAT,NATIVE_NUMBER_NULL_POINTER,NATIVE_NUMBER_FAILURE} NativeJavaNumberResult;
/* Independent Java8 numeric-API boundary. FORMAT is the caught original
   NumberFormatException; NULL_POINTER/failure are not swallowed by Value.
   No caller-visible output replacement on unsuccessful parsing. */
bool NativeJavaNumber_parseBoolean(const NBTString *,bool *);
NativeJavaNumberResult NativeJavaNumber_parseInt(const NBTString *,int32_t *);
NativeJavaNumberResult NativeJavaNumber_parseDouble(const NBTString *,double *);
#endif
