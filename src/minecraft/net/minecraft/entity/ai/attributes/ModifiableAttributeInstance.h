#ifndef C919_SOURCE_MODIFIABLE_ATTRIBUTE_INSTANCE_H
#define C919_SOURCE_MODIFIABLE_ATTRIBUTE_INSTANCE_H
#include "entity/ai/attributes/BaseAttributeMap.h"
typedef struct ModifiableAttributeInstance {
    IAttributeInstance instance;
    BaseAttributeMap *attributeMap;
    IAttribute *genericAttribute;
    AttributeCollection *mapByOperation[3];
    AttributeNativeMap *mapByName,*mapByUUID;
    double baseValue,cachedValue;
    bool needsUpdate;
} ModifiableAttributeInstance;
ModifiableAttributeInstance *ModifiableAttributeInstance_new(MCObjectHeap *,BaseAttributeMap *,IAttribute *);
bool ModifiableAttributeInstance_isInstance(const MCObject *);
IAttribute *ModifiableAttributeInstance_getAttribute(ModifiableAttributeInstance *);
double ModifiableAttributeInstance_getBaseValue(ModifiableAttributeInstance *);
bool ModifiableAttributeInstance_setBaseValue(ModifiableAttributeInstance *,double);
AttributeCollection *ModifiableAttributeInstance_getModifiersByOperation(ModifiableAttributeInstance *,int32_t);
AttributeCollection *ModifiableAttributeInstance_func_111122_c(ModifiableAttributeInstance *);
AttributeModifier *ModifiableAttributeInstance_getModifier(ModifiableAttributeInstance *,NativeJavaUUID *);
bool ModifiableAttributeInstance_hasModifier(ModifiableAttributeInstance *,AttributeModifier *);
bool ModifiableAttributeInstance_applyModifier(ModifiableAttributeInstance *,AttributeModifier *);
bool ModifiableAttributeInstance_flagForUpdate(ModifiableAttributeInstance *);
bool ModifiableAttributeInstance_removeModifier(ModifiableAttributeInstance *,AttributeModifier *);
bool ModifiableAttributeInstance_removeAllModifiers(ModifiableAttributeInstance *);
double ModifiableAttributeInstance_getAttributeValue(ModifiableAttributeInstance *);
#endif
