#include "entity/ai/attributes/RangedAttribute.h"
bool IAttribute_isInstance(const MCObject *o) {
  return RangedAttribute_isInstance(o);
}
static bool begin(IAttribute *a, MCObjectRootScope *s) {
  MCObjectHeap *h = a ? a->object.heap : NULL;
  if (!IAttribute_isInstance((MCObject *)a) || !a->methods ||
      MCObjectHeap_failed(h) || !MCObjectRootScope_begin(s, h) ||
      !MCObjectRootScope_pin(s, (MCObject *)a)) {
    MCObjectHeap_fail(h);
    return false;
  }
  return true;
}
#define ATTR_DISPATCH(ret, name, args, call, zero)                             \
  ret IAttribute_##name args {                                                 \
    MCObjectRootScope s = {0};                                                 \
    ret value = zero;                                                          \
    if (begin(a, &s) && a->methods->name)                                      \
      value = a->methods->name call;                                           \
    else                                                                       \
      MCObjectHeap_fail(a ? a->object.heap : NULL);                            \
    MCObjectRootScope_end(&s);                                                 \
    return value;                                                              \
  }
ATTR_DISPATCH(NBTString *, getAttributeUnlocalizedName, (IAttribute * a), (a),
              NULL)
ATTR_DISPATCH(double, clampValue, (IAttribute * a, double input), (a, input), 0)
ATTR_DISPATCH(double, getDefaultValue, (IAttribute * a), (a), 0)
ATTR_DISPATCH(bool, getShouldWatch, (IAttribute * a), (a), false)
ATTR_DISPATCH(IAttribute *, func_180372_d, (IAttribute * a), (a), NULL)
#undef ATTR_DISPATCH
bool IAttribute_equals(IAttribute *a, IAttribute *b) {
  if (!IAttribute_isInstance((MCObject *)a)) {
    MCObjectHeap_fail(a ? a->object.heap : NULL);
    return false;
  }
  if (!IAttribute_isInstance((MCObject *)b))
    return false;
  NBTString *name = IAttribute_getAttributeUnlocalizedName(a);
  NBTString *other = IAttribute_getAttributeUnlocalizedName(b);
  return name && other && NBTString_equals(name, other);
}
int32_t IAttribute_hashCode(IAttribute *a) {
  NBTString *name = IAttribute_getAttributeUnlocalizedName(a);
  if (!name) {
    MCObjectHeap_fail(a ? a->object.heap : NULL);
    return 0;
  }
  return NBTString_hashCode(name);
}
