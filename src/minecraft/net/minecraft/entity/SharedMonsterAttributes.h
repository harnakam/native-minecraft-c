#ifndef C919_SOURCE_SHARED_MONSTER_ATTRIBUTES_H
#define C919_SOURCE_SHARED_MONSTER_ATTRIBUTES_H
#include "entity/ai/attributes/ServersideAttributeMap.h"
#include "nbt/NBTTagCompound.h"
typedef struct SharedMonsterAttributes {
    MCObject object;
    IAttribute *maxHealth,*followRange,*knockbackResistance,*movementSpeed,*attackDamage;
} SharedMonsterAttributes;
/* Source static identity provider is a native per-heap retained root. No object
   from one heap may be installed in another; snapshots preserve these refs. */
SharedMonsterAttributes *SharedMonsterAttributes_get(MCObjectHeap *);
typedef struct {
    bool (*unknownAttribute)(MCObject *,NBTString *);
    bool (*invalidModifier)(MCObject *);
} SharedMonsterAttributesLogging;
NBTTagList *SharedMonsterAttributes_writeBaseAttributeMapToNBT(BaseAttributeMap *);
NBTTagCompound *SharedMonsterAttributes_writeAttributeInstanceToNBT(IAttributeInstance *);
NBTTagCompound *SharedMonsterAttributes_writeAttributeModifierToNBT(AttributeModifier *);
bool SharedMonsterAttributes_setAttributeModifiers(BaseAttributeMap *,NBTTagList *,const SharedMonsterAttributesLogging *,MCObject *);
AttributeModifier *SharedMonsterAttributes_readAttributeModifierFromNBT(NBTTagCompound *,const SharedMonsterAttributesLogging *,MCObject *);
#endif
