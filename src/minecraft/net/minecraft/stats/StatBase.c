#include "stats/StatBase.h"
#include "stats/StatList.h"

extern const MCObjectClass c919_achievement_class;
extern const MCObjectClass c919_statcrafting_class;
void StatBase_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    StatBase *stat=(StatBase *)object;
    stat->statId=(NBTString *)visitor((MCObject *)stat->statId,context);
}
static const MCObjectClass base_class={"net.minecraft.stats.StatBase",MCObjectHeap_plainClone,StatBase_trace,NULL};
static const MCObjectClass basic_class={"C919.identity.StatBasic",MCObjectHeap_plainClone,StatBase_trace,NULL};
static const MCObjectClass crafting_class={"C919.identity.StatCrafting",MCObjectHeap_plainClone,StatBase_trace,NULL};
StatBase *StatBase_newIdentity(MCObjectHeap *heap,NBTString *id,StatBaseKind kind) {
    const MCObjectClass *klass=kind==STAT_BASE_KIND_BASE ? &base_class :
        kind==STAT_BASE_KIND_BASIC ? &basic_class : kind==STAT_BASE_KIND_CRAFTING ? &crafting_class : NULL;
    if (!klass || (id && ((MCObject *)id)->heap!=heap)) { MCObjectHeap_fail(heap); return NULL; }
    StatBase *stat=(StatBase *)MCObjectHeap_alloc(heap,sizeof(*stat),klass);
    if (stat) stat->statId=id;
    return stat;
}
StatBaseKind StatBase_getKind(const StatBase *stat) {
    if (!stat) return STAT_BASE_KIND_UNKNOWN;
    const MCObjectClass *klass=stat->object.klass;
    if (klass==&base_class) return STAT_BASE_KIND_BASE;
    if (klass==&basic_class) return STAT_BASE_KIND_BASIC;
    if (klass==&crafting_class || klass==&c919_statcrafting_class) return STAT_BASE_KIND_CRAFTING;
    if (klass==&c919_achievement_class) return STAT_BASE_KIND_ACHIEVEMENT;
    return STAT_BASE_KIND_UNKNOWN;
}
StatBase *StatBase_initIndependentStat(StatBase *stat) {
    if (stat) { stat->isIndependent=true; MCObjectHeap_touch(stat->object.heap); }
    return stat;
}
bool StatBase_isAchievement(const StatBase *stat) { return StatBase_getKind(stat)==STAT_BASE_KIND_ACHIEVEMENT; }
bool StatBase_equals(const StatBase *stat,const StatBase *other) {
    if (stat==other) return true;
    if (!stat || !other || stat->object.klass!=other->object.klass) return false;
    if (!stat->statId) { MCObjectHeap_fail(stat->object.heap); return false; }
    return NBTString_equals(stat->statId,other->statId);
}
int32_t StatBase_hashCode(const StatBase *stat) {
    if (!stat) return 0;
    if (!stat->statId) { MCObjectHeap_fail(stat->object.heap); return 0; }
    return NBTString_hashCode(stat->statId);
}
StatBase *StatBase_registerStat(StatBase *stat,StatList *list) {
    MCObjectHeap *heap=list?list->object.heap:stat?stat->object.heap:NULL;
    if(!list||!stat||stat->object.heap!=heap||StatList_getOneShotStat(list,stat->statId)) {
        MCObjectHeap_fail(heap);return NULL;
    }
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)list)&&MCObjectRootScope_pin(&scope,(MCObject *)stat)&&
        StatListList_add(list->allStats,stat)&&StatBaseRegistry_register(list->oneShotStats,stat);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(heap)?stat:NULL;
}
typedef struct RegistryEntry {
    MCObject object;
    StatBase *stat;
    struct RegistryEntry *next;
} RegistryEntry;
struct StatBaseRegistry { MCObject object; RegistryEntry *entries; size_t count; };
static void entry_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    RegistryEntry *entry=(RegistryEntry *)object;
    entry->stat=(StatBase *)visitor((MCObject *)entry->stat,context);
    entry->next=(RegistryEntry *)visitor((MCObject *)entry->next,context);
}
static void registry_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    StatBaseRegistry *registry=(StatBaseRegistry *)object;
    registry->entries=(RegistryEntry *)visitor((MCObject *)registry->entries,context);
}
static const MCObjectClass entry_class={"C919.StatRegistryEntry",MCObjectHeap_plainClone,entry_trace,NULL};
static const MCObjectClass registry_class={"C919.StatListRegistry",MCObjectHeap_plainClone,registry_trace,NULL};
StatBaseRegistry *StatBaseRegistry_new(MCObjectHeap *heap) {
    return (StatBaseRegistry *)MCObjectHeap_alloc(heap,sizeof(StatBaseRegistry),&registry_class);
}
StatBase *StatBaseRegistry_find(const StatBaseRegistry *registry,const NBTString *id) {
    if (!registry) return NULL;
    for (RegistryEntry *entry=registry->entries;entry;entry=entry->next)
        if (NBTString_equals(entry->stat->statId,id)) return entry->stat;
    return NULL;
}
StatBase *StatBaseRegistry_find_ascii(const StatBaseRegistry *registry,const char *id) {
    if (!registry) return NULL;
    if (!id) return StatBaseRegistry_find(registry,NULL);
    for (RegistryEntry *entry=registry->entries;entry;entry=entry->next)
        if (NBTString_equalsASCII(entry->stat->statId,id)) return entry->stat;
    return NULL;
}
bool StatBaseRegistry_register(StatBaseRegistry *registry,StatBase *stat) {
    if (!registry) return false;
    MCObjectHeap *heap=registry->object.heap;
    if (!stat || stat->object.heap!=heap || StatBase_getKind(stat)<STAT_BASE_KIND_BASE ||
        StatBaseRegistry_find(registry,stat->statId)) { MCObjectHeap_fail(heap); return false; }
    RegistryEntry *entry=(RegistryEntry *)MCObjectHeap_alloc(heap,sizeof(*entry),&entry_class);
    if (!entry) return false;
    entry->stat=stat; entry->next=registry->entries; registry->entries=entry; ++registry->count;
    MCObjectHeap_touch(heap); return true;
}
size_t StatBaseRegistry_size(const StatBaseRegistry *registry) { return registry ? registry->count : 0; }
