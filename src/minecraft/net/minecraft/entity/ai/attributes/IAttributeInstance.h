#ifndef C919_SOURCE_I_ATTRIBUTE_INSTANCE_H
#define C919_SOURCE_I_ATTRIBUTE_INSTANCE_H
#include "entity/ai/attributes/IAttribute.h"
#include "entity/ai/attributes/AttributeCollections.h"
typedef struct IAttributeInstance IAttributeInstance;
typedef struct {
    IAttribute *(*getAttribute)(IAttributeInstance *);
    double (*getBaseValue)(IAttributeInstance *);
    bool (*setBaseValue)(IAttributeInstance *,double);
    AttributeCollection *(*getModifiersByOperation)(IAttributeInstance *,int32_t);
    AttributeCollection *(*func_111122_c)(IAttributeInstance *);
    bool (*hasModifier)(IAttributeInstance *,AttributeModifier *);
    AttributeModifier *(*getModifier)(IAttributeInstance *,NativeJavaUUID *);
    bool (*applyModifier)(IAttributeInstance *,AttributeModifier *);
    bool (*removeModifier)(IAttributeInstance *,AttributeModifier *);
    bool (*removeAllModifiers)(IAttributeInstance *);
    double (*getAttributeValue)(IAttributeInstance *);
} IAttributeInstanceMethods;
struct IAttributeInstance { MCObject object; const IAttributeInstanceMethods *methods; };
bool IAttributeInstance_isInstance(const MCObject *);
IAttribute *IAttributeInstance_getAttribute(IAttributeInstance *);
double IAttributeInstance_getBaseValue(IAttributeInstance *);
bool IAttributeInstance_setBaseValue(IAttributeInstance *,double);
AttributeCollection *IAttributeInstance_getModifiersByOperation(IAttributeInstance *,int32_t);
AttributeCollection *IAttributeInstance_func_111122_c(IAttributeInstance *);
bool IAttributeInstance_hasModifier(IAttributeInstance *,AttributeModifier *);
AttributeModifier *IAttributeInstance_getModifier(IAttributeInstance *,NativeJavaUUID *);
bool IAttributeInstance_applyModifier(IAttributeInstance *,AttributeModifier *);
bool IAttributeInstance_removeModifier(IAttributeInstance *,AttributeModifier *);
bool IAttributeInstance_removeAllModifiers(IAttributeInstance *);
double IAttributeInstance_getAttributeValue(IAttributeInstance *);
#endif
