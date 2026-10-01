#include "entity/ai/attributes/ServersideAttributeMap.h"
#include "entity/ai/attributes/RangedAttribute.h"
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
  ServersideAttributeMap *m = (ServersideAttributeMap *)o;
  BaseAttributeMap_trace(&m->base, v, c);
  m->attributeInstanceSet =
      (AttributeCollection *)v((MCObject *)m->attributeInstanceSet, c);
  m->descriptionToAttributeInstanceMap = (AttributeNativeMap *)v(
      (MCObject *)m->descriptionToAttributeInstanceMap, c);
}
static const MCObjectClass klass = {
    "net.minecraft.entity.ai.attributes.ServersideAttributeMap",
    MCObjectHeap_plainClone, trace, NULL};
bool ServersideAttributeMap_isInstance(const MCObject *o) {
  return o && o->klass == &klass &&
         MCObjectHeap_objectSize(o) >= sizeof(ServersideAttributeMap);
}
static IAttributeInstance *create(BaseAttributeMap *m, IAttribute *a) {
  return ServersideAttributeMap_func_180376_c((ServersideAttributeMap *)m, a);
}
static bool update(BaseAttributeMap *m, IAttributeInstance *i) {
  return ServersideAttributeMap_func_180794_a((ServersideAttributeMap *)m, i);
}
static IAttributeInstance *reg(BaseAttributeMap *m, IAttribute *a) {
  return ServersideAttributeMap_registerAttribute((ServersideAttributeMap *)m,
                                                  a);
}
static IAttributeInstance *byname(BaseAttributeMap *m, NBTString *s) {
  return (IAttributeInstance *)
      ServersideAttributeMap_getAttributeInstanceByName(
          (ServersideAttributeMap *)m, s);
}
static const BaseAttributeMapMethods methods = {create, update, reg, byname};
ServersideAttributeMap *ServersideAttributeMap_newWithLowercase(
    MCObjectHeap *h, AttributeLowercase lower, MCObject *context) {
  MCObjectRootScope scope = {0};
  ServersideAttributeMap *m = NULL;
  if (!MCObjectRootScope_begin(&scope, h) ||
      !MCObjectRootScope_pin(&scope, context))
    goto fail;
  m = (ServersideAttributeMap *)MCObjectHeap_alloc(h, sizeof *m, &klass);
  if (!m || !BaseAttributeMap_construct(&m->base, &methods, lower, context))
    goto fail;
  m->attributeInstanceSet =
      AttributeCollection_newSet(h, ATTRIBUTE_KEY_IDENTITY);
  m->descriptionToAttributeInstanceMap =
      AttributeNativeMap_new(h, ATTRIBUTE_KEY_STRING, true);
  if (!m->attributeInstanceSet || !m->descriptionToAttributeInstanceMap)
    goto fail;
  MCObjectRootScope_end(&scope);
  return m;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&scope);
  return NULL;
}
IAttributeInstance *
ServersideAttributeMap_func_180376_c(ServersideAttributeMap *m, IAttribute *a) {
  return m ? (IAttributeInstance *)ModifiableAttributeInstance_new(
                 m->base.object.heap, &m->base, a)
           : NULL;
}
ServersideAttributeMap *ServersideAttributeMap_new(MCObjectHeap *h) {
  return ServersideAttributeMap_newWithLowercase(h, NULL, NULL);
}
static bool valid(ServersideAttributeMap *m) {
  if (!ServersideAttributeMap_isInstance((MCObject *)m) ||
      MCObjectHeap_failed(m ? m->base.object.heap : NULL)) {
    MCObjectHeap_fail(m ? m->base.object.heap : NULL);
    return false;
  }
  return true;
}
ModifiableAttributeInstance *
ServersideAttributeMap_getAttributeInstance(ServersideAttributeMap *m,
                                            IAttribute *a) {
  return valid(m) ? (ModifiableAttributeInstance *)
                        BaseAttributeMap_getAttributeInstance(&m->base, a)
                  : NULL;
}
ModifiableAttributeInstance *
ServersideAttributeMap_getAttributeInstanceByName(ServersideAttributeMap *m,
                                                  NBTString *name) {
  if (!valid(m))
    return NULL;
  ModifiableAttributeInstance *i = (ModifiableAttributeInstance *)
      BaseAttributeMap_getAttributeInstanceByNameBase(&m->base, name);
  if (!i && !MCObjectHeap_failed(m->base.object.heap)) {
    NBTString *lower = BaseAttributeMap_lowercase(&m->base, name);
    if (lower)
      i = (ModifiableAttributeInstance *)AttributeNativeMap_get(
          m->descriptionToAttributeInstanceMap, (MCObject *)lower);
  }
  return i;
}
IAttributeInstance *
ServersideAttributeMap_registerAttribute(ServersideAttributeMap *m,
                                         IAttribute *a) {
  if (!valid(m))
    return NULL;
  IAttributeInstance *i = BaseAttributeMap_registerAttributeBase(&m->base, a);
  if (!i)
    return NULL;
  if (RangedAttribute_isInstance((MCObject *)a)) {
    NBTString *description =
        RangedAttribute_getDescription((RangedAttribute *)a);
    if (description) {
      NBTString *lower = BaseAttributeMap_lowercase(&m->base, description);
      if (!lower ||
          !AttributeNativeMap_put(m->descriptionToAttributeInstanceMap,
                                  (MCObject *)lower, (MCObject *)i))
        return NULL;
    }
  }
  return i;
}
bool ServersideAttributeMap_func_180794_a(ServersideAttributeMap *m,
                                          IAttributeInstance *i) {
  if (!valid(m) || !IAttributeInstance_isInstance((MCObject *)i) ||
      i->object.heap != m->base.object.heap) {
    MCObjectHeap_fail(m ? m->base.object.heap : NULL);
    return false;
  }
  if (++m->base.nativeUpdateDepth > 256) {
    m->base.nativeUpdateDepth--;
    MCObjectHeap_fail(m->base.object.heap);
    return false;
  }
  bool ok = true;
  IAttribute *a = IAttributeInstance_getAttribute(i);
  if (IAttribute_getShouldWatch(a))
    ok = AttributeCollection_add(m->attributeInstanceSet, (MCObject *)i);
  AttributeCollection *children = (AttributeCollection *)AttributeNativeMap_get(
      m->base.field_180377_c, (MCObject *)IAttributeInstance_getAttribute(i));
  if (children)
    for (int32_t j = 0; ok && j < AttributeCollection_size(children); j++) {
      ModifiableAttributeInstance *child =
          ServersideAttributeMap_getAttributeInstance(
              m, (IAttribute *)AttributeCollection_getAt(children, j));
      if (child)
        ok = ModifiableAttributeInstance_flagForUpdate(child);
    }
  m->base.nativeUpdateDepth--;
  return ok && !MCObjectHeap_failed(m->base.object.heap);
}
AttributeCollection *
ServersideAttributeMap_getAttributeInstanceSet(ServersideAttributeMap *m) {
  return valid(m) ? m->attributeInstanceSet : NULL;
}
AttributeCollection *
ServersideAttributeMap_getWatchedAttributes(ServersideAttributeMap *m) {
  if (!valid(m))
    return NULL;
  MCObjectRootScope scope = {0};
  MCObjectHeap *h = m->base.object.heap;
  if (!MCObjectRootScope_begin(&scope, h) ||
      !MCObjectRootScope_pin(&scope, (MCObject *)m))
    goto fail;
  AttributeCollection *set =
      AttributeCollection_newSet(h, ATTRIBUTE_KEY_IDENTITY);
  AttributeCollection *all = BaseAttributeMap_getAllAttributes(&m->base);
  if (!set || !all)
    goto fail;
  for (int32_t j = 0; j < AttributeCollection_size(all); j++) {
    IAttributeInstance *i =
        (IAttributeInstance *)AttributeCollection_getAt(all, j);
    if (IAttribute_getShouldWatch(IAttributeInstance_getAttribute(i)) &&
        !AttributeCollection_add(set, (MCObject *)i))
      goto fail;
  }
  MCObjectRootScope_end(&scope);
  return set;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&scope);
  return NULL;
}
