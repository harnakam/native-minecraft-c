#include "util/NativeLinkedHashMap.h"
#include "nbt/NBTInternal.h"
#include <limits.h>
#include <string.h>

typedef struct LinkedEntry {
    MCObject object;
    MCObject *key, *value;
    uint32_t hash;
    struct LinkedEntry *bucketNext, *before, *after;
} LinkedEntry;
typedef struct LinkedTable {
    MCObject object;
    int32_t capacity;
    LinkedEntry *buckets[];
} LinkedTable;
struct NativeLinkedHashMap {
    MCObject object;
    LinkedTable *table;
    LinkedEntry *head, *tail;
    int32_t size, threshold;
    uint32_t modCount;
    NativeLinkedHashMapView *keys, *values;
};
struct NativeLinkedHashMapView {
    MCObject object;
    NativeLinkedHashMap *map;
    bool values;
};
struct NativeLinkedHashMapIterator {
    MCObject object;
    NativeLinkedHashMap *map;
    LinkedEntry *next, *current;
    uint32_t expectedModCount;
    bool values;
};
static const MCObjectClass mapClass, tableClass, entryClass, viewClass, iteratorClass;
static bool fail(MCObjectHeap *heap) {
    MCObjectHeap_fail(heap);
    return false;
}
static bool string_valid(MCObjectHeap *heap, const MCObject *key) {
    if (!key)
        return true;
    if (key->heap != heap || !NBTString_isInstance(key) ||
        MCObjectHeap_objectSize(key) < sizeof(NBTString))
        return fail(heap);
    const NBTString *s = (const NBTString *)key;
    if (s->length > (MCObjectHeap_objectSize(key) - sizeof(*s)) / sizeof(uint16_t))
        return fail(heap);
    return true;
}
static bool table_valid(const LinkedTable *t, MCObjectHeap *heap) {
    if (!t || t->object.heap != heap || t->object.klass != &tableClass ||
        MCObjectHeap_objectSize((const MCObject *)t) < sizeof(*t))
        return fail(heap);
    if (t->capacity <= 0 || (t->capacity & (t->capacity - 1)) != 0 ||
        (size_t)t->capacity >
            (MCObjectHeap_objectSize((const MCObject *)t) - sizeof(*t)) / sizeof(*t->buckets))
        return fail(heap);
    return true;
}
static bool entry_valid(const LinkedEntry *e, MCObjectHeap *heap) {
    if (!e || e->object.heap != heap || e->object.klass != &entryClass ||
        MCObjectHeap_objectSize((const MCObject *)e) < sizeof(*e))
        return fail(heap);
    return true;
}
bool NativeLinkedHashMap_isInstance(const MCObject *o) {
    return o && o->klass == &mapClass && MCObjectHeap_objectSize(o) >= sizeof(NativeLinkedHashMap);
}
bool NativeLinkedHashMapView_isInstance(const MCObject *o) {
    return o && o->klass == &viewClass &&
           MCObjectHeap_objectSize(o) >= sizeof(NativeLinkedHashMapView);
}
bool NativeLinkedHashMapIterator_isInstance(const MCObject *o) {
    return o && o->klass == &iteratorClass &&
           MCObjectHeap_objectSize(o) >= sizeof(NativeLinkedHashMapIterator);
}
static bool map_valid(const NativeLinkedHashMap *m) {
    MCObjectHeap *heap = m ? m->object.heap : NULL;
    if (!NativeLinkedHashMap_isInstance((const MCObject *)m) || MCObjectHeap_failed(heap) ||
        m->size < 0)
        return fail(heap);
    if (m->table && !table_valid(m->table, heap))
        return false;
    if (m->size == 0)
        return !m->head && !m->tail ? true : fail(heap);
    if (!m->table || !entry_valid(m->head, heap) || !entry_valid(m->tail, heap))
        return fail(heap);
    return !m->head->before && !m->tail->after ? true : fail(heap);
}
static bool view_valid(const NativeLinkedHashMapView *v) {
    MCObjectHeap *heap = v ? v->object.heap : NULL;
    return NativeLinkedHashMapView_isInstance((const MCObject *)v) && v->map &&
                   v->map->object.heap == heap && map_valid(v->map)
               ? true
               : fail(heap);
}
static bool iterator_valid(const NativeLinkedHashMapIterator *it) {
    MCObjectHeap *heap = it ? it->object.heap : NULL;
    return NativeLinkedHashMapIterator_isInstance((const MCObject *)it) && it->map &&
                   it->map->object.heap == heap && map_valid(it->map)
               ? true
               : fail(heap);
}
static void trace_entry(MCObject *o, MCObjectVisitor visit, void *context) {
    if (MCObjectHeap_objectSize(o) < sizeof(LinkedEntry)) {
        fail(o->heap);
        return;
    }
    LinkedEntry *e = (LinkedEntry *)o;
    e->key = visit(e->key, context);
    e->value = visit(e->value, context);
    e->bucketNext = (LinkedEntry *)visit((MCObject *)e->bucketNext, context);
    e->before = (LinkedEntry *)visit((MCObject *)e->before, context);
    e->after = (LinkedEntry *)visit((MCObject *)e->after, context);
}
static void trace_table(MCObject *o, MCObjectVisitor visit, void *context) {
    LinkedTable *t = (LinkedTable *)o;
    if (!table_valid(t, o->heap))
        return;
    for (int32_t i = 0; i < t->capacity; i++)
        t->buckets[i] = (LinkedEntry *)visit((MCObject *)t->buckets[i], context);
}
static void trace_map(MCObject *o, MCObjectVisitor visit, void *context) {
    if (MCObjectHeap_objectSize(o) < sizeof(NativeLinkedHashMap)) {
        fail(o->heap);
        return;
    }
    NativeLinkedHashMap *m = (NativeLinkedHashMap *)o;
    m->table = (LinkedTable *)visit((MCObject *)m->table, context);
    m->head = (LinkedEntry *)visit((MCObject *)m->head, context);
    m->tail = (LinkedEntry *)visit((MCObject *)m->tail, context);
    m->keys = (NativeLinkedHashMapView *)visit((MCObject *)m->keys, context);
    m->values = (NativeLinkedHashMapView *)visit((MCObject *)m->values, context);
}
static void trace_view(MCObject *o, MCObjectVisitor visit, void *context) {
    if (MCObjectHeap_objectSize(o) < sizeof(NativeLinkedHashMapView)) {
        fail(o->heap);
        return;
    }
    NativeLinkedHashMapView *v = (NativeLinkedHashMapView *)o;
    v->map = (NativeLinkedHashMap *)visit((MCObject *)v->map, context);
}
static void trace_iterator(MCObject *o, MCObjectVisitor visit, void *context) {
    if (MCObjectHeap_objectSize(o) < sizeof(NativeLinkedHashMapIterator)) {
        fail(o->heap);
        return;
    }
    NativeLinkedHashMapIterator *it = (NativeLinkedHashMapIterator *)o;
    it->map = (NativeLinkedHashMap *)visit((MCObject *)it->map, context);
    it->next = (LinkedEntry *)visit((MCObject *)it->next, context);
    it->current = (LinkedEntry *)visit((MCObject *)it->current, context);
}
static const MCObjectClass mapClass = {"native.LinkedHashMap", MCObjectHeap_plainClone, trace_map,
                                       NULL};
