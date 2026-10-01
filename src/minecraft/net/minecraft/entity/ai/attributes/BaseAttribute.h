#ifndef C919_SOURCE_BASE_ATTRIBUTE_H
#define C919_SOURCE_BASE_ATTRIBUTE_H
#include "entity/ai/attributes/IAttribute.h"
typedef struct BaseAttribute {
    IAttribute attribute;
    IAttribute *field_180373_a;
    NBTString *unlocalizedName;
    double defaultValue;
    bool shouldWatch;
} BaseAttribute;
bool BaseAttribute_construct(BaseAttribute *,IAttribute *,NBTString *,double,const IAttributeMethods *);
void BaseAttribute_trace(BaseAttribute *,MCObjectVisitor,void *);
NBTString *BaseAttribute_getAttributeUnlocalizedName(BaseAttribute *);
double BaseAttribute_getDefaultValue(BaseAttribute *);
bool BaseAttribute_getShouldWatch(BaseAttribute *);
BaseAttribute *BaseAttribute_setShouldWatch(BaseAttribute *,bool);
IAttribute *BaseAttribute_func_180372_d(BaseAttribute *);
int32_t BaseAttribute_hashCode(BaseAttribute *);
bool BaseAttribute_equals(BaseAttribute *,IAttribute *);
#endif
