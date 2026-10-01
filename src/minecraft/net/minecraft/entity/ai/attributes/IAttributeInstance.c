#include "entity/ai/attributes/ModifiableAttributeInstance.h"
bool IAttributeInstance_isInstance(const MCObject *o) {
  return ModifiableAttributeInstance_isInstance(o);
}
static bool begin(IAttributeInstance *i, MCObjectRootScope *s) {
  MCObjectHeap *h = i ? i->object.heap : NULL;
  if (!IAttributeInstance_isInstance((MCObject *)i) || !i->methods ||
      MCObjectHeap_failed(h) || !MCObjectRootScope_begin(s, h) ||
      !MCObjectRootScope_pin(s, (MCObject *)i)) {
    MCObjectHeap_fail(h);
    return false;
  }
  return true;
}
#define DISPATCH(ret, name, args, call, zero)                                  \
  ret IAttributeInstance_##name args {                                         \
    MCObjectRootScope s = {0};                                                 \
    ret result = zero;                                                         \
    if (begin(i, &s) && i->methods->name)                                      \
      result = i->methods->name call;                                          \
    else                                                                       \
      MCObjectHeap_fail(i ? i->object.heap : NULL);                            \
    MCObjectRootScope_end(&s);                                                 \
    return result;                                                             \
  }
DISPATCH(IAttribute *, getAttribute, (IAttributeInstance * i), (i), NULL)
DISPATCH(double, getBaseValue, (IAttributeInstance * i), (i), 0)
DISPATCH(bool, setBaseValue, (IAttributeInstance * i, double v), (i, v), false)
DISPATCH(AttributeCollection *, getModifiersByOperation,
         (IAttributeInstance * i, int32_t op), (i, op), NULL)
DISPATCH(AttributeCollection *, func_111122_c, (IAttributeInstance * i), (i),
         NULL)
DISPATCH(bool, hasModifier, (IAttributeInstance * i, AttributeModifier *m),
         (i, m), false)
DISPATCH(AttributeModifier *, getModifier,
         (IAttributeInstance * i, NativeJavaUUID *id), (i, id), NULL)
DISPATCH(bool, applyModifier, (IAttributeInstance * i, AttributeModifier *m),
         (i, m), false)
DISPATCH(bool, removeModifier, (IAttributeInstance * i, AttributeModifier *m),
         (i, m), false)
DISPATCH(bool, removeAllModifiers, (IAttributeInstance * i), (i), false)
DISPATCH(double, getAttributeValue, (IAttributeInstance * i), (i), 0)
#undef DISPATCH
