#include "MCObjectHeap.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

typedef struct ObjectEntry {
    struct ObjectEntry *next;
    size_t bytes;
    uint32_t identity_hash;
    bool marked;
    max_align_t alignment;
    unsigned char data[];
} ObjectEntry;
typedef struct { uint64_t id; MCObject *object; } RootEntry;
struct MCObjectHeap {
    ObjectEntry *objects;
    size_t bytes, count, budget;
    RootEntry *roots;
    size_t root_count, root_capacity, scopes;
    uint64_t revision, lineage;
    const MCObjectHeap *parent;
    MCObjectHeap *adopted;
    uint64_t parent_revision;
    bool failed;
};
static atomic_uint_fast64_t next_lineage=1, next_root=1, next_object=1;
static uint64_t unique_id(atomic_uint_fast64_t *counter) {
    uint_fast64_t value=atomic_load_explicit(counter,memory_order_relaxed);
    while (value && value<UINT64_MAX) {
        if (atomic_compare_exchange_weak_explicit(counter,&value,value+1,
            memory_order_relaxed,memory_order_relaxed)) return (uint64_t)value;
    }
    return 0;
}
static ObjectEntry *entry(const MCObject *object) {
    return (ObjectEntry *)((unsigned char *)object - offsetof(ObjectEntry,data));
}
static void release_objects(ObjectEntry *object) {
    while (object) {
        ObjectEntry *next=object->next;
        MCObject *value=(MCObject *)object->data;
        if (value->klass->destroy) value->klass->destroy(value);
        free(object); object=next;
    }
}
MCObjectHeap *MCObjectHeap_new(size_t byte_budget) {
    MCObjectHeap *heap=calloc(1,sizeof(*heap));
    if (heap) {
        heap->budget=byte_budget; heap->lineage=unique_id(&next_lineage);
        if (!heap->lineage) { free(heap); return NULL; }
    }
    return heap;
}
void MCObjectHeap_free(MCObjectHeap *heap) {
    if (!heap) return;
    release_objects(heap->objects); free(heap->roots); free(heap);
}
MCObject *MCObjectHeap_alloc(MCObjectHeap *heap,size_t bytes,const MCObjectClass *klass) {
    if (!heap) return NULL;
    if (heap->failed || heap->adopted || !klass || bytes<sizeof(MCObject) ||
        bytes>SIZE_MAX-sizeof(ObjectEntry) || bytes>heap->budget-heap->bytes) {
        heap->failed=true; return NULL;
    }
    ObjectEntry *storage=calloc(1,sizeof(ObjectEntry)+bytes);
    if (!storage) { heap->failed=true; return NULL; }
    uint64_t identity=unique_id(&next_object);
    if (!identity) { free(storage); heap->failed=true; return NULL; }
    uint32_t hash=(uint32_t)identity ^ (uint32_t)(identity>>32);
    hash^=hash>>16; hash*=UINT32_C(0x7feb352d); hash^=hash>>15;
    hash*=UINT32_C(0x846ca68b); hash^=hash>>16;
    storage->identity_hash=hash;
    storage->bytes=bytes; storage->next=heap->objects; heap->objects=storage;
    heap->bytes+=bytes; ++heap->count; ++heap->revision;
    MCObject *result=(MCObject *)storage->data;
    result->heap=heap; result->klass=klass; return result;
}
size_t MCObjectHeap_objectSize(const MCObject *object) { return object ? entry(object)->bytes : 0; }
int32_t MCObjectHeap_identityHashCode(const MCObject *object) {
    uint32_t hash=object ? entry(object)->identity_hash : 0;
    return hash<=INT32_MAX ? (int32_t)hash : -1-(int32_t)(UINT32_MAX-hash);
}
MCObject *MCObjectHeap_plainClone(MCObjectHeap *destination,const MCObject *source) {
    if (!source) return NULL;
    size_t bytes=MCObjectHeap_objectSize(source);
    MCObject *copy=MCObjectHeap_alloc(destination,bytes,source->klass);
    if (copy) {
        memcpy(copy,source,bytes); copy->heap=destination;
        entry(copy)->identity_hash=entry(source)->identity_hash;
    }
    return copy;
}
bool MCObjectHeap_failed(const MCObjectHeap *heap) { return !heap || heap->failed; }
void MCObjectHeap_fail(MCObjectHeap *heap) { if (heap) heap->failed=true; }
size_t MCObjectHeap_liveObjects(const MCObjectHeap *heap) { return heap ? heap->count : 0; }
size_t MCObjectHeap_liveBytes(const MCObjectHeap *heap) { return heap ? heap->bytes : 0; }
bool MCObjectHeap_hasBorrowers(const MCObjectHeap *heap) { return heap && heap->scopes!=0; }
void MCObjectHeap_touch(MCObjectHeap *heap) { if (heap) ++heap->revision; }
MCObject *MCObjectHeap_findObject(MCObjectHeap *heap,const MCObjectClass *klass,
    bool (*predicate)(const MCObject *,void *),void *context) {
    if (!heap || heap->adopted || !klass || !predicate) return NULL;
    for (ObjectEntry *storage=heap->objects;storage;storage=storage->next) {
        MCObject *object=(MCObject *)storage->data;
        if (object->klass==klass && predicate(object,context)) return object;
    }
    return NULL;
}
static size_t root_index(const MCObjectHeap *heap,uint64_t id) {
    size_t low=0,high=heap->root_count;
    while (low<high) {
        size_t middle=low+(high-low)/2;
        if (heap->roots[middle].id<id) low=middle+1; else high=middle;
    }
    return low;
}
static RootEntry *find_root(MCObjectHeap *heap,uint64_t id) {
    if (!heap || !id) return NULL;
    size_t index=root_index(heap,id);
    return index<heap->root_count && heap->roots[index].id==id ? &heap->roots[index] : NULL;
}
bool MCObjectRoot_init(MCObjectRoot *root,MCObjectHeap *heap,MCObject *object) {
    if (!root || !heap) return false;
    if (heap->failed || heap->adopted || (object && object->heap!=heap)) {
        heap->failed=true; return false;
    }
    if (heap->root_count==heap->root_capacity) {
        size_t capacity=heap->root_capacity ? heap->root_capacity*2 : 16;
        if (capacity<heap->root_capacity || capacity>SIZE_MAX/sizeof(RootEntry) ||
            capacity>heap->budget/sizeof(RootEntry)) { heap->failed=true; return false; }
        RootEntry *roots=realloc(heap->roots,capacity*sizeof(*roots));
        if (!roots) { heap->failed=true; return false; }
        heap->roots=roots; heap->root_capacity=capacity;
    }
    uint64_t id=unique_id(&next_root);
    if (!id) { heap->failed=true; return false; }
    heap->roots[heap->root_count++]=(RootEntry){id,object};
    *root=(MCObjectRoot){heap,id}; ++heap->revision; return true;
}
MCObject *MCObjectRoot_get(const MCObjectRoot *root) {
    RootEntry *found=root ? find_root(root->heap,root->id) : NULL;
    return found ? found->object : NULL;
}
bool MCObjectRoot_set(MCObjectRoot *root,MCObject *object) {
    RootEntry *found=root ? find_root(root->heap,root->id) : NULL;
    if (!found) return false;
    if (object && object->heap!=root->heap) { root->heap->failed=true; return false; }
    found->object=object; ++root->heap->revision; return true;
}
void MCObjectRoot_drop(MCObjectRoot *root) {
    if (!root) return;
    MCObjectHeap *heap=root->heap;
    if (heap && root->id) {
        size_t index=root_index(heap,root->id);
        if (index<heap->root_count && heap->roots[index].id==root->id) {
            memmove(heap->roots+index,heap->roots+index+1,(heap->root_count-index-1)*sizeof(RootEntry));
            --heap->root_count; ++heap->revision;
        }
    }
    *root=(MCObjectRoot){0};
}
bool MCObjectRoot_rebind(MCObjectRoot *destination,MCObjectHeap *heap,const MCObjectRoot *source) {
    if (!destination || !heap || !source || !source->heap ||
        source->heap->lineage!=heap->lineage || !find_root(heap,source->id)) return false;
    MCObjectHeap *source_owner=source->heap->adopted ? source->heap->adopted : source->heap;
    if (!find_root(source_owner,source->id)) return false;
    *destination=(MCObjectRoot){heap,source->id}; return true;
}
bool MCObjectRootScope_begin(MCObjectRootScope *scope,MCObjectHeap *heap) {
    if (!scope || scope->active || !heap || heap->failed || heap->adopted || heap->scopes==SIZE_MAX) return false;
    *scope=(MCObjectRootScope){heap,true}; ++heap->scopes; return true;
}
bool MCObjectRootScope_pin(MCObjectRootScope *scope,MCObject *object) {
    if (!scope || !scope->active) return false;
    if (object && object->heap!=scope->heap) { scope->heap->failed=true; return false; }
    return true;
}
void MCObjectRootScope_end(MCObjectRootScope *scope) {
    if (!scope || !scope->active) return;
    --scope->heap->scopes;
    /* Original public fields may be modified directly inside a borrow scope. */
    ++scope->heap->revision; *scope=(MCObjectRootScope){0};
}
bool MCObjectReadScope_begin(MCObjectReadScope *scope,MCObjectHeap *heap) {
    if (!scope || scope->active || !heap || heap->failed || heap->adopted || heap->scopes==SIZE_MAX) return false;
    *scope=(MCObjectReadScope){heap,true}; ++heap->scopes; return true;
}
void MCObjectReadScope_end(MCObjectReadScope *scope) {
    if (!scope || !scope->active) return;
    --scope->heap->scopes; *scope=(MCObjectReadScope){0};
}
typedef struct { MCObjectHeap *heap; MCObject **queue; size_t count,capacity; bool failed; } Marker;
static MCObject *mark_child(MCObject *child,void *context) {
    Marker *marker=context;
    if (!child || marker->failed) return child;
    if (child->heap!=marker->heap) { marker->failed=true; return child; }
    ObjectEntry *storage=entry(child);
    if (!storage->marked) {
        if (marker->count==marker->capacity) { marker->failed=true; return child; }
        storage->marked=true; marker->queue[marker->count++]=child;
    }
    return child;
}
bool MCObjectHeap_collect(MCObjectHeap *heap) {
    if (!heap || heap->scopes || heap->failed || heap->adopted) return false;
    if (heap->count>SIZE_MAX/sizeof(MCObject *)) { heap->failed=true; return false; }
    MCObject **queue=heap->count ? malloc(heap->count*sizeof(*queue)) : NULL;
    if (heap->count && !queue) { heap->failed=true; return false; }
    for (ObjectEntry *value=heap->objects;value;value=value->next) value->marked=false;
    Marker marker={heap,queue,0,heap->count,false};
    for (size_t i=0;i<heap->root_count;i++) mark_child(heap->roots[i].object,&marker);
    for (size_t i=0;i<marker.count && !marker.failed && !heap->failed;i++) {
        MCObject *object=queue[i];
        if (object->klass->trace) object->klass->trace(object,mark_child,&marker);
    }
    free(queue);
    if (marker.failed || heap->failed) { heap->failed=true; return false; }
    ObjectEntry **next=&heap->objects;
    while (*next) {
        ObjectEntry *value=*next;
        if (value->marked) { next=&value->next; continue; }
        *next=value->next; heap->bytes-=value->bytes; --heap->count;
        MCObject *object=(MCObject *)value->data;
        if (object->klass->destroy) object->klass->destroy(object);
        free(value);
    }
    ++heap->revision; return true;
}
typedef struct { const MCObject *source; MCObject *copy; } Memo;
typedef struct {
    const MCObjectHeap *source;
    MCObjectHeap *destination;
    Memo *memo;
    MCObject **queue;
    size_t capacity,count,max_count;
    bool failed;
} Cloner;
static size_t pointer_hash(const MCObject *object) {
    uintptr_t value=(uintptr_t)object;
    value^=value>>16; value*=UINT32_C(0x7feb352d); value^=value>>15;
    return (size_t)value;
}
static MCObject *clone_child(MCObject *child,void *context) {
    Cloner *cloner=context;
    if (!child || cloner->failed) return NULL;
    if (child->heap!=cloner->source) { cloner->failed=true; return NULL; }
    size_t slot=pointer_hash(child)&(cloner->capacity-1);
    while (cloner->memo[slot].source && cloner->memo[slot].source!=child)
        slot=(slot+1)&(cloner->capacity-1);
    if (cloner->memo[slot].source) return cloner->memo[slot].copy;
    if (cloner->count==cloner->max_count) { cloner->failed=true; return NULL; }
    MCObject *copy=child->klass->shallow_clone ?
        child->klass->shallow_clone(cloner->destination,child) :
        MCObjectHeap_plainClone(cloner->destination,child);
    if (!copy || copy->heap!=cloner->destination || copy->klass!=child->klass) {
        cloner->failed=true; return NULL;
    }
    entry(copy)->identity_hash=entry(child)->identity_hash;
    cloner->memo[slot]=(Memo){child,copy}; cloner->queue[cloner->count++]=copy; return copy;
}
MCObjectHeap *MCObjectHeap_clone(const MCObjectHeap *source) {
    if (!source || source->failed || source->adopted || source->scopes || source->count>SIZE_MAX/2) return NULL;
    size_t capacity=16;
    while (capacity<source->count*2) {
        if (capacity>SIZE_MAX/2) return NULL;
        capacity*=2;
    }
    if (capacity>SIZE_MAX/sizeof(Memo) || source->count>SIZE_MAX/sizeof(MCObject *)) return NULL;
    MCObjectHeap *copy=MCObjectHeap_new(source->budget);
    if (!copy) return NULL;
    Cloner cloner={source,copy,NULL,NULL,capacity,0,source->count,false};
    cloner.memo=calloc(capacity,sizeof(Memo));
    cloner.queue=source->count ? malloc(source->count*sizeof(MCObject *)) : NULL;
    if (!cloner.memo || (source->count && !cloner.queue)) goto failed;
    if (source->root_count) {
        if (source->root_count>SIZE_MAX/sizeof(RootEntry)) goto failed;
        copy->roots=malloc(source->root_count*sizeof(RootEntry));
        if (!copy->roots) goto failed;
        copy->root_count=copy->root_capacity=source->root_count;
        for (size_t i=0;i<source->root_count;i++) {
            copy->roots[i]=(RootEntry){source->roots[i].id,clone_child(source->roots[i].object,&cloner)};
        }
    }
    for (size_t i=0;i<cloner.count && !cloner.failed;i++) {
        MCObject *object=cloner.queue[i];
        if (object->klass->trace) object->klass->trace(object,clone_child,&cloner);
    }
    if (cloner.failed || copy->failed) goto failed;
    copy->lineage=source->lineage;
    copy->parent=source; copy->parent_revision=source->revision;
    free(cloner.memo); free(cloner.queue); return copy;
failed:
    free(cloner.memo); free(cloner.queue); MCObjectHeap_free(copy); return NULL;
}
bool MCObjectHeap_canAdopt(const MCObjectHeap *target,const MCObjectHeap *working) {
    return target && working && target!=working && !target->scopes && !working->scopes &&
        !target->failed && !working->failed && !target->adopted && !working->adopted && working->parent==target &&
        working->parent_revision==target->revision && working->lineage==target->lineage;
}
bool MCObjectHeap_adopt(MCObjectHeap *target,MCObjectHeap *working) {
    if (!MCObjectHeap_canAdopt(target,working)) return false;
    ObjectEntry *old_objects=target->objects; RootEntry *old_roots=target->roots;
    target->objects=working->objects; target->bytes=working->bytes; target->count=working->count;
    target->roots=working->roots; target->root_count=working->root_count;
    target->root_capacity=working->root_capacity;
    ++target->revision;
    for (ObjectEntry *value=target->objects;value;value=value->next)
        ((MCObject *)value->data)->heap=target;
    working->objects=NULL; working->bytes=working->count=0;
    working->roots=NULL; working->root_count=working->root_capacity=0; working->parent=NULL;
    working->adopted=target;
    release_objects(old_objects); free(old_roots); return true;
}
