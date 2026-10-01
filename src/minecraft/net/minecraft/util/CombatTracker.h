#ifndef C919_SOURCE_COMBAT_TRACKER_H
#define C919_SOURCE_COMBAT_TRACKER_H
#include "nbt/NBTString.h"
typedef struct EntityLivingBase EntityLivingBase;
/* Native managed ArrayList storage at its original empty-constructor state.
   Both Java8 zero-length element-data identities are distinct per heap. There
   are no combat-list mutators until CombatEntry/actual combat bodies are ported. */
typedef struct CombatObjectArray { MCObject object; int32_t length; MCObject *items[]; } CombatObjectArray;
typedef struct CombatEntryList { MCObject object; CombatObjectArray *elementData; int32_t size,modCount; } CombatEntryList;
typedef struct CombatTracker {
    MCObject object;
    CombatEntryList *combatEntries;
    EntityLivingBase *fighter;
    int32_t field_94555_c,field_152775_d,field_152776_e;
    bool field_94552_d,field_94553_e;
    NBTString *field_94551_f;
} CombatTracker;
/* Original full constructor/state only. DamageSource/CombatEntry/chat/combat
   methods are explicitly unported, rather than returning empty success. */
CombatTracker *CombatTracker_new(MCObjectHeap *,EntityLivingBase *);
bool CombatTracker_isInstance(const MCObject *);
bool CombatEntryList_isInstance(const MCObject *);
#endif
