#ifndef C919_SOURCE_SERVERSIDE_ATTRIBUTE_MAP_H
#define C919_SOURCE_SERVERSIDE_ATTRIBUTE_MAP_H
#include "entity/ai/attributes/ModifiableAttributeInstance.h"
typedef struct ServersideAttributeMap {
    BaseAttributeMap base;
    AttributeCollection *attributeInstanceSet;
    AttributeNativeMap *descriptionToAttributeInstanceMap;
} ServersideAttributeMap;
ServersideAttributeMap *ServersideAttributeMap_new(MCObjectHeap *);
ServersideAttributeMap *ServersideAttributeMap_newWithLowercase(MCObjectHeap *,AttributeLowercase,MCObject *);
bool ServersideAttributeMap_isInstance(const MCObject *);
ModifiableAttributeInstance *ServersideAttributeMap_getAttributeInstance(ServersideAttributeMap *,IAttribute *);
ModifiableAttributeInstance *ServersideAttributeMap_getAttributeInstanceByName(ServersideAttributeMap *,NBTString *);
IAttributeInstance *ServersideAttributeMap_registerAttribute(ServersideAttributeMap *,IAttribute *);
bool ServersideAttributeMap_func_180794_a(ServersideAttributeMap *,IAttributeInstance *);
IAttributeInstance *ServersideAttributeMap_func_180376_c(ServersideAttributeMap *,IAttribute *);
AttributeCollection *ServersideAttributeMap_getAttributeInstanceSet(ServersideAttributeMap *);
AttributeCollection *ServersideAttributeMap_getWatchedAttributes(ServersideAttributeMap *);
#endif
