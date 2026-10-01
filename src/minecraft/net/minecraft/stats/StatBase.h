#ifndef C919_STAT_BASE_H
#define C919_STAT_BASE_H
#include "nbt/NBTString.h"

/* Source constructor fields and objective identity. Chat components/IStatType
   and Class metadata are explicit managed dependencies, not formatter/chat
   implementations. Basic/Crafting identity factories remain native adapters;
   every such identity owns the real ObjectiveStat constructor result. */
typedef enum {
    STAT_BASE_KIND_UNKNOWN=-1,STAT_BASE_KIND_BASE,STAT_BASE_KIND_BASIC,STAT_BASE_KIND_CRAFTING,
    STAT_BASE_KIND_ACHIEVEMENT
} StatBaseKind;
typedef struct StatBase {
    MCObject object;
    NBTString *statId;
    MCObject *statName;
    bool isIndependent;
    MCObject *type;
    struct IScoreObjectiveCriteria *objectiveCriteria;
    MCObject *field_150956_d;
} StatBase;
typedef struct StatList StatList;
/* Exact three-argument constructor body. The delegated overload receives its
   already evaluated class-static simpleStatType dependency explicitly. */
StatBase *StatBase_new(MCObjectHeap *,NBTString *,MCObject *statName,MCObject *type);
StatBase *StatBase_newWithSimpleType(MCObjectHeap *,NBTString *,MCObject *statName,MCObject *capturedSimpleStatType);
bool StatBase_construct(StatBase *,NBTString *,MCObject *statName,MCObject *type);
bool StatBase_constructIdentity(StatBase *,NBTString *);
struct IScoreObjectiveCriteria *StatBase_getCriteria(StatBase *);
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
