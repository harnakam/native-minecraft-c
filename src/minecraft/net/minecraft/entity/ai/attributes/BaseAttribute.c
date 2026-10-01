#include "entity/ai/attributes/BaseAttribute.h"
bool BaseAttribute_construct(BaseAttribute *a, IAttribute *parent,
                             NBTString *name, double value,
                             const IAttributeMethods *methods) {
  MCObjectHeap *h = a ? a->attribute.object.heap : NULL;
  if (!a || MCObjectHeap_objectSize((MCObject *)a) < sizeof *a || !methods ||
      (parent && (!IAttribute_isInstance((MCObject *)parent) ||
                  parent->object.heap != h)) ||
      (name && (!NBTString_isInstance((MCObject *)name) ||
                ((MCObject *)name)->heap != h))) {
    MCObjectHeap_fail(h);
    return false;
  }
  a->attribute.methods = methods;
  a->field_180373_a = parent;
  a->unlocalizedName = name;
  a->defaultValue = value;
  MCObjectHeap_touch(h);
  if (!name) {
    MCObjectHeap_fail(h);
    return false;
  }
  return true;
}
void BaseAttribute_trace(BaseAttribute *a, MCObjectVisitor v, void *c) {
  a->field_180373_a = (IAttribute *)v((MCObject *)a->field_180373_a, c);
  a->unlocalizedName = (NBTString *)v((MCObject *)a->unlocalizedName, c);
}
static bool valid(BaseAttribute *a) {
  if (!IAttribute_isInstance((MCObject *)a) ||
      MCObjectHeap_failed(a ? a->attribute.object.heap : NULL)) {
    MCObjectHeap_fail(a ? a->attribute.object.heap : NULL);
    return false;
  }
  return true;
}
NBTString *BaseAttribute_getAttributeUnlocalizedName(BaseAttribute *a) {
  return valid(a) ? a->unlocalizedName : NULL;
}
double BaseAttribute_getDefaultValue(BaseAttribute *a) {
  return valid(a) ? a->defaultValue : 0;
}
bool BaseAttribute_getShouldWatch(BaseAttribute *a) {
  return valid(a) && a->shouldWatch;
}
BaseAttribute *BaseAttribute_setShouldWatch(BaseAttribute *a, bool value) {
  if (!valid(a))
    return NULL;
  a->shouldWatch = value;
  MCObjectHeap_touch(a->attribute.object.heap);
  return a;
}
IAttribute *BaseAttribute_func_180372_d(BaseAttribute *a) {
  return valid(a) ? a->field_180373_a : NULL;
}
int32_t BaseAttribute_hashCode(BaseAttribute *a) {
  return IAttribute_hashCode((IAttribute *)a);
}
bool BaseAttribute_equals(BaseAttribute *a, IAttribute *b) {
  return IAttribute_equals((IAttribute *)a, b);
}
