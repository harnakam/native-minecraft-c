#ifndef C919_NATIVE_ATTRIBUTE_COLLECTIONS_H
#define C919_NATIVE_ATTRIBUTE_COLLECTIONS_H
#include "entity/ai/attributes/AttributeModifier.h"
/* Native Java collection dependencies, not a java.util class translation.
   Sets use Java hash spreading/bucket iteration; lower maps preserve insertion
   order. Live collection views retain their backing map; mutation is visible.
   No caller may retain an iteration index across mutation. Resource bounds
   are 4096 live entries and parent/update depth 256. Treeified collision bins
   are an unclosed JDK collection dependency and fail rather than approximating
   their floating-point iteration order. ASCII lowercase is a native default
   locale view; callers supply the actual locale converter when needed. */
typedef struct AttributeCollection AttributeCollection;
typedef struct AttributeNativeMap AttributeNativeMap;
typedef enum { ATTRIBUTE_KEY_IDENTITY,ATTRIBUTE_KEY_ATTRIBUTE,ATTRIBUTE_KEY_STRING,
               ATTRIBUTE_KEY_UUID,ATTRIBUTE_KEY_MODIFIER } AttributeKeyKind;
AttributeCollection *AttributeCollection_newSet(MCObjectHeap *,AttributeKeyKind);
AttributeCollection *AttributeCollection_copySet(AttributeCollection *,AttributeKeyKind);
int32_t AttributeCollection_size(const AttributeCollection *);
MCObject *AttributeCollection_getAt(AttributeCollection *,int32_t);
bool AttributeCollection_contains(AttributeCollection *,MCObject *);
bool AttributeCollection_add(AttributeCollection *,MCObject *);
bool AttributeCollection_remove(AttributeCollection *,MCObject *);
bool AttributeCollection_clear(AttributeCollection *);
bool AttributeCollection_addAll(AttributeCollection *,AttributeCollection *);
typedef struct AttributeModifierMultimap AttributeModifierMultimap;
AttributeModifierMultimap *AttributeModifierMultimap_new(MCObjectHeap *);
bool AttributeModifierMultimap_put(AttributeModifierMultimap *,NBTString *,AttributeModifier *);
int32_t AttributeModifierMultimap_size(const AttributeModifierMultimap *);
NBTString *AttributeModifierMultimap_keyAt(AttributeModifierMultimap *,int32_t);
AttributeModifier *AttributeModifierMultimap_valueAt(AttributeModifierMultimap *,int32_t);
/* Internal map API is public only as a native dependency of translated fields. */
AttributeNativeMap *AttributeNativeMap_new(MCObjectHeap *,AttributeKeyKind,bool);
MCObject *AttributeNativeMap_get(AttributeNativeMap *,MCObject *);
bool AttributeNativeMap_containsKey(AttributeNativeMap *,MCObject *);
bool AttributeNativeMap_put(AttributeNativeMap *,MCObject *,MCObject *);
bool AttributeNativeMap_remove(AttributeNativeMap *,MCObject *);
AttributeCollection *AttributeNativeMap_values(AttributeNativeMap *);
bool AttributeNativeMap_clear(AttributeNativeMap *);
#endif
