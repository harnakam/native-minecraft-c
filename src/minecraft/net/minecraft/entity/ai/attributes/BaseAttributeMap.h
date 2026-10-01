#ifndef C919_SOURCE_BASE_ATTRIBUTE_MAP_H
#define C919_SOURCE_BASE_ATTRIBUTE_MAP_H
#include "entity/ai/attributes/IAttributeInstance.h"
typedef struct BaseAttributeMap BaseAttributeMap;
/* Non-ASCII default-locale String.toLowerCase is a required native dependency.
   Built-in ASCII attribute names are closed without that dependency. */
typedef NBTString *(*AttributeLowercase)(MCObject *,NBTString *);
typedef struct {
    IAttributeInstance *(*func_180376_c)(BaseAttributeMap *,IAttribute *);
    bool (*func_180794_a)(BaseAttributeMap *,IAttributeInstance *);
    IAttributeInstance *(*registerAttribute)(BaseAttributeMap *,IAttribute *);
    IAttributeInstance *(*getAttributeInstanceByName)(BaseAttributeMap *,NBTString *);
} BaseAttributeMapMethods;
struct BaseAttributeMap {
    MCObject object;
    AttributeNativeMap *attributes,*attributesByName,*field_180377_c;
    const BaseAttributeMapMethods *methods;
    AttributeLowercase lowercase;
    MCObject *lowercaseContext;
    unsigned nativeUpdateDepth;
};
bool BaseAttributeMap_construct(BaseAttributeMap *,const BaseAttributeMapMethods *,AttributeLowercase,MCObject *);
void BaseAttributeMap_trace(BaseAttributeMap *,MCObjectVisitor,void *);
bool BaseAttributeMap_isInstance(const MCObject *);
IAttributeInstance *BaseAttributeMap_getAttributeInstance(BaseAttributeMap *,IAttribute *);
IAttributeInstance *BaseAttributeMap_getAttributeInstanceByName(BaseAttributeMap *,NBTString *);
IAttributeInstance *BaseAttributeMap_getAttributeInstanceByNameBase(BaseAttributeMap *,NBTString *);
IAttributeInstance *BaseAttributeMap_registerAttribute(BaseAttributeMap *,IAttribute *);
IAttributeInstance *BaseAttributeMap_registerAttributeBase(BaseAttributeMap *,IAttribute *);
AttributeCollection *BaseAttributeMap_getAllAttributes(BaseAttributeMap *);
bool BaseAttributeMap_func_180794_a(BaseAttributeMap *,IAttributeInstance *);
bool BaseAttributeMap_func_180794_aBase(BaseAttributeMap *,IAttributeInstance *);
IAttributeInstance *BaseAttributeMap_func_180376_c(BaseAttributeMap *,IAttribute *);
bool BaseAttributeMap_removeAttributeModifiers(BaseAttributeMap *,AttributeModifierMultimap *);
bool BaseAttributeMap_applyAttributeModifiers(BaseAttributeMap *,AttributeModifierMultimap *);
NBTString *BaseAttributeMap_lowercase(BaseAttributeMap *,NBTString *);
#endif