static const MCObjectClass tableClass = {"native.LinkedHashMap.Table", MCObjectHeap_plainClone,
                                         trace_table, NULL};
static const MCObjectClass entryClass = {"native.LinkedHashMap.Entry", MCObjectHeap_plainClone,
                                         trace_entry, NULL};
static const MCObjectClass viewClass = {"native.LinkedHashMap.View", MCObjectHeap_plainClone,
                                        trace_view, NULL};
static const MCObjectClass iteratorClass = {"native.LinkedHashMap.Iterator",
                                            MCObjectHeap_plainClone, trace_iterator, NULL};

static bool begin(NativeLinkedHashMap *m, MCObjectRootScope *scope, MCObject *key,
                  MCObject *value) {
    if (!map_valid(m) || !MCObjectRootScope_begin(scope, m->object.heap))
        return false;
    if (MCObjectRootScope_pin(scope, (MCObject *)m) && MCObjectRootScope_pin(scope, key) &&
        MCObjectRootScope_pin(scope, value) && string_valid(m->object.heap, key))
        return true;
    MCObjectRootScope_end(scope);
    return false;
}
static uint32_t key_hash(const MCObject *key) {
    uint32_t h = (uint32_t)NBTString_hashCode((const NBTString *)key);
    return h ^ (h >> 16);
}
static bool key_equal(const MCObject *query, const MCObject *stored) {
    return query == stored ||
           (query && stored &&
            NBTString_equals((const NBTString *)query, (const NBTString *)stored));
}
static LinkedEntry *find(NativeLinkedHashMap *m, MCObject *key, uint32_t hash) {
    if (!m->table)
        return NULL;
    int32_t remaining = m->size;
    for (LinkedEntry *e = m->table->buckets[hash & (uint32_t)(m->table->capacity - 1)]; e;
         e = e->bucketNext) {
        if (remaining-- <= 0 || !entry_valid(e, m->object.heap) ||
            !string_valid(m->object.heap, e->key)) {
            fail(m->object.heap);
            return NULL;
        }
        if (e->hash == hash && key_equal(key, e->key))
            return e;
    }
    return NULL;
}
static bool resize(NativeLinkedHashMap *m) {
    int32_t capacity = m->table ? m->table->capacity : 0;
    if (capacity >= 1 << 30) {
        m->threshold = INT32_MAX;
        return true;
    }
    capacity = capacity ? capacity * 2 : 16;
    if ((size_t)capacity > (SIZE_MAX - sizeof(LinkedTable)) / sizeof(LinkedEntry *))
        return fail(m->object.heap);
    LinkedTable *table = (LinkedTable *)MCObjectHeap_alloc(
        m->object.heap, sizeof(*table) + (size_t)capacity * sizeof(*table->buckets), &tableClass);
    if (!table)
        return false;
    table->capacity = capacity;
    /* Rehash the same entries in encounter order. Chain traversal is bounded
       by the explicitly supported list-bin size; encounter links are untouched. */
    int32_t remaining = m->size;
    for (LinkedEntry *e = m->head; e; e = e->after) {
        if (remaining-- <= 0 || !entry_valid(e, m->object.heap))
            return fail(m->object.heap);
        uint32_t index = e->hash & (uint32_t)(capacity - 1);
        e->bucketNext = NULL;
        LinkedEntry **slot = &table->buckets[index];
        int32_t budget = m->size;
        while (*slot) {
            if (budget-- <= 0 || !entry_valid(*slot, m->object.heap))
                return fail(m->object.heap);
            slot = &(*slot)->bucketNext;
        }
        *slot = e;
    }
    if (remaining != 0)
        return fail(m->object.heap);
    m->table = table;
    m->threshold = capacity - (capacity >> 2);
    MCObjectHeap_touch(m->object.heap);
    return true;
}
NativeLinkedHashMap *NativeLinkedHashMap_new(MCObjectHeap *heap) {
    return (NativeLinkedHashMap *)MCObjectHeap_alloc(heap, sizeof(NativeLinkedHashMap), &mapClass);
}
int32_t NativeLinkedHashMap_size(const NativeLinkedHashMap *m) {
    return map_valid(m) ? m->size : -1;
}
MCObject *NativeLinkedHashMap_get(NativeLinkedHashMap *m, MCObject *key) {
    MCObjectRootScope scope = {0};
    if (!begin(m, &scope, key, NULL))
        return NULL;
    LinkedEntry *e = find(m, key, key_hash(key));
    MCObject *value = e ? e->value : NULL;
    if (value && value->heap != m->object.heap) {
        fail(m->object.heap);
        value = NULL;
    }
    MCObjectRootScope_end(&scope);
    return MCObjectHeap_failed(m->object.heap) ? NULL : value;
}
bool NativeLinkedHashMap_containsKey(NativeLinkedHashMap *m, MCObject *key) {
    MCObjectRootScope scope = {0};
    if (!begin(m, &scope, key, NULL))
        return false;
    bool found = find(m, key, key_hash(key)) != NULL;
    MCObjectRootScope_end(&scope);
    return found && !MCObjectHeap_failed(m->object.heap);
}
bool NativeLinkedHashMap_putWithPrevious(NativeLinkedHashMap *m, MCObject *key, MCObject *value,
                                         MCObject **previous) {
    if (!previous)
        return fail(m ? m->object.heap : NULL);
    MCObjectRootScope scope = {0};
    if (!begin(m, &scope, key, value))
        return false;
    bool ok = false;
    MCObject *old = NULL;
    uint32_t hash = key_hash(key);
    LinkedEntry *existing = find(m, key, hash);
    if (MCObjectHeap_failed(m->object.heap))
        goto done;
    if (existing) {
        old = existing->value;
        existing->value = value;
        MCObjectHeap_touch(m->object.heap);
        ok = true;
        goto done;
    }
    if (!m->table && !resize(m))
        goto done;
    uint32_t index = hash & (uint32_t)(m->table->capacity - 1);
    int32_t count = 0;
    LinkedEntry *last = NULL;
    for (LinkedEntry *e = m->table->buckets[index]; e; e = e->bucketNext) {
        if (count >= m->size || !entry_valid(e, m->object.heap)) {
            fail(m->object.heap);
            goto done;
        }
        ++count;
        last = e;
    }
    if ((count >= 8 && m->table->capacity >= 64) || m->size == INT32_MAX) {
        fail(m->object.heap);
        goto done;
    }
    LinkedEntry *e = (LinkedEntry *)MCObjectHeap_alloc(m->object.heap, sizeof(*e), &entryClass);
    if (!e)
        goto done;
    e->key = key;
    e->value = value;
    e->hash = hash;
    e->before = m->tail;
    if (last)
        last->bucketNext = e;
    else
        m->table->buckets[index] = e;
    if (m->tail)
        m->tail->after = e;
    else
        m->head = e;
    m->tail = e;
    ++m->size;
    ++m->modCount;
    MCObjectHeap_touch(m->object.heap);
    if (count >= 8 && !resize(m))
        goto done;
    if (m->size > m->threshold && !resize(m))
        goto done;
    ok = true;
done:
    MCObjectRootScope_end(&scope);
    ok = ok && !MCObjectHeap_failed(m->object.heap);
    if (ok)
        *previous = old;
    return ok;
}
bool NativeLinkedHashMap_put(NativeLinkedHashMap *m, MCObject *key, MCObject *value) {
    MCObject *previous = NULL;
    return NativeLinkedHashMap_putWithPrevious(m, key, value, &previous);
}
MCObject *NativeLinkedHashMap_remove(NativeLinkedHashMap *m, MCObject *key) {
    MCObjectRootScope scope = {0};
    if (!begin(m, &scope, key, NULL))
        return NULL;
    MCObject *out = NULL;
    uint32_t hash = key_hash(key);
    LinkedEntry *target = find(m, key, hash);
    if (!target || MCObjectHeap_failed(m->object.heap))
        goto done;
    uint32_t index = hash & (uint32_t)(m->table->capacity - 1);
    LinkedEntry **link = &m->table->buckets[index];
    int32_t remaining = m->size;
    while (*link != target) {
        if (remaining-- <= 0 || !entry_valid(*link, m->object.heap)) {
            fail(m->object.heap);
            goto done;
        }
        link = &(*link)->bucketNext;
    }
    LinkedEntry *before = target->before, *after = target->after;
    if ((before && !entry_valid(before, m->object.heap)) ||
        (after && !entry_valid(after, m->object.heap)))
        goto done;
    out = target->value;
    if (out && out->heap != m->object.heap) {
        fail(m->object.heap);
        out = NULL;
        goto done;
    }
    *link = target->bucketNext;
    if (before)
        before->after = after;
    else
        m->head = after;
    if (after)
        after->before = before;
    else
        m->tail = before;
    target->before = target->after = NULL;
    --m->size;
    ++m->modCount;
    MCObjectHeap_touch(m->object.heap);
done:
    MCObjectRootScope_end(&scope);
    return MCObjectHeap_failed(m->object.heap) ? NULL : out;
}
bool NativeLinkedHashMap_clear(NativeLinkedHashMap *m) {
    if (!map_valid(m))
        return false;
    ++m->modCount;
    if (m->table && m->size)
        memset(m->table->buckets, 0, (size_t)m->table->capacity * sizeof(*m->table->buckets));
    m->size = 0;
    m->head = m->tail = NULL;
    MCObjectHeap_touch(m->object.heap);
    return true;
}
static NativeLinkedHashMapView *make_view(NativeLinkedHashMap *m, bool values) {
    MCObjectRootScope scope = {0};
    if (!begin(m, &scope, NULL, NULL))
        return NULL;
    NativeLinkedHashMapView **slot = values ? &m->values : &m->keys;
    if (!*slot) {
        *slot = (NativeLinkedHashMapView *)MCObjectHeap_alloc(m->object.heap, sizeof(**slot),
                                                              &viewClass);
        if (*slot) {
            (*slot)->map = m;
            (*slot)->values = values;
            MCObjectHeap_touch(m->object.heap);
        }
    }
    NativeLinkedHashMapView *view = *slot;
    if (view && (!NativeLinkedHashMapView_isInstance((MCObject *)view) ||
                 view->object.heap != m->object.heap || view->map != m || view->values != values)) {
        fail(m->object.heap);
        view = NULL;
    }
    MCObjectRootScope_end(&scope);
    return view;
}
NativeLinkedHashMapView *NativeLinkedHashMap_values(NativeLinkedHashMap *m) {
    return make_view(m, true);
}
NativeLinkedHashMapView *NativeLinkedHashMap_keys(NativeLinkedHashMap *m) {
    return make_view(m, false);
}
int32_t NativeLinkedHashMapView_size(NativeLinkedHashMapView *v) {
    return view_valid(v) ? v->map->size : -1;
}
bool NativeLinkedHashMapView_clear(NativeLinkedHashMapView *v) {
    return view_valid(v) && NativeLinkedHashMap_clear(v->map);
}
NativeLinkedHashMapIterator *NativeLinkedHashMapIterator_fromView(NativeLinkedHashMapView *v) {
    if (!view_valid(v))
        return NULL;
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, v->object.heap))
        return NULL;
    NativeLinkedHashMapIterator *it = NULL;
    if (MCObjectRootScope_pin(&scope, (MCObject *)v)) {
        it = (NativeLinkedHashMapIterator *)MCObjectHeap_alloc(v->object.heap, sizeof(*it),
                                                               &iteratorClass);
        if (it) {
            it->map = v->map;
            it->next = v->map->head;
            it->expectedModCount = v->map->modCount;
            it->values = v->values;
        }
    }
    MCObjectRootScope_end(&scope);
    return it;
}
bool NativeLinkedHashMapIterator_hasNext(NativeLinkedHashMapIterator *it) {
    return iterator_valid(it) && it->next != NULL;
}
NativeLinkedHashMapIteratorResult
NativeLinkedHashMapIterator_nextSource(NativeLinkedHashMapIterator *it, MCObject **out) {
    if (!iterator_valid(it) || !out) {
        fail(it ? it->object.heap : NULL);
        return NATIVE_LINKED_ITERATOR_FAILURE;
    }
    LinkedEntry *e = it->next;
    if (it->expectedModCount != it->map->modCount || !e)
        return NATIVE_LINKED_ITERATOR_EXCEPTION;
    if (!entry_valid(e, it->object.heap))
        return NATIVE_LINKED_ITERATOR_FAILURE;
    MCObject *value = it->values ? e->value : e->key;
    if ((value && value->heap != it->object.heap) ||
        (!it->values && !string_valid(it->object.heap, value))) {
        fail(it->object.heap);
        return NATIVE_LINKED_ITERATOR_FAILURE;
    }
    it->current = e;
    it->next = e->after;
    *out = value;
    MCObjectHeap_touch(it->object.heap);
    return NATIVE_LINKED_ITERATOR_OK;
}
bool NativeLinkedHashMapIterator_next(NativeLinkedHashMapIterator *it, MCObject **out) {
    NativeLinkedHashMapIteratorResult result = NativeLinkedHashMapIterator_nextSource(it, out);
    if (result == NATIVE_LINKED_ITERATOR_EXCEPTION)
        fail(it ? it->object.heap : NULL);
    return result == NATIVE_LINKED_ITERATOR_OK;
}
