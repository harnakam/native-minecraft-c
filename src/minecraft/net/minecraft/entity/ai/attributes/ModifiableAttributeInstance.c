#include "entity/ai/attributes/ModifiableAttributeInstance.h"
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
  ModifiableAttributeInstance *i = (ModifiableAttributeInstance *)o;
  i->attributeMap = (BaseAttributeMap *)v((MCObject *)i->attributeMap, c);
  i->genericAttribute = (IAttribute *)v((MCObject *)i->genericAttribute, c);
  for (int op = 0; op < 3; op++)
    i->mapByOperation[op] =
        (AttributeCollection *)v((MCObject *)i->mapByOperation[op], c);
  i->mapByName = (AttributeNativeMap *)v((MCObject *)i->mapByName, c);
  i->mapByUUID = (AttributeNativeMap *)v((MCObject *)i->mapByUUID, c);
}
static const MCObjectClass klass = {
    "net.minecraft.entity.ai.attributes.ModifiableAttributeInstance",
    MCObjectHeap_plainClone, trace, NULL};
bool ModifiableAttributeInstance_isInstance(const MCObject *o) {
  return o && o->klass == &klass &&
         MCObjectHeap_objectSize(o) >= sizeof(ModifiableAttributeInstance);
}
static bool valid(ModifiableAttributeInstance *i) {
  if (!ModifiableAttributeInstance_isInstance((MCObject *)i) ||
      MCObjectHeap_failed(i ? i->instance.object.heap : NULL)) {
    MCObjectHeap_fail(i ? i->instance.object.heap : NULL);
    return false;
  }
  return true;
}
#define WRAP(ret, name, args, call)                                            \
  static ret wrap_##name args {                                                \
    return ModifiableAttributeInstance_##name call;                            \
  }
WRAP(IAttribute *, getAttribute, (IAttributeInstance * i),
     ((ModifiableAttributeInstance *)i))
WRAP(double, getBaseValue, (IAttributeInstance * i),
     ((ModifiableAttributeInstance *)i))
WRAP(bool, setBaseValue, (IAttributeInstance * i, double v),
     ((ModifiableAttributeInstance *)i, v))
WRAP(AttributeCollection *, getModifiersByOperation,
     (IAttributeInstance * i, int32_t op),
     ((ModifiableAttributeInstance *)i, op))
WRAP(AttributeCollection *, func_111122_c, (IAttributeInstance * i),
     ((ModifiableAttributeInstance *)i))
WRAP(bool, hasModifier, (IAttributeInstance * i, AttributeModifier *m),
     ((ModifiableAttributeInstance *)i, m))
WRAP(AttributeModifier *, getModifier,
     (IAttributeInstance * i, NativeJavaUUID *id),
     ((ModifiableAttributeInstance *)i, id))
WRAP(bool, applyModifier, (IAttributeInstance * i, AttributeModifier *m),
     ((ModifiableAttributeInstance *)i, m))
WRAP(bool, removeModifier, (IAttributeInstance * i, AttributeModifier *m),
     ((ModifiableAttributeInstance *)i, m))
WRAP(bool, removeAllModifiers, (IAttributeInstance * i),
     ((ModifiableAttributeInstance *)i))
WRAP(double, getAttributeValue, (IAttributeInstance * i),
     ((ModifiableAttributeInstance *)i))
#undef WRAP
static const IAttributeInstanceMethods methods = {
    wrap_getAttribute,     wrap_getBaseValue,
    wrap_setBaseValue,     wrap_getModifiersByOperation,
    wrap_func_111122_c,    wrap_hasModifier,
    wrap_getModifier,      wrap_applyModifier,
    wrap_removeModifier,   wrap_removeAllModifiers,
    wrap_getAttributeValue};
