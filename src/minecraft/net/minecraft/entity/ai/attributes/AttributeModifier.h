#ifndef C919_SOURCE_ATTRIBUTE_MODIFIER_H
#define C919_SOURCE_ATTRIBUTE_MODIFIER_H
#include "util/NativeJavaUUID.h"
typedef struct AttributeModifier {
    MCObject object;
    double amount;
    int32_t operation;
    NBTString *name;
    NativeJavaUUID *id;
    bool isSaved;
} AttributeModifier;
typedef struct {
    NativeJavaUUID *(*threadLocalRandomUuid)(MCObject *);
    NBTString *(*formatToString)(MCObject *,AttributeModifier *);
} AttributeModifierDependencies;
AttributeModifier *AttributeModifier_new(MCObjectHeap *,NativeJavaUUID *,NBTString *,double,int32_t);
AttributeModifier *AttributeModifier_newRandom(MCObjectHeap *,NBTString *,double,int32_t,const AttributeModifierDependencies *,MCObject *);
bool AttributeModifier_isInstance(const MCObject *);
NativeJavaUUID *AttributeModifier_getID(AttributeModifier *);
NBTString *AttributeModifier_getName(AttributeModifier *);
int32_t AttributeModifier_getOperation(AttributeModifier *);
double AttributeModifier_getAmount(AttributeModifier *);
bool AttributeModifier_isSaved(AttributeModifier *);
AttributeModifier *AttributeModifier_setSaved(AttributeModifier *,bool);
bool AttributeModifier_equals(AttributeModifier *,AttributeModifier *);
int32_t AttributeModifier_hashCode(AttributeModifier *);
NBTString *AttributeModifier_toString(AttributeModifier *,const AttributeModifierDependencies *,MCObject *);
#endif
