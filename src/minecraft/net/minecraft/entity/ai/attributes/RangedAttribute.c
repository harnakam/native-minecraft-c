#include "entity/ai/attributes/RangedAttribute.h"
#include "util/MathHelper.h"
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
  RangedAttribute *a = (RangedAttribute *)o;
  BaseAttribute_trace(&a->base, v, c);
  a->description = (NBTString *)v((MCObject *)a->description, c);
}
static const MCObjectClass klass = {
    "net.minecraft.entity.ai.attributes.RangedAttribute",
    MCObjectHeap_plainClone, trace, NULL};
static NBTString *name(IAttribute *a) {
  return BaseAttribute_getAttributeUnlocalizedName((BaseAttribute *)a);
}
static double def(IAttribute *a) {
  return BaseAttribute_getDefaultValue((BaseAttribute *)a);
}
static bool watch(IAttribute *a) {
  return BaseAttribute_getShouldWatch((BaseAttribute *)a);
}
static IAttribute *parent(IAttribute *a) {
  return BaseAttribute_func_180372_d((BaseAttribute *)a);
}
static double clamp(IAttribute *a, double v) {
  return RangedAttribute_clampValue((RangedAttribute *)a, v);
}
static const IAttributeMethods methods = {name, clamp, def, watch, parent};
bool RangedAttribute_isInstance(const MCObject *o) {
  return o && o->klass == &klass &&
         MCObjectHeap_objectSize(o) >= sizeof(RangedAttribute);
}
RangedAttribute *RangedAttribute_new(MCObjectHeap *h, IAttribute *p,
                                     NBTString *n, double value, double minimum,
                                     double maximum) {
  MCObjectRootScope s = {0};
  RangedAttribute *a = NULL;
  if (!MCObjectRootScope_begin(&s, h) ||
      !MCObjectRootScope_pin(&s, (MCObject *)p) ||
      !MCObjectRootScope_pin(&s, (MCObject *)n))
    goto fail;
  a = (RangedAttribute *)MCObjectHeap_alloc(h, sizeof *a, &klass);
  if (!a || !BaseAttribute_construct(&a->base, p, n, value, &methods))
    goto fail;
  a->minimumValue = minimum;
  a->maximumValue = maximum;
  if (minimum > maximum || value < minimum || value > maximum)
    goto fail;
  MCObjectRootScope_end(&s);
  return a;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&s);
  return NULL;
}
RangedAttribute *RangedAttribute_setDescription(RangedAttribute *a,
                                                NBTString *d) {
  MCObjectHeap *h = a ? a->base.attribute.object.heap : NULL;
  if (!RangedAttribute_isInstance((MCObject *)a) || MCObjectHeap_failed(h) ||
      (d &&
       (!NBTString_isInstance((MCObject *)d) || ((MCObject *)d)->heap != h))) {
    MCObjectHeap_fail(h);
    return NULL;
  }
  a->description = d;
  MCObjectHeap_touch(h);
  return a;
}
NBTString *RangedAttribute_getDescription(RangedAttribute *a) {
  if (!RangedAttribute_isInstance((MCObject *)a)) {
    MCObjectHeap_fail(a ? a->base.attribute.object.heap : NULL);
    return NULL;
  }
  return a->description;
}
double RangedAttribute_clampValue(RangedAttribute *a, double v) {
  if (!RangedAttribute_isInstance((MCObject *)a)) {
    MCObjectHeap_fail(a ? a->base.attribute.object.heap : NULL);
    return 0;
  }
  return MathHelper_clamp_double(v, a->minimumValue, a->maximumValue);
}
