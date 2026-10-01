#ifndef C919_SOURCE_RANGED_ATTRIBUTE_H
#define C919_SOURCE_RANGED_ATTRIBUTE_H
#include "entity/ai/attributes/BaseAttribute.h"
typedef struct RangedAttribute {
    BaseAttribute base;
    double minimumValue,maximumValue;
    NBTString *description;
} RangedAttribute;
RangedAttribute *RangedAttribute_new(MCObjectHeap *,IAttribute *,NBTString *,double,double,double);
bool RangedAttribute_isInstance(const MCObject *);
RangedAttribute *RangedAttribute_setDescription(RangedAttribute *,NBTString *);
NBTString *RangedAttribute_getDescription(RangedAttribute *);
double RangedAttribute_clampValue(RangedAttribute *,double);
#endif
