#include "entity/ai/attributes/BaseAttributeMap.h"
#include "entity/ai/attributes/ServersideAttributeMap.h"
#include <stdlib.h>
bool BaseAttributeMap_isInstance(const MCObject *o) {
  return ServersideAttributeMap_isInstance(o);
}
static bool valid(BaseAttributeMap *m) {
  if (!BaseAttributeMap_isInstance((MCObject *)m) ||
      MCObjectHeap_failed(m ? m->object.heap : NULL)) {
    MCObjectHeap_fail(m ? m->object.heap : NULL);
    return false;
  }
  return true;
}
static bool begin(BaseAttributeMap *m, MCObject *arg, MCObjectRootScope *s) {
  return valid(m) && MCObjectRootScope_begin(s, m->object.heap) &&
         MCObjectRootScope_pin(s, (MCObject *)m) &&
         MCObjectRootScope_pin(s, arg);
}
void BaseAttributeMap_trace(BaseAttributeMap *m, MCObjectVisitor v, void *c) {
  m->attributes = (AttributeNativeMap *)v((MCObject *)m->attributes, c);
  m->attributesByName =
      (AttributeNativeMap *)v((MCObject *)m->attributesByName, c);
  m->field_180377_c = (AttributeNativeMap *)v((MCObject *)m->field_180377_c, c);
  m->lowercaseContext = v(m->lowercaseContext, c);
}
bool BaseAttributeMap_construct(BaseAttributeMap *m,
                                const BaseAttributeMapMethods *methods,
                                AttributeLowercase lowercase,
                                MCObject *context) {
  MCObjectHeap *h = m ? m->object.heap : NULL;
  MCObjectRootScope s = {0};
  if (!m || !methods || MCObjectHeap_objectSize((MCObject *)m) < sizeof *m ||
      !MCObjectRootScope_begin(&s, h) ||
      !MCObjectRootScope_pin(&s, (MCObject *)m) ||
      !MCObjectRootScope_pin(&s, context))
    goto fail;
  m->methods = methods;
  m->lowercase = lowercase;
  m->lowercaseContext = context;
  m->attributes = AttributeNativeMap_new(h, ATTRIBUTE_KEY_ATTRIBUTE, false);
  m->attributesByName = AttributeNativeMap_new(h, ATTRIBUTE_KEY_STRING, true);
  m->field_180377_c = AttributeNativeMap_new(h, ATTRIBUTE_KEY_ATTRIBUTE, false);
  if (!m->attributes || !m->attributesByName || !m->field_180377_c)
    goto fail;
  MCObjectRootScope_end(&s);
  return true;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&s);
  return false;
}
NBTString *BaseAttributeMap_lowercase(BaseAttributeMap *m, NBTString *name) {
  MCObjectRootScope scope = {0};
  NBTString *result = NULL;
  if (!begin(m, (MCObject *)name, &scope) ||
      !NBTString_isInstance((MCObject *)name))
    goto fail;
  if (m->lowercase) {
    result = m->lowercase(m->lowercaseContext, name);
    if (!NBTString_isInstance((MCObject *)result) ||
        ((MCObject *)result)->heap != m->object.heap)
      goto fail;
  } else {
    const uint16_t *units = NBTString_units(name);
    size_t count = NBTString_length(name);
    bool changed = false;
    for (size_t i = 0; i < count; i++) {
      if (units[i] > 127)
        goto fail;
      if (units[i] >= 'A' && units[i] <= 'Z')
        changed = true;
    }
    if (!changed)
      result = name;
    else {
      uint16_t *lower = (uint16_t *)malloc(count * sizeof *lower);
      if (!lower)
        goto fail;
      for (size_t i = 0; i < count; i++)
        lower[i] = units[i] >= 'A' && units[i] <= 'Z'
                       ? (uint16_t)(units[i] + 32)
                       : units[i];
      result = NBTString_fromUTF16(m->object.heap, lower, count);
      free(lower);
      if (!result)
        goto fail;
    }
  }
  MCObjectRootScope_end(&scope);
  return result;
fail:
  MCObjectHeap_fail(m ? m->object.heap : NULL);
  MCObjectRootScope_end(&scope);
  return NULL;
}
IAttributeInstance *BaseAttributeMap_getAttributeInstance(BaseAttributeMap *m,
                                                          IAttribute *a) {
  if (!valid(m))
    return NULL;
  return (IAttributeInstance *)AttributeNativeMap_get(m->attributes,
                                                      (MCObject *)a);
}
IAttributeInstance *
BaseAttributeMap_getAttributeInstanceByNameBase(BaseAttributeMap *m,
                                                NBTString *name) {
  MCObjectRootScope s = {0};
  IAttributeInstance *i = NULL;
  if (!begin(m, (MCObject *)name, &s))
    goto fail;
  NBTString *lower = BaseAttributeMap_lowercase(m, name);
  if (!lower)
    goto fail;
  i = (IAttributeInstance *)AttributeNativeMap_get(m->attributesByName,
                                                   (MCObject *)lower);
  MCObjectRootScope_end(&s);
  return i;
fail:
  MCObjectHeap_fail(m ? m->object.heap : NULL);
  MCObjectRootScope_end(&s);
  return NULL;
}
IAttributeInstance *
BaseAttributeMap_getAttributeInstanceByName(BaseAttributeMap *m,
                                            NBTString *name) {
  MCObjectRootScope s = {0};
  IAttributeInstance *i = NULL;
  if (begin(m, (MCObject *)name, &s) && m->methods->getAttributeInstanceByName)
    i = m->methods->getAttributeInstanceByName(m, name);
  else
    MCObjectHeap_fail(m ? m->object.heap : NULL);
  MCObjectRootScope_end(&s);
  return i;
}
IAttributeInstance *BaseAttributeMap_registerAttributeBase(BaseAttributeMap *m,
                                                           IAttribute *a) {
  MCObjectRootScope s = {0};
  if (!begin(m, (MCObject *)a, &s) || !IAttribute_isInstance((MCObject *)a))
    goto fail;
  NBTString *name = IAttribute_getAttributeUnlocalizedName(a);
  NBTString *lower = BaseAttributeMap_lowercase(m, name);
  if (!lower)
    goto fail;
  if (AttributeNativeMap_containsKey(m->attributesByName, (MCObject *)lower) ||
      !m->methods->func_180376_c)
    goto fail;
  IAttributeInstance *instance = BaseAttributeMap_func_180376_c(m, a);
  if (!instance || MCObjectHeap_failed(m->object.heap))
    goto fail;
  lower =
      BaseAttributeMap_lowercase(m, IAttribute_getAttributeUnlocalizedName(a));
  if (!lower ||
      !AttributeNativeMap_put(m->attributesByName, (MCObject *)lower,
                              (MCObject *)instance) ||
      !AttributeNativeMap_put(m->attributes, (MCObject *)a,
                              (MCObject *)instance))
    goto fail;
  unsigned depth = 0;
  for (IAttribute *p = IAttribute_func_180372_d(a); p;
       p = IAttribute_func_180372_d(p)) {
    if (++depth > 256)
      goto fail;
    AttributeCollection *desc = (AttributeCollection *)AttributeNativeMap_get(
        m->field_180377_c, (MCObject *)p);
    if (!desc) {
      desc =
          AttributeCollection_newSet(m->object.heap, ATTRIBUTE_KEY_ATTRIBUTE);
      if (!desc || !AttributeNativeMap_put(m->field_180377_c, (MCObject *)p,
                                           (MCObject *)desc))
        goto fail;
    }
    if (!AttributeCollection_add(desc, (MCObject *)a))
      goto fail;
  }
  if (MCObjectHeap_failed(m->object.heap))
    goto fail;
  MCObjectRootScope_end(&s);
  return instance;
fail:
  MCObjectHeap_fail(m ? m->object.heap : NULL);
  MCObjectRootScope_end(&s);
  return NULL;
}
IAttributeInstance *BaseAttributeMap_registerAttribute(BaseAttributeMap *m,
                                                       IAttribute *a) {
  MCObjectRootScope s = {0};
  IAttributeInstance *i = NULL;
  if (begin(m, (MCObject *)a, &s) && m->methods->registerAttribute)
    i = m->methods->registerAttribute(m, a);
  else
    MCObjectHeap_fail(m ? m->object.heap : NULL);
  MCObjectRootScope_end(&s);
  return i;
}
IAttributeInstance *BaseAttributeMap_func_180376_c(BaseAttributeMap *m,
                                                   IAttribute *a) {
  MCObjectRootScope s = {0};
  IAttributeInstance *i = NULL;
  if (begin(m, (MCObject *)a, &s) && m->methods->func_180376_c)
    i = m->methods->func_180376_c(m, a);
  else
    MCObjectHeap_fail(m ? m->object.heap : NULL);
  MCObjectRootScope_end(&s);
  return i;
}
AttributeCollection *BaseAttributeMap_getAllAttributes(BaseAttributeMap *m) {
  return valid(m) ? AttributeNativeMap_values(m->attributesByName) : NULL;
}
bool BaseAttributeMap_func_180794_aBase(BaseAttributeMap *m,
                                        IAttributeInstance *i) {
  (void)i;
  return valid(m);
}
bool BaseAttributeMap_func_180794_a(BaseAttributeMap *m,
                                    IAttributeInstance *i) {
  MCObjectRootScope s = {0};
  bool ok = false;
  if (begin(m, (MCObject *)i, &s) && m->methods->func_180794_a)
    ok = m->methods->func_180794_a(m, i);
  if (!ok)
    MCObjectHeap_fail(m ? m->object.heap : NULL);
  MCObjectRootScope_end(&s);
  return ok;
}
static bool modifiers(BaseAttributeMap *m, AttributeModifierMultimap *entries,
                      bool apply) {
  MCObjectRootScope s = {0};
  if (!begin(m, (MCObject *)entries, &s) || !entries)
    goto fail;
  for (int32_t j = 0; j < AttributeModifierMultimap_size(entries); j++) {
    IAttributeInstance *i = BaseAttributeMap_getAttributeInstanceByName(
        m, AttributeModifierMultimap_keyAt(entries, j));
    if (MCObjectHeap_failed(m->object.heap))
      goto fail;
    if (i) {
      AttributeModifier *value = AttributeModifierMultimap_valueAt(entries, j);
      if (!IAttributeInstance_removeModifier(i, value) ||
          (apply && !IAttributeInstance_applyModifier(i, value)))
        goto fail;
    }
  }
  MCObjectRootScope_end(&s);
  return true;
fail:
  MCObjectHeap_fail(m ? m->object.heap : NULL);
  MCObjectRootScope_end(&s);
  return false;
}
bool BaseAttributeMap_removeAttributeModifiers(BaseAttributeMap *m,
                                               AttributeModifierMultimap *e) {
  return modifiers(m, e, false);
}
bool BaseAttributeMap_applyAttributeModifiers(BaseAttributeMap *m,
                                              AttributeModifierMultimap *e) {
  return modifiers(m, e, true);
}
