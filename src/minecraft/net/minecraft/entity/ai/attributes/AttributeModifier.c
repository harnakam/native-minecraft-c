#include "entity/ai/attributes/AttributeModifier.h"
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
  AttributeModifier *m = (AttributeModifier *)o;
  m->name = (NBTString *)v((MCObject *)m->name, c);
  m->id = (NativeJavaUUID *)v((MCObject *)m->id, c);
}
static const MCObjectClass klass = {
    "net.minecraft.entity.ai.attributes.AttributeModifier",
    MCObjectHeap_plainClone, trace, NULL};
bool AttributeModifier_isInstance(const MCObject *o) {
  return o && o->klass == &klass &&
         MCObjectHeap_objectSize(o) >= sizeof(AttributeModifier);
}
static bool valid(AttributeModifier *m) {
  if (!AttributeModifier_isInstance((MCObject *)m) ||
      MCObjectHeap_failed(m ? m->object.heap : NULL)) {
    MCObjectHeap_fail(m ? m->object.heap : NULL);
    return false;
  }
  return true;
}
AttributeModifier *AttributeModifier_newCaught(MCObjectHeap *h,
                                               NativeJavaUUID *id,
                                               NBTString *name, double amount,
                                               int32_t op,
                                               bool catchValidation) {
  MCObjectRootScope s = {0};
  AttributeModifier *m = NULL;
  if (!MCObjectRootScope_begin(&s, h) ||
      !MCObjectRootScope_pin(&s, (MCObject *)id) ||
      !MCObjectRootScope_pin(&s, (MCObject *)name))
    goto fail;
  if ((id && !NativeJavaUUID_isInstance((MCObject *)id)) ||
      (name && !NBTString_isInstance((MCObject *)name)))
    goto fail;
  m = (AttributeModifier *)MCObjectHeap_alloc(h, sizeof *m, &klass);
  if (!m)
    goto fail;
  m->isSaved = true;
  m->id = id;
  m->name = name;
  m->amount = amount;
  m->operation = op;
  if (!name || NBTString_length(name) == 0 || op < 0 || op > 2) {
    if (catchValidation) {
      MCObjectRootScope_end(&s);
      return NULL;
    }
    goto fail;
  }
  MCObjectRootScope_end(&s);
  return m;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&s);
  return NULL;
}
AttributeModifier *AttributeModifier_new(MCObjectHeap *h, NativeJavaUUID *id,
                                         NBTString *name, double amount,
                                         int32_t op) {
  return AttributeModifier_newCaught(h, id, name, amount, op, false);
}
AttributeModifier *
AttributeModifier_newRandom(MCObjectHeap *h, NBTString *n, double amount,
                            int32_t op, const AttributeModifierDependencies *d,
                            MCObject *context) {
  MCObjectRootScope s = {0};
  AttributeModifier *m = NULL;
  if (!MCObjectRootScope_begin(&s, h) ||
      !MCObjectRootScope_pin(&s, (MCObject *)n) ||
      !MCObjectRootScope_pin(&s, context) || !d || !d->threadLocalRandomUuid)
    goto fail;
  NativeJavaUUID *id = d->threadLocalRandomUuid(context);
  if (!id || !NativeJavaUUID_isInstance((MCObject *)id) ||
      id->object.heap != h || MCObjectHeap_failed(h))
    goto fail;
  m = AttributeModifier_new(h, id, n, amount, op);
  MCObjectRootScope_end(&s);
  return m;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&s);
  return NULL;
}
NativeJavaUUID *AttributeModifier_getID(AttributeModifier *m) {
  return valid(m) ? m->id : NULL;
}
NBTString *AttributeModifier_getName(AttributeModifier *m) {
  return valid(m) ? m->name : NULL;
}
int32_t AttributeModifier_getOperation(AttributeModifier *m) {
  return valid(m) ? m->operation : 0;
}
double AttributeModifier_getAmount(AttributeModifier *m) {
  return valid(m) ? m->amount : 0;
}
bool AttributeModifier_isSaved(AttributeModifier *m) {
  return valid(m) && m->isSaved;
}
AttributeModifier *AttributeModifier_setSaved(AttributeModifier *m, bool v) {
  if (!valid(m))
    return NULL;
  m->isSaved = v;
  MCObjectHeap_touch(m->object.heap);
  return m;
}
bool AttributeModifier_equals(AttributeModifier *a, AttributeModifier *b) {
  if (!valid(a))
    return false;
  if (a == b)
    return true;
  if (!AttributeModifier_isInstance((MCObject *)a) ||
      !AttributeModifier_isInstance((MCObject *)b))
    return false;
  return a->id == b->id ||
         (a->id && b->id &&
          a->id->mostSignificantBits == b->id->mostSignificantBits &&
          a->id->leastSignificantBits == b->id->leastSignificantBits);
}
int32_t AttributeModifier_hashCode(AttributeModifier *m) {
  if (!valid(m) || !m->id)
    return 0;
  uint64_t x = (uint64_t)m->id->mostSignificantBits ^
               (uint64_t)m->id->leastSignificantBits;
  uint32_t h = (uint32_t)x ^ (uint32_t)(x >> 32);
  return h <= INT32_MAX ? (int32_t)h : -1 - (int32_t)(UINT32_MAX - h);
}
NBTString *AttributeModifier_toString(AttributeModifier *m,
                                      const AttributeModifierDependencies *d,
                                      MCObject *context) {
  MCObjectRootScope s = {0};
  NBTString *result = NULL;
  MCObjectHeap *h = m ? m->object.heap : NULL;
  if (!valid(m) || !MCObjectRootScope_begin(&s, h) ||
      !MCObjectRootScope_pin(&s, (MCObject *)m) ||
      !MCObjectRootScope_pin(&s, context) || !d || !d->formatToString)
    goto fail;
  result = d->formatToString(context, m);
  if (!NBTString_isInstance((MCObject *)result) ||
      ((MCObject *)result)->heap != h || MCObjectHeap_failed(h))
    goto fail;
  MCObjectRootScope_end(&s);
  return result;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&s);
  return NULL;
}
