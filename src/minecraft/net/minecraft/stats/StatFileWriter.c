#include "stats/StatFileWriter.h"
#include <string.h>

typedef struct StatTuple { MCObject object; int32_t integerValue; MCObject *jsonSerializableValue; } StatTuple;
typedef struct StatEntry {
    MCObject object; StatBase *key; StatTuple *value; struct StatEntry *next; uint32_t hash;
} StatEntry;
typedef struct StatBuckets { MCObject object; size_t capacity; StatEntry *entries[]; } StatBuckets;
struct StatMap { MCObject object; StatBuckets *table; size_t count; };
static void tuple_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    StatTuple *tuple=(StatTuple *)object; tuple->jsonSerializableValue=visitor(tuple->jsonSerializableValue,context);
}
static void entry_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    StatEntry *entry=(StatEntry *)object;
    entry->key=(StatBase *)visitor((MCObject *)entry->key,context);
    entry->value=(StatTuple *)visitor((MCObject *)entry->value,context);
    entry->next=(StatEntry *)visitor((MCObject *)entry->next,context);
}
static void buckets_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    StatBuckets *buckets=(StatBuckets *)object;
    for (size_t i=0;i<buckets->capacity;i++) buckets->entries[i]=(StatEntry *)visitor((MCObject *)buckets->entries[i],context);
}
static void map_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    StatMap *map=(StatMap *)object; map->table=(StatBuckets *)visitor((MCObject *)map->table,context);
}
void StatFileWriter_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    StatFileWriter *writer=(StatFileWriter *)object; writer->statsData=(StatMap *)visitor((MCObject *)writer->statsData,context);
}
static const MCObjectClass tuple_class={"net.minecraft.util.TupleIntJsonSerializable",MCObjectHeap_plainClone,tuple_trace,NULL};
static const MCObjectClass entry_class={"C919.StatMapEntry",MCObjectHeap_plainClone,entry_trace,NULL};
static const MCObjectClass buckets_class={"C919.StatMapBuckets",MCObjectHeap_plainClone,buckets_trace,NULL};
static const MCObjectClass map_class={"C919.StatCounterMap",MCObjectHeap_plainClone,map_trace,NULL};
static const MCObjectClass writer_class={"net.minecraft.stats.StatFileWriter",MCObjectHeap_plainClone,StatFileWriter_trace,NULL};
static StatBuckets *buckets_new(MCObjectHeap *heap,size_t capacity) {
    if (!capacity || capacity>(SIZE_MAX-sizeof(StatBuckets))/sizeof(StatEntry *)) { MCObjectHeap_fail(heap); return NULL; }
    StatBuckets *buckets=(StatBuckets *)MCObjectHeap_alloc(heap,sizeof(*buckets)+capacity*sizeof(*buckets->entries),&buckets_class);
    if (buckets) buckets->capacity=capacity;
    return buckets;
}
bool StatFileWriter_construct(StatFileWriter *writer,const StatFileWriterOverrides *overrides) {
    if(!writer)return false;
    MCObjectHeap *heap=writer->object.heap;
    writer->statsData=(StatMap *)MCObjectHeap_alloc(heap,sizeof(StatMap),&map_class);
    if (!writer->statsData) return false;
    writer->statsData->table=buckets_new(heap,16); if (!writer->statsData->table) return false;
    writer->overrides=overrides;return true;
}
StatFileWriter *StatFileWriter_newWithOverrides(MCObjectHeap *heap,const StatFileWriterOverrides *overrides) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    StatFileWriter *writer=(StatFileWriter *)MCObjectHeap_alloc(heap,sizeof(*writer),&writer_class);
    if(writer&&!StatFileWriter_construct(writer,overrides))writer=NULL;
    MCObjectRootScope_end(&scope);return writer;
}
StatFileWriter *StatFileWriter_new(MCObjectHeap *heap) { return StatFileWriter_newWithOverrides(heap,NULL); }
static bool valid(const StatFileWriter *writer,const StatBase *stat) {
    if (!writer) return false;
    if (!stat || stat->object.heap!=writer->object.heap || StatBase_getKind(stat)<STAT_BASE_KIND_BASE) {
        MCObjectHeap_fail(writer->object.heap); return false;
    }
    return !MCObjectHeap_failed(writer->object.heap);
}
static uint32_t stat_hash(const StatBase *stat) {
    uint32_t h=(uint32_t)StatBase_hashCode(stat); return h^(h>>16);
}
static StatEntry *find(const StatFileWriter *writer,const StatBase *stat) {
    if (!valid(writer,stat)) return NULL;
    uint32_t hash=stat_hash(stat);
    if (MCObjectHeap_failed(writer->object.heap)) return NULL;
    StatBuckets *table=writer->statsData->table;
    for (StatEntry *entry=table->entries[hash&(table->capacity-1)];entry;entry=entry->next)
        if (entry->hash==hash && StatBase_equals(entry->key,stat)) return entry;
    return NULL;
}
static bool grow(StatMap *map) {
    StatBuckets *old=map->table;
    if (map->count<old->capacity-old->capacity/4) return true;
    if (old->capacity>SIZE_MAX/2) { MCObjectHeap_fail(map->object.heap); return false; }
    StatBuckets *table=buckets_new(map->object.heap,old->capacity*2); if (!table) return false;
    for (size_t i=0;i<old->capacity;i++) for (StatEntry *entry=old->entries[i];entry;) {
        StatEntry *next=entry->next; size_t index=entry->hash&(table->capacity-1);
        entry->next=table->entries[index]; table->entries[index]=entry; entry=next;
    }
    map->table=table; MCObjectHeap_touch(map->object.heap); return true;
}
static StatTuple *get_or_create(StatFileWriter *writer,StatBase *stat) {
    StatEntry *old=find(writer,stat); if (old) return old->value;
    MCObjectHeap *heap=writer->object.heap;
    if (MCObjectHeap_failed(heap)) return NULL;
    StatTuple *tuple=(StatTuple *)MCObjectHeap_alloc(heap,sizeof(*tuple),&tuple_class);
    StatEntry *entry=(StatEntry *)MCObjectHeap_alloc(heap,sizeof(*entry),&entry_class);
    if (!tuple || !entry || !grow(writer->statsData)) return NULL;
    StatBuckets *table=writer->statsData->table;
    entry->key=stat; entry->value=tuple; entry->hash=stat_hash(stat);
    size_t index=entry->hash&(table->capacity-1); entry->next=table->entries[index]; table->entries[index]=entry;
    ++writer->statsData->count; MCObjectHeap_touch(heap); return tuple;
}
int32_t StatFileWriter_readStat(const StatFileWriter *writer,const StatBase *stat) {
    StatEntry *entry=find(writer,stat); return entry ? entry->value->integerValue : 0;
}
bool StatFileWriter_hasAchievementUnlocked(const StatFileWriter *writer,const Achievement *achievement) {
    return StatFileWriter_readStat(writer,(const StatBase *)achievement)>0;
}
bool StatFileWriter_canUnlockAchievement(const StatFileWriter *writer,const Achievement *achievement) {
    if (!valid(writer,(const StatBase *)achievement) || !StatBase_isAchievement(&achievement->base)) {
        if (writer) MCObjectHeap_fail(writer->object.heap);
        return false;
    }
    return !achievement->parentAchievement || StatFileWriter_hasAchievementUnlocked(writer,achievement->parentAchievement);
}
static const Achievement *next_locked(const StatFileWriter *writer,const Achievement *achievement) {
    return achievement && !StatFileWriter_hasAchievementUnlocked(writer,achievement) ? achievement->parentAchievement : NULL;
}
int32_t StatFileWriter_func_150874_c(const StatFileWriter *writer,const Achievement *achievement) {
    if (!valid(writer,(const StatBase *)achievement) || !StatBase_isAchievement(&achievement->base)) {
        if (writer) MCObjectHeap_fail(writer->object.heap);
        return 0;
    }
    if (StatFileWriter_hasAchievementUnlocked(writer,achievement)) return 0;
    uint32_t count=0;
    const Achievement *slow=achievement->parentAchievement,*fast=slow;
    for (const Achievement *current=slow;current && !StatFileWriter_hasAchievementUnlocked(writer,current);current=current->parentAchievement) {
        ++count; slow=next_locked(writer,slow); fast=next_locked(writer,next_locked(writer,fast));
        if (MCObjectHeap_failed(writer->object.heap) || (slow && slow==fast)) {
            MCObjectHeap_fail(writer->object.heap); return 0;
        }
    }
    int32_t value; memcpy(&value,&count,sizeof(value)); return value;
}
bool StatFileWriter_unlockAchievementBase(StatFileWriter *writer,MCObject *player,StatBase *stat,int32_t value) {
    (void)player;
    if (!valid(writer,stat)) return false;
    StatTuple *tuple=get_or_create(writer,stat); if (!tuple) return false;
    tuple->integerValue=value; MCObjectHeap_touch(writer->object.heap); return !MCObjectHeap_failed(writer->object.heap);
}
bool StatFileWriter_unlockAchievement(StatFileWriter *writer,MCObject *player,StatBase *stat,int32_t value) {
    if (!valid(writer,stat)) return false;
    if (!writer->overrides || !writer->overrides->unlockAchievement)
        return StatFileWriter_unlockAchievementBase(writer,player,stat,value);
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,writer->object.heap)) return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)writer) && MCObjectRootScope_pin(&scope,(MCObject *)stat) &&
        MCObjectRootScope_pin(&scope,player) && writer->overrides->unlockAchievement(writer,player,stat,value);
    if (!ok) MCObjectHeap_fail(writer->object.heap);
    MCObjectRootScope_end(&scope); return ok && !MCObjectHeap_failed(writer->object.heap);
}
bool StatFileWriter_increaseStat(StatFileWriter *writer,MCObject *player,StatBase *stat,int32_t amount) {
    if (!valid(writer,stat)) return false;
    if (StatBase_isAchievement(stat) && !StatFileWriter_canUnlockAchievement(writer,(Achievement *)stat))
        return !MCObjectHeap_failed(writer->object.heap);
    uint32_t bits=(uint32_t)StatFileWriter_readStat(writer,stat)+(uint32_t)amount;
    int32_t value; memcpy(&value,&bits,sizeof(value));
    return StatFileWriter_unlockAchievement(writer,player,stat,value);
}
MCObject *StatFileWriter_func_150870_b(const StatFileWriter *writer,const StatBase *stat) {
    StatEntry *entry=find(writer,stat); return entry ? entry->value->jsonSerializableValue : NULL;
}
MCObject *StatFileWriter_func_150872_a(StatFileWriter *writer,StatBase *stat,MCObject *progress) {
    if (!valid(writer,stat)) return NULL;
    if (progress && progress->heap!=writer->object.heap) { MCObjectHeap_fail(writer->object.heap); return NULL; }
    StatTuple *tuple=get_or_create(writer,stat); if (!tuple) return NULL;
    tuple->jsonSerializableValue=progress; MCObjectHeap_touch(writer->object.heap); return progress;
}
size_t StatFileWriter_statCount(const StatFileWriter *writer) { return writer ? writer->statsData->count : 0; }
bool StatFileWriter_clear(StatFileWriter *writer) {
    if(!writer||!writer->statsData)return false;
    StatMap *map=writer->statsData;
    memset(map->table->entries,0,map->table->capacity*sizeof(*map->table->entries));
    map->count=0;MCObjectHeap_touch(writer->object.heap);return !MCObjectHeap_failed(writer->object.heap);
}
bool StatFileWriter_putAll(StatFileWriter *destination,const StatFileWriter *source) {
    MCObjectHeap *heap=destination?destination->object.heap:NULL;
    if(!destination||!source||source->object.heap!=heap){MCObjectHeap_fail(heap);return false;}
    if(destination==source)return !MCObjectHeap_failed(heap);
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)destination)&&MCObjectRootScope_pin(&scope,(MCObject *)source);
    StatBuckets *sourceTable=source->statsData->table;
    for(size_t i=0;ok&&i<sourceTable->capacity;i++)for(StatEntry *entry=sourceTable->entries[i];ok&&entry;entry=entry->next) {
        StatEntry *old=find(destination,entry->key);
        if(MCObjectHeap_failed(heap)){ok=false;break;}
        if(old)old->value=entry->value;
        else {
            StatEntry *created=(StatEntry *)MCObjectHeap_alloc(heap,sizeof(*created),&entry_class);
            if(!created||!grow(destination->statsData)){ok=false;break;}
            StatBuckets *table=destination->statsData->table;size_t index=entry->hash&(table->capacity-1);
            created->key=entry->key;created->value=entry->value;created->hash=entry->hash;
            created->next=table->entries[index];table->entries[index]=created;++destination->statsData->count;
        }
        MCObjectHeap_touch(heap);
    }
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(heap);
}
bool StatFileWriter_entryAt(const StatFileWriter *writer,size_t index,StatBase **stat,int32_t *value,MCObject **progress) {
    if (!writer || index>=writer->statsData->count) return false;
    StatBuckets *table=writer->statsData->table;
    for (size_t i=0;i<table->capacity;i++) for (StatEntry *entry=table->entries[i];entry;entry=entry->next) {
        if (index--) continue;
        if (stat) *stat=entry->key;
        if (value) *value=entry->value->integerValue;
        if (progress) *progress=entry->value->jsonSerializableValue;
        return true;
    }
    return false;
}
