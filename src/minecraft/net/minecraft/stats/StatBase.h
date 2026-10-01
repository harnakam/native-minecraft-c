#ifndef C919_STAT_BASE_H
#define C919_STAT_BASE_H
#include "nbt/NBTString.h"

/* Identity fields needed by the source equals/hashCode and counter methods.
   Chat components, formatters, objective construction/registration and the
   original overloaded constructors are separate, undeclared dependencies.
   Basic/Crafting identities allocated here are native constructor adapters.
   StatCrafting's separate source Item field/getter has its own descriptor;
   display/objective behavior remains a separate dependency. */
typedef enum {
    STAT_BASE_KIND_UNKNOWN=-1,STAT_BASE_KIND_BASE,STAT_BASE_KIND_BASIC,STAT_BASE_KIND_CRAFTING,
    STAT_BASE_KIND_ACHIEVEMENT
} StatBaseKind;
typedef struct StatBase {
    MCObject object;
    NBTString *statId;
    bool isIndependent;
} StatBase;
typedef struct StatList StatList;
StatBase *StatBase_newIdentity(MCObjectHeap *,NBTString *id,StatBaseKind kind);
StatBase *StatBase_initIndependentStat(StatBase *);
bool StatBase_isAchievement(const StatBase *);
bool StatBase_equals(const StatBase *,const StatBase *);
int32_t StatBase_hashCode(const StatBase *);
StatBaseKind StatBase_getKind(const StatBase *);
/* Original registration body with explicit per-heap static-field owner.
   Duplicate IDs fail the heap; allStats and oneShotStats retain this object. */
StatBase *StatBase_registerStat(StatBase *,StatList *);
/* Base-field trace is also used by the Achievement class descriptor. */
void StatBase_trace(MCObject *,MCObjectVisitor,void *);

/* Native canonical registry adapter for StatList.oneShotStats. Registration
   retains the same object and rejects an existing ID across every subclass.
   It does not stand in for StatList.init or its recipe/furnace enumeration. */
typedef struct StatBaseRegistry StatBaseRegistry;
StatBaseRegistry *StatBaseRegistry_new(MCObjectHeap *);
bool StatBaseRegistry_register(StatBaseRegistry *,StatBase *);
StatBase *StatBaseRegistry_find(const StatBaseRegistry *,const NBTString *id);
StatBase *StatBaseRegistry_find_ascii(const StatBaseRegistry *,const char *id);
size_t StatBaseRegistry_size(const StatBaseRegistry *);
#endif