ModifiableAttributeInstance *
ModifiableAttributeInstance_new(MCObjectHeap *h, BaseAttributeMap *map,
                                IAttribute *attribute) {
  MCObjectRootScope s = {0};
  ModifiableAttributeInstance *i = NULL;
  if (!MCObjectRootScope_begin(&s, h) ||
      !MCObjectRootScope_pin(&s, (MCObject *)map) ||
      !MCObjectRootScope_pin(&s, (MCObject *)attribute))
    goto fail;
  if ((map && !BaseAttributeMap_isInstance((MCObject *)map)) ||
      !IAttribute_isInstance((MCObject *)attribute))
    goto fail;
  i = (ModifiableAttributeInstance *)MCObjectHeap_alloc(h, sizeof *i, &klass);
  if (!i)
    goto fail;
  i->instance.methods = &methods;
  i->needsUpdate = true;
  i->mapByName = AttributeNativeMap_new(h, ATTRIBUTE_KEY_STRING, false);
  i->mapByUUID = AttributeNativeMap_new(h, ATTRIBUTE_KEY_UUID, false);
  if (!i->mapByName || !i->mapByUUID)
    goto fail;
  i->attributeMap = map;
  i->genericAttribute = attribute;
  i->baseValue = IAttribute_getDefaultValue(attribute);
  if (MCObjectHeap_failed(h))
    goto fail;
  for (int op = 0; op < 3; op++)
    if (!(i->mapByOperation[op] =
              AttributeCollection_newSet(h, ATTRIBUTE_KEY_MODIFIER)))
      goto fail;
  MCObjectRootScope_end(&s);
  return i;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&s);
  return NULL;
}
IAttribute *
ModifiableAttributeInstance_getAttribute(ModifiableAttributeInstance *i) {
  return valid(i) ? i->genericAttribute : NULL;
}
double
ModifiableAttributeInstance_getBaseValue(ModifiableAttributeInstance *i) {
  return valid(i) ? i->baseValue : 0;
}
bool ModifiableAttributeInstance_setBaseValue(ModifiableAttributeInstance *i,
                                              double v) {
  if (!valid(i))
    return false;
  if (v != ModifiableAttributeInstance_getBaseValue(i)) {
    i->baseValue = v;
    MCObjectHeap_touch(i->instance.object.heap);
    return ModifiableAttributeInstance_flagForUpdate(i);
  }
  return true;
}
AttributeCollection *ModifiableAttributeInstance_getModifiersByOperation(
    ModifiableAttributeInstance *i, int32_t op) {
  return valid(i) && op >= 0 && op < 3 ? i->mapByOperation[op] : NULL;
}
AttributeCollection *
ModifiableAttributeInstance_func_111122_c(ModifiableAttributeInstance *i) {
  MCObjectRootScope s = {0};
  MCObjectHeap *h = i ? i->instance.object.heap : NULL;
  if (!valid(i) || !MCObjectRootScope_begin(&s, h) ||
      !MCObjectRootScope_pin(&s, (MCObject *)i))
    goto fail;
  AttributeCollection *set =
      AttributeCollection_newSet(h, ATTRIBUTE_KEY_MODIFIER);
  if (!set)
    goto fail;
  for (int op = 0; op < 3; op++)
    if (!AttributeCollection_addAll(
            set, ModifiableAttributeInstance_getModifiersByOperation(i, op)))
      goto fail;
  MCObjectRootScope_end(&s);
  return set;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&s);
  return NULL;
}
AttributeModifier *
ModifiableAttributeInstance_getModifier(ModifiableAttributeInstance *i,
                                        NativeJavaUUID *id) {
  return valid(i) ? (AttributeModifier *)AttributeNativeMap_get(i->mapByUUID,
                                                                (MCObject *)id)
                  : NULL;
}
bool ModifiableAttributeInstance_hasModifier(ModifiableAttributeInstance *i,
                                             AttributeModifier *m) {
  if (!valid(i) || !AttributeModifier_isInstance((MCObject *)m)) {
    MCObjectHeap_fail(i ? i->instance.object.heap : NULL);
    return false;
  }
  return AttributeNativeMap_get(i->mapByUUID,
                                (MCObject *)AttributeModifier_getID(m)) != NULL;
}
bool ModifiableAttributeInstance_flagForUpdate(ModifiableAttributeInstance *i) {
  if (!valid(i))
    return false;
  i->needsUpdate = true;
  MCObjectHeap_touch(i->instance.object.heap);
  return BaseAttributeMap_func_180794_a(i->attributeMap,
                                        (IAttributeInstance *)i);
}
static bool begin(ModifiableAttributeInstance *i, AttributeModifier *m,
                  MCObjectRootScope *s) {
  if (!valid(i) || !AttributeModifier_isInstance((MCObject *)m)) {
    MCObjectHeap_fail(i ? i->instance.object.heap : NULL);
    return false;
  }
  return MCObjectRootScope_begin(s, i->instance.object.heap) &&
         MCObjectRootScope_pin(s, (MCObject *)i) &&
         MCObjectRootScope_pin(s, (MCObject *)m);
}
bool ModifiableAttributeInstance_applyModifier(ModifiableAttributeInstance *i,
                                               AttributeModifier *m) {
  MCObjectRootScope scope = {0};
  if (!begin(i, m, &scope))
    goto fail;
  NativeJavaUUID *id = AttributeModifier_getID(m);
  if (ModifiableAttributeInstance_getModifier(i, id))
    goto fail;
  NBTString *name = AttributeModifier_getName(m);
  AttributeCollection *set = (AttributeCollection *)AttributeNativeMap_get(
      i->mapByName, (MCObject *)name);
  if (!set) {
    set = AttributeCollection_newSet(i->instance.object.heap,
                                     ATTRIBUTE_KEY_MODIFIER);
    if (!set || !AttributeNativeMap_put(i->mapByName, (MCObject *)name,
                                        (MCObject *)set))
      goto fail;
  }
  if (!AttributeCollection_add(
          ModifiableAttributeInstance_getModifiersByOperation(
              i, AttributeModifier_getOperation(m)),
          (MCObject *)m) ||
      !AttributeCollection_add(set, (MCObject *)m) ||
      !AttributeNativeMap_put(i->mapByUUID, (MCObject *)id, (MCObject *)m) ||
      !ModifiableAttributeInstance_flagForUpdate(i))
    goto fail;
  MCObjectRootScope_end(&scope);
  return true;
fail:
  MCObjectHeap_fail(i ? i->instance.object.heap : NULL);
  MCObjectRootScope_end(&scope);
  return false;
}
bool ModifiableAttributeInstance_removeModifier(ModifiableAttributeInstance *i,
                                                AttributeModifier *m) {
  MCObjectRootScope scope = {0};
  if (!begin(i, m, &scope))
    goto fail;
  for (int op = 0; op < 3; op++)
    if (!AttributeCollection_remove(i->mapByOperation[op], (MCObject *)m))
      goto fail;
  NBTString *name = AttributeModifier_getName(m);
  AttributeCollection *set = (AttributeCollection *)AttributeNativeMap_get(
      i->mapByName, (MCObject *)name);
  if (set) {
    if (!AttributeCollection_remove(set, (MCObject *)m))
      goto fail;
    if (AttributeCollection_size(set) == 0 &&
        !AttributeNativeMap_remove(i->mapByName, (MCObject *)name))
      goto fail;
  }
  if (!AttributeNativeMap_remove(i->mapByUUID,
                                 (MCObject *)AttributeModifier_getID(m)) ||
      !ModifiableAttributeInstance_flagForUpdate(i))
    goto fail;
  MCObjectRootScope_end(&scope);
  return true;
fail:
  MCObjectHeap_fail(i ? i->instance.object.heap : NULL);
  MCObjectRootScope_end(&scope);
  return false;
}
bool ModifiableAttributeInstance_removeAllModifiers(
    ModifiableAttributeInstance *i) {
  MCObjectRootScope scope = {0};
  MCObjectHeap *h = i ? i->instance.object.heap : NULL;
  if (!valid(i) || !MCObjectRootScope_begin(&scope, h) ||
      !MCObjectRootScope_pin(&scope, (MCObject *)i))
    goto fail;
  AttributeCollection *set = ModifiableAttributeInstance_func_111122_c(i);
  if (!set)
    goto fail;
  /* Original takes a list snapshot. This fresh union is already detached
     from operation storage and has the same iteration sequence. */
  for (int32_t j = 0; j < AttributeCollection_size(set); j++)
    if (!ModifiableAttributeInstance_removeModifier(
            i, (AttributeModifier *)AttributeCollection_getAt(set, j)))
      goto fail;
  MCObjectRootScope_end(&scope);
  return true;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&scope);
  return false;
}
static AttributeCollection *inherited(ModifiableAttributeInstance *i, int op) {
  AttributeCollection *set = AttributeCollection_copySet(
      ModifiableAttributeInstance_getModifiersByOperation(i, op),
      ATTRIBUTE_KEY_MODIFIER);
  if (!set)
    return NULL;
  unsigned depth = 0;
  for (IAttribute *a = IAttribute_func_180372_d(i->genericAttribute); a;
       a = IAttribute_func_180372_d(a)) {
    if (++depth > 256) {
      MCObjectHeap_fail(i->instance.object.heap);
      return NULL;
    }
    IAttributeInstance *parent =
        BaseAttributeMap_getAttributeInstance(i->attributeMap, a);
    if (parent &&
        !AttributeCollection_addAll(
            set, IAttributeInstance_getModifiersByOperation(parent, op)))
      return NULL;
  }
  return MCObjectHeap_failed(i->instance.object.heap) ? NULL : set;
}
static bool compute(ModifiableAttributeInstance *i, double *value) {
  double d0 = ModifiableAttributeInstance_getBaseValue(i);
  AttributeCollection *set = inherited(i, 0);
  if (!set)
    return false;
  for (int32_t j = 0; j < AttributeCollection_size(set); j++)
    d0 += AttributeModifier_getAmount(
        (AttributeModifier *)AttributeCollection_getAt(set, j));
  double d1 = d0;
  set = inherited(i, 1);
  if (!set)
    return false;
  for (int32_t j = 0; j < AttributeCollection_size(set); j++)
    d1 += d0 * AttributeModifier_getAmount(
                   (AttributeModifier *)AttributeCollection_getAt(set, j));
  set = inherited(i, 2);
  if (!set)
    return false;
  for (int32_t j = 0; j < AttributeCollection_size(set); j++)
    d1 *= 1.0 + AttributeModifier_getAmount(
                    (AttributeModifier *)AttributeCollection_getAt(set, j));
  *value = IAttribute_clampValue(i->genericAttribute, d1);
  return !MCObjectHeap_failed(i->instance.object.heap);
}
double
ModifiableAttributeInstance_getAttributeValue(ModifiableAttributeInstance *i) {
  MCObjectRootScope scope = {0};
  MCObjectHeap *h = i ? i->instance.object.heap : NULL;
  double result = 0;
  if (!valid(i) || !MCObjectRootScope_begin(&scope, h) ||
      !MCObjectRootScope_pin(&scope, (MCObject *)i))
    goto fail;
  if (i->needsUpdate) {
    double value;
    if (!compute(i, &value))
      goto fail;
    i->cachedValue = value;
    i->needsUpdate = false;
    MCObjectHeap_touch(h);
  }
  result = i->cachedValue;
  MCObjectRootScope_end(&scope);
  return result;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&scope);
  return 0;
}
