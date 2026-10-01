#ifndef C919_ATTRIBUTE_INTERNAL_H
#define C919_ATTRIBUTE_INTERNAL_H
#include "entity/ai/attributes/AttributeModifier.h"
/* Source SharedMonsterAttributes catches constructor validation Exception,
   but cannot turn a native allocation/lifetime failure into success. */
AttributeModifier *AttributeModifier_newCaught(MCObjectHeap *,NativeJavaUUID *,NBTString *,double,int32_t,bool);
#endif
