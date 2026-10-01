#include "entity/SharedMonsterAttributes.h"
#include "entity/ai/attributes/AttributeInternal.h"
#include "entity/ai/attributes/RangedAttribute.h"
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
  SharedMonsterAttributes *s = (SharedMonsterAttributes *)o;
  s->maxHealth = (IAttribute *)v((MCObject *)s->maxHealth, c);
  s->followRange = (IAttribute *)v((MCObject *)s->followRange, c);
  s->knockbackResistance =
      (IAttribute *)v((MCObject *)s->knockbackResistance, c);
  s->movementSpeed = (IAttribute *)v((MCObject *)s->movementSpeed, c);
  s->attackDamage = (IAttribute *)v((MCObject *)s->attackDamage, c);
}
static const MCObjectClass klass = {"native.SharedMonsterAttributesStatics",
                                    MCObjectHeap_plainClone, trace, NULL};
static bool any(const MCObject *object, void *context) {
  (void)context;
  return MCObjectHeap_objectSize(object)>=sizeof(SharedMonsterAttributes);
}
static IAttribute *create(MCObjectHeap *h, const char *name, double def,
                          double min, double max, const char *description,
                          bool watch) {
  NBTString *n = NBTString_literalASCII(h, name);
  RangedAttribute *a =
      n ? RangedAttribute_new(h, NULL, n, def, min, max) : NULL;
  if (!a)
    return NULL;
  if (description && !RangedAttribute_setDescription(
                         a, NBTString_literalASCII(h, description)))
    return NULL;
  if (watch && !BaseAttribute_setShouldWatch(&a->base, true))
    return NULL;
  return (IAttribute *)a;
}
SharedMonsterAttributes *SharedMonsterAttributes_get(MCObjectHeap *h) {
  if (!h || MCObjectHeap_failed(h))
    return NULL;
  SharedMonsterAttributes *s =
      (SharedMonsterAttributes *)MCObjectHeap_findObject(h, &klass, any, NULL);
  if (s)
    return s;
  MCObjectRootScope scope = {0};
  if (!MCObjectRootScope_begin(&scope, h))
    return NULL;
  s = (SharedMonsterAttributes *)MCObjectHeap_alloc(h, sizeof *s, &klass);
  if (!s)
    goto fail;
  s->maxHealth =
      create(h, "generic.maxHealth", 20, 0, 1024, "Max Health", true);
  if (!s->maxHealth)
    goto fail;
  s->followRange =
      create(h, "generic.followRange", 32, 0, 2048, "Follow Range", false);
  if (!s->followRange)
    goto fail;
  s->knockbackResistance = create(h, "generic.knockbackResistance", 0, 0, 1,
                                  "Knockback Resistance", false);
  if (!s->knockbackResistance)
    goto fail;
  s->movementSpeed = create(h, "generic.movementSpeed", 0.699999988079071, 0,
                            1024, "Movement Speed", true);
  if (!s->movementSpeed)
    goto fail;
  s->attackDamage = create(h, "generic.attackDamage", 2, 0, 2048, NULL, false);
  if (!s->attackDamage)
    goto fail;
  MCObjectRoot root = {0};
  if (!MCObjectRoot_init(&root, h, (MCObject *)s))
    goto fail;
  MCObjectRootScope_end(&scope);
  return s;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&scope);
  return NULL;
}
static bool begin(MCObject *o, MCObject *arg, MCObjectRootScope *scope) {
  MCObjectHeap *h = o ? o->heap : NULL;
  if (!o || MCObjectHeap_failed(h) || !MCObjectRootScope_begin(scope, h) ||
      !MCObjectRootScope_pin(scope, o) || !MCObjectRootScope_pin(scope, arg)) {
    MCObjectHeap_fail(h);
    return false;
  }
  return true;
}
NBTTagCompound *
SharedMonsterAttributes_writeAttributeModifierToNBT(AttributeModifier *m) {
  MCObjectRootScope scope = {0};
  MCObjectHeap *h = m ? m->object.heap : NULL;
  if (!begin((MCObject *)m, NULL, &scope) ||
      !AttributeModifier_isInstance((MCObject *)m))
    goto fail;
  NBTTagCompound *tag = NBTTagCompound_new(h);
  if (!tag)
    goto fail;
  if (!NBTTagCompound_setString_ascii(tag, "Name",
                                      AttributeModifier_getName(m)) ||
      !NBTTagCompound_setDouble_ascii(tag, "Amount",
                                      AttributeModifier_getAmount(m)) ||
      !NBTTagCompound_setInteger_ascii(tag, "Operation",
                                       AttributeModifier_getOperation(m)))
    goto fail;
  NativeJavaUUID *id = AttributeModifier_getID(m);
  if (!NativeJavaUUID_isInstance((MCObject *)id))
    goto fail;
  if (!NBTTagCompound_setLong_ascii(tag, "UUIDMost", id->mostSignificantBits) ||
      !NBTTagCompound_setLong_ascii(
          tag, "UUIDLeast", AttributeModifier_getID(m)->leastSignificantBits))
    goto fail;
  MCObjectRootScope_end(&scope);
  return tag;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&scope);
  return NULL;
}
NBTTagCompound *
SharedMonsterAttributes_writeAttributeInstanceToNBT(IAttributeInstance *i) {
  MCObjectRootScope scope = {0};
  MCObjectHeap *h = i ? i->object.heap : NULL;
  if (!begin((MCObject *)i, NULL, &scope) ||
      !IAttributeInstance_isInstance((MCObject *)i))
    goto fail;
  NBTTagCompound *tag = NBTTagCompound_new(h);
  if (!tag)
    goto fail;
  IAttribute *a = IAttributeInstance_getAttribute(i);
  if (!NBTTagCompound_setString_ascii(
          tag, "Name", IAttribute_getAttributeUnlocalizedName(a)) ||
      !NBTTagCompound_setDouble_ascii(tag, "Base",
                                      IAttributeInstance_getBaseValue(i)))
    goto fail;
  AttributeCollection *all = IAttributeInstance_func_111122_c(i);
  if (all && AttributeCollection_size(all) > 0) {
    NBTTagList *list = NBTTagList_new(h);
    if (!list)
      goto fail;
    for (int32_t j = 0; j < AttributeCollection_size(all); j++) {
      AttributeModifier *m =
          (AttributeModifier *)AttributeCollection_getAt(all, j);
      if (AttributeModifier_isSaved(m)) {
        NBTTagCompound *entry =
            SharedMonsterAttributes_writeAttributeModifierToNBT(m);
        if (!entry || !NBTTagList_appendTag(list, (NBTBase *)entry))
          goto fail;
      }
    }
    if (!NBTTagCompound_setTag_ascii(tag, "Modifiers", (NBTBase *)list))
      goto fail;
  }
  if (MCObjectHeap_failed(h))
    goto fail;
  MCObjectRootScope_end(&scope);
  return tag;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&scope);
  return NULL;
}
NBTTagList *
SharedMonsterAttributes_writeBaseAttributeMapToNBT(BaseAttributeMap *m) {
  MCObjectRootScope scope = {0};
  MCObjectHeap *h = m ? m->object.heap : NULL;
  if (!begin((MCObject *)m, NULL, &scope) ||
      !BaseAttributeMap_isInstance((MCObject *)m))
    goto fail;
  NBTTagList *list = NBTTagList_new(h);
  AttributeCollection *all = BaseAttributeMap_getAllAttributes(m);
  if (!list || !all)
    goto fail;
  for (int32_t j = 0; j < AttributeCollection_size(all); j++) {
    NBTTagCompound *tag = SharedMonsterAttributes_writeAttributeInstanceToNBT(
        (IAttributeInstance *)AttributeCollection_getAt(all, j));
    if (!tag || !NBTTagList_appendTag(list, (NBTBase *)tag))
      goto fail;
  }
  MCObjectRootScope_end(&scope);
  return list;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&scope);
  return NULL;
}
AttributeModifier *SharedMonsterAttributes_readAttributeModifierFromNBT(
    NBTTagCompound *tag, const SharedMonsterAttributesLogging *logging,
    MCObject *context) {
  MCObjectRootScope scope = {0};
  MCObjectHeap *h = tag ? ((MCObject *)tag)->heap : NULL;
  if (!begin((MCObject *)tag, context, &scope) ||
      NBTBase_getId((NBTBase *)tag) != 10)
    goto fail;
  int64_t most = NBTTagCompound_getLong_ascii(tag, "UUIDMost"),
          least = NBTTagCompound_getLong_ascii(tag, "UUIDLeast");
  if (MCObjectHeap_failed(h))
    goto fail;
  NativeJavaUUID *id = NativeJavaUUID_new(h, most, least);
  if (!id)
    goto fail;
  NBTString *name = NBTTagCompound_getString_ascii(tag, "Name");
  double amount = NBTTagCompound_getDouble_ascii(tag, "Amount");
  int32_t op = NBTTagCompound_getInteger_ascii(tag, "Operation");
  if (MCObjectHeap_failed(h))
    goto fail;
  AttributeModifier *m =
      AttributeModifier_newCaught(h, id, name, amount, op, true);
  if (!m) {
    if (MCObjectHeap_failed(h) || !logging || !logging->invalidModifier ||
        !logging->invalidModifier(context))
      goto fail;
  }
  MCObjectRootScope_end(&scope);
  return m;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&scope);
  return NULL;
}
static bool apply(IAttributeInstance *i, NBTTagCompound *tag,
                  const SharedMonsterAttributesLogging *logging,
                  MCObject *context) {
  if (!IAttributeInstance_setBaseValue(
          i, NBTTagCompound_getDouble_ascii(tag, "Base")))
    return false;
  if (NBTTagCompound_hasKeyType_ascii(tag, "Modifiers", 9)) {
    NBTTagList *list = NBTTagCompound_getTagList_ascii(tag, "Modifiers", 10);
    if (!list)
      return false;
    for (int32_t j = 0; j < NBTTagList_tagCount(list); j++) {
      AttributeModifier *m =
          SharedMonsterAttributes_readAttributeModifierFromNBT(
              NBTTagList_getCompoundTagAt(list, j), logging, context);
      if (MCObjectHeap_failed(i->object.heap))
        return false;
      if (m) {
        AttributeModifier *previous =
            IAttributeInstance_getModifier(i, AttributeModifier_getID(m));
        if (previous && !IAttributeInstance_removeModifier(i, previous))
          return false;
        if (!IAttributeInstance_applyModifier(i, m))
          return false;
      }
    }
  }
  return !MCObjectHeap_failed(i->object.heap);
}
bool SharedMonsterAttributes_setAttributeModifiers(
    BaseAttributeMap *m, NBTTagList *list,
    const SharedMonsterAttributesLogging *logging, MCObject *context) {
  MCObjectRootScope scope = {0};
  MCObjectHeap *h = m ? m->object.heap : NULL;
  if (!begin((MCObject *)m, (MCObject *)list, &scope) ||
      !MCObjectRootScope_pin(&scope, context) ||
      !BaseAttributeMap_isInstance((MCObject *)m) || !list ||
      NBTBase_getId((NBTBase *)list) != 9)
    goto fail;
  for (int32_t j = 0; j < NBTTagList_tagCount(list); j++) {
    NBTTagCompound *tag = NBTTagList_getCompoundTagAt(list, j);
    IAttributeInstance *i = BaseAttributeMap_getAttributeInstanceByName(
        m, NBTTagCompound_getString_ascii(tag, "Name"));
    if (MCObjectHeap_failed(h))
      goto fail;
    if (i) {
      if (!apply(i, tag, logging, context))
        goto fail;
    } else if (!logging || !logging->unknownAttribute ||
               !logging->unknownAttribute(
                   context, NBTTagCompound_getString_ascii(tag, "Name")))
      goto fail;
  }
  MCObjectRootScope_end(&scope);
  return true;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&scope);
  return false;
}
