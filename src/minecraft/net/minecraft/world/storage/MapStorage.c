#include "world/storage/MapStorage.h"
#include "world/storage/SaveDataMemoryStorage.h"
#include <limits.h>
#include <string.h>
static bool source_save_counts(MapStorage *, MCObjectRootScope *);

typedef struct CounterEntry {
    MCObject object;
    NBTString *key;
    struct CounterEntry *next;
    uint32_t hash;
    int16_t value;
} CounterEntry;
typedef struct {
    MCObject object;
    int32_t capacity;
    CounterEntry *items[];
} CounterBuckets;
struct MapStorageIdCounts {
    MCObject object;
    CounterBuckets *table;
    int32_t size, threshold;
};
static bool fail(MCObjectHeap *h) {
    MCObjectHeap_fail(h);
    return false;
}
static bool capacity_valid(const CounterBuckets *b) {
    size_t bytes = b ? MCObjectHeap_objectSize((MCObject *)b) : 0;
    return b && bytes >= sizeof(*b) && b->capacity >= 16 && b->capacity <= 1073741824 &&
           ((uint32_t)b->capacity & ((uint32_t)b->capacity - 1u)) == 0 &&
           (size_t)b->capacity <= (bytes - sizeof(*b)) / sizeof(b->items[0]);
}
static void entry_trace(MCObject *o, MCObjectVisitor v, void *c) {
    CounterEntry *e = (CounterEntry *)o;
    e->key = (NBTString *)v((MCObject *)e->key, c);
    e->next = (CounterEntry *)v((MCObject *)e->next, c);
}
static const MCObjectClass entryClass = {"native.MapStorage.HashMap.ShortEntry",
                                         MCObjectHeap_plainClone, entry_trace, NULL};
static void bucket_trace(MCObject *o, MCObjectVisitor v, void *c) {
    CounterBuckets *b = (CounterBuckets *)o;
    if (!capacity_valid(b)) {
        MCObjectHeap_fail(o->heap);
        return;
    }
    for (int32_t i = 0; i < b->capacity; i++)
        b->items[i] = (CounterEntry *)v((MCObject *)b->items[i], c);
}
static const MCObjectClass bucketClass = {"native.MapStorage.HashMap.Buckets",
                                          MCObjectHeap_plainClone, bucket_trace, NULL};
static void counter_trace(MCObject *o, MCObjectVisitor v, void *c) {
    MapStorageIdCounts *m = (MapStorageIdCounts *)o;
    m->table = (CounterBuckets *)v((MCObject *)m->table, c);
}
static const MCObjectClass counterClass = {"native.MapStorage.NullableStringShortMap",
                                           MCObjectHeap_plainClone, counter_trace, NULL};
void MapStorage_nativeTraceFields(MCObject *o, MCObjectVisitor v, void *c) {
    MapStorage *s = (MapStorage *)o;
    s->saveHandler = v(s->saveHandler, c);
    s->nativeContext = v(s->nativeContext, c);
    s->loadedDataMap = (NativeHashMap *)v((MCObject *)s->loadedDataMap, c);
    s->loadedDataList = (NativeReferenceList *)v((MCObject *)s->loadedDataList, c);
    s->idCounts = (MapStorageIdCounts *)v((MCObject *)s->idCounts, c);
}
static const MCObjectClass klass = {"net.minecraft.world.storage.MapStorage",
                                    MCObjectHeap_plainClone, MapStorage_nativeTraceFields, NULL};
bool MapStorage_isInstance(const MCObject *o) {
    return o && ((o->klass == &klass && MCObjectHeap_objectSize(o) >= sizeof(MapStorage)) ||
                 SaveDataMemoryStorage_isInstance(o));
}
static MapStorageIdCounts *valid_state(const MapStorage *s, bool allowFailed) {
    MCObjectHeap *h = s ? s->object.heap : NULL;
    if (!MapStorage_isInstance((MCObject *)s) || (!allowFailed && MCObjectHeap_failed(h)) ||
        !s->idCounts || s->idCounts->object.heap != h ||
        s->idCounts->object.klass != &counterClass ||
        MCObjectHeap_objectSize((MCObject *)s->idCounts) < sizeof(*s->idCounts) ||
        (s->saveHandler && s->saveHandler->heap != h) ||
        (s->nativeContext && s->nativeContext->heap != h)) {
        fail(h);
        return NULL;
    }
    MapStorageIdCounts *m = s->idCounts;
    if (m->size < 0 || (!m->table && m->size) ||
        (m->table && (m->table->object.heap != h || m->table->object.klass != &bucketClass ||
                      !capacity_valid(m->table)))) {
        fail(h);
        return NULL;
    }
    return m;
}
static MapStorageIdCounts *valid(const MapStorage *s) { return valid_state(s, false); }
static bool valid_key(MCObjectHeap *h, const NBTString *key) {
    return !key || (NBTString_isInstance((MCObject *)key) && ((MCObject *)key)->heap == h) ||
           fail(h);
}
static uint32_t hash(const NBTString *key) {
    uint32_t h = key ? (uint32_t)NBTString_hashCode(key) : 0;
    return h ^ (h >> 16);
}
static CounterEntry *find(MapStorageIdCounts *m, const NBTString *key) {
    if (!m->table)
        return NULL;
    uint32_t h = hash(key);
    CounterEntry *e = m->table->items[h & ((uint32_t)m->table->capacity - 1u)];
    int32_t visited = 0;
    while (e) {
        if (e->object.klass != &entryClass || e->object.heap != m->object.heap ||
            MCObjectHeap_objectSize((MCObject *)e) < sizeof(*e) || ++visited > m->size ||
            !valid_key(m->object.heap, e->key)) {
            fail(m->object.heap);
            return NULL;
        }
        if (h == e->hash && NBTString_equals(key, e->key))
            return e;
        e = e->next;
    }
    return NULL;
}
static bool resize(MapStorageIdCounts *m) {
    int32_t old = m->table ? m->table->capacity : 0;
    if (old == 1073741824) {
        m->threshold = INT32_MAX;
        return true;
    }
    int32_t cap = old ? old * 2 : 16;
    if ((size_t)cap > (SIZE_MAX - sizeof(CounterBuckets)) / sizeof(CounterEntry *))
        return fail(m->object.heap);
    CounterBuckets *b = (CounterBuckets *)MCObjectHeap_alloc(
        m->object.heap, sizeof(*b) + (size_t)cap * sizeof(b->items[0]), &bucketClass);
    if (!b)
        return false;
    b->capacity = cap;
    if (old)
        for (int32_t i = 0; i < old; i++) {
            CounterEntry *low = NULL, *high = NULL, *lowTail = NULL, *highTail = NULL,
                         *e = m->table->items[i];
            while (e) {
                CounterEntry *next = e->next;
                if (e->hash & (uint32_t)old) {
                    if (highTail)
                        highTail->next = e;
                    else
                        high = e;
                    highTail = e;
                } else {
                    if (lowTail)
                        lowTail->next = e;
                    else
                        low = e;
                    lowTail = e;
                }
                e = next;
            }
            if (lowTail)
                lowTail->next = NULL;
            if (highTail)
                highTail->next = NULL;
            b->items[i] = low;
            b->items[i + old] = high;
        }
    m->table = b;
    m->threshold = cap - cap / 4;
    MCObjectHeap_touch(m->object.heap);
    return true;
}
static bool put(MapStorageIdCounts *m, NBTString *key, int16_t value) {
    CounterEntry *e = find(m, key);
    if (MCObjectHeap_failed(m->object.heap))
        return false;
    if (e) {
        e->value = value;
        MCObjectHeap_touch(m->object.heap);
        return true;
    }
    if (!m->table && !resize(m))
        return false;
    if (m->size == INT32_MAX)
        return fail(m->object.heap);
    uint32_t h = hash(key), index = h & ((uint32_t)m->table->capacity - 1u);
    CounterEntry *tail = m->table->items[index];
    int32_t chain = 0;
    while (tail) {
        ++chain;
        if (!tail->next)
            break;
        tail = tail->next;
    }
    e = (CounterEntry *)MCObjectHeap_alloc(m->object.heap, sizeof(*e), &entryClass);
    if (!e)
        return false;
    e->key = key;
    e->hash = h;
    e->value = value;
    if (tail)
        tail->next = e;
    else
        m->table->items[index] = e;
    ++m->size;
    MCObjectHeap_touch(m->object.heap);
    /* The JDK requests treeification on adding the ninth chain node. Below64
       it resizes instead; at64 the unported TreeNode branch is a native error.
       Do not silently return a different key traversal for those inputs. */
    if (chain >= 8) {
        if (m->table->capacity >= 64)
            return fail(m->object.heap);
        if (!resize(m))
            return false;
    }
    return m->size <= m->threshold || resize(m);
}
bool MapStorage_nativeInitializeCounterProvider(MapStorage *s, MCObject *save, MCObject *context,
                                                const MapStorageDependencies *d) {
    MCObjectHeap *h = s ? s->object.heap : NULL;
    if (!MapStorage_isInstance((MCObject *)s) || (save && save->heap != h) ||
        (context && context->heap != h))
        return fail(h);
    MapStorageIdCounts *m = (MapStorageIdCounts *)MCObjectHeap_alloc(h, sizeof(*m), &counterClass);
    if (!m)
        return false;
    s->saveHandler = save;
    s->nativeContext = context;
    s->dependencies = d;
    s->idCounts = m;
    MCObjectHeap_touch(h);
    return true;
}
MapStorage *MapStorage_nativeNewCounterProvider(MCObjectHeap *h, MCObject *save, MCObject *context,
                                                const MapStorageDependencies *d) {
    if ((save && save->heap != h) || (context && context->heap != h)) {
        fail(h);
        return NULL;
    }
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h))
        return NULL;
    MapStorage *s = (MapStorage *)MCObjectHeap_alloc(h, sizeof(*s), &klass);
    bool ok = s && MapStorage_nativeInitializeCounterProvider(s, save, context, d);
    MCObjectRootScope_end(&scope);
    return ok ? s : NULL;
}
int32_t MapStorage_nativeIdCountSize(const MapStorage *s) {
    MapStorageIdCounts *m = valid(s);
    return m ? m->size : 0;
}
bool MapStorage_nativeIdCountEntryAt(const MapStorage *s, int32_t index, NBTString **key,
                                     int16_t *value) {
    MapStorageIdCounts *m = valid(s);
    if (!m || !key || !value || index < 0 || index >= m->size)
        return fail(s ? s->object.heap : NULL);
    int32_t at = 0;
    for (int32_t i = 0; i < m->table->capacity; i++)
        for (CounterEntry *e = m->table->items[i]; e; e = e->next)
            if (at++ == index) {
                *key = e->key;
                *value = e->value;
                return true;
            }
    return fail(s->object.heap);
}
bool MapStorage_nativeFindExactShort(const MapStorage *s, const NBTString *key, bool *present,
                                     int16_t *value) {
    MapStorageIdCounts *m = valid(s);
    if (!m || !present || !value || !valid_key(s->object.heap, key))
        return fail(s ? s->object.heap : NULL);
    CounterEntry *e = find(m, key);
    if (MCObjectHeap_failed(s->object.heap))
        return false;
    *present = e != NULL;
    if (e)
        *value = e->value;
    return true;
}
bool MapStorage_nativeImportExactShort(MapStorage *s, NBTString *key, int16_t value) {
    MapStorageIdCounts *m = valid(s);
    if (!m || !valid_key(s->object.heap, key))
        return false;
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, s->object.heap))
        return false;
    bool ok = put(m, key, value);
    MCObjectRootScope_end(&scope);
    return ok;
}
bool MapStorage_nativeClearIdCounts(MapStorage *s) {
    MapStorageIdCounts *m = valid(s);
    if (!m)
        return false;
    if (m->table)
        memset(m->table->items, 0, (size_t)m->table->capacity * sizeof(m->table->items[0]));
    m->size = 0;
    MCObjectHeap_touch(s->object.heap);
    return true;
}
NBTTagCompound *MapStorage_nativeSnapshotIdCounts(MapStorage *s) {
    MapStorageIdCounts *m = valid(s);
    if (!m)
        return NULL;
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, s->object.heap))
        return NULL;
    NBTTagCompound *tag = NBTTagCompound_new(s->object.heap);
    bool ok = tag != NULL;
    if (ok && m->table)
        for (int32_t i = 0; i < m->table->capacity && ok; i++)
            for (CounterEntry *e = m->table->items[i]; e && ok; e = e->next)
                ok = NBTTagCompound_setShort(tag, e->key, e->value);
    MCObjectRootScope_end(&scope);
    return ok ? tag : NULL;
}
bool MapStorage_nativeImportIdCounts(MapStorage *s, NBTTagCompound *tag, bool clearFirst) {
    MapStorageIdCounts *m = valid(s);
    MCObjectHeap *h = s ? s->object.heap : NULL;
    if (!m || !NBTTagCompound_isInstance((MCObject *)tag) || ((MCObject *)tag)->heap != h)
        return fail(h);
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h))
        return false;
    bool ok = !clearFirst || MapStorage_nativeClearIdCounts(s);
    NBTCompoundKeySet *keys = ok ? NBTTagCompound_getKeySet(tag) : NULL;
    ok = ok && keys;
    int32_t n = keys ? NBTCompoundKeySet_size(keys) : 0;
    for (int32_t i = 0; i < n && ok; i++) {
        NBTString *key = NBTCompoundKeySet_keyAt(keys, i);
        if (NBTTagCompound_getTagId(tag, key) == 2)
            ok = put(m, key, NBTTagCompound_getShort(tag, key));
    }
    MCObjectRootScope_end(&scope);
    return ok && !MCObjectHeap_failed(h);
}
static bool project_next(const MapStorage *s, int32_t *out, bool diagnostic) {
    MapStorageIdCounts *m = valid_state(s, diagnostic);
    if (!m || !out)
        return fail(s ? s->object.heap : NULL);
    /* Pure projection: no literal allocation, mutable scope or Source ID draw. */
    CounterEntry *e = NULL;
    uint32_t hashMap = 107868u;
    hashMap ^= hashMap >> 16;
    int32_t visited = 0;
    if (m->table)
        for (e = m->table->items[hashMap & ((uint32_t)m->table->capacity - 1u)]; e; e = e->next) {
            if (e->object.klass != &entryClass || e->object.heap != m->object.heap ||
                MCObjectHeap_objectSize((MCObject *)e) < sizeof(*e) || ++visited > m->size ||
                !valid_key(m->object.heap, e->key))
                return fail(s->object.heap);
            if (e->hash == hashMap && NBTString_equalsASCII(e->key, "map"))
                break;
        }
    *out = e ? (int32_t)(((uint32_t)(uint16_t)e->value + 1u) & 65535u) : 0;
    return true;
}
bool MapStorage_nativeGetMapNextProjection(const MapStorage *s, int32_t *out) {
    return project_next(s, out, false);
}
bool MapStorage_nativeGetMapNextProjectionDiagnostic(const MapStorage *s, int32_t *out) {
    return project_next(s, out, true);
}
static bool io_result(MapStorage *s, MapStorageIOResult result, bool *caught) {
    if (s->nativeContext && (s->nativeContext->heap != s->object.heap ||
                             MCObjectHeap_objectSize(s->nativeContext) < sizeof(MCObject)))
        return fail(s->object.heap);
    if (MCObjectHeap_failed(s->object.heap) || result == MAP_STORAGE_IO_FAILURE)
        return fail(s->object.heap);
    if (result == MAP_STORAGE_IO_EXCEPTION) {
        *caught = true;
        bool (*print)(MCObject *) = s->sourceDependencies
                                        ? s->sourceDependencies->printCaughtException
                                    : s->dependencies ? s->dependencies->printCaughtException
                                                      : NULL;
        if (!print || !print(s->nativeContext))
            return fail(s->object.heap);
        if (s->nativeContext && (s->nativeContext->heap != s->object.heap ||
                                 MCObjectHeap_objectSize(s->nativeContext) < sizeof(MCObject)))
            return fail(s->object.heap);
        return !MCObjectHeap_failed(s->object.heap);
    }
    return result == MAP_STORAGE_IO_OK || fail(s->object.heap);
}
static bool save_counts(MapStorage *s, MCObjectRootScope *scope) {
    const MapStorageDependencies *d = s->dependencies;
    bool caught = false;
    if (!d || !d->getMapFileFromName)
        return fail(s->object.heap);
    MCObject *save = s->saveHandler, *file = NULL, *stream = NULL;
    NBTString *name = NBTString_literalASCII(s->object.heap, "idcounts");
    if (!name)
        return false;
    if (!io_result(s, d->getMapFileFromName(s->nativeContext, save, name, &file), &caught))
        return false;
    if (caught || !file)
        return true;
    if (!MCObjectRootScope_pin(scope, file))
        return false;
    NBTTagCompound *tag = MapStorage_nativeSnapshotIdCounts(s);
    if (!tag)
        return false;
    if (!d->newDataOutputStream)
        return fail(s->object.heap);
    if (!io_result(s, d->newDataOutputStream(s->nativeContext, file, &stream), &caught))
        return false;
    if (caught)
        return true;
    if (!stream || !MCObjectRootScope_pin(scope, stream) || !d->writeRawNBT)
        return fail(s->object.heap);
    if (!io_result(s, d->writeRawNBT(s->nativeContext, tag, stream), &caught))
        return false;
    if (caught)
        return true; /* Source does not close after a write exception. */
    if (!d->closeOutputStream)
        return fail(s->object.heap);
    return io_result(s, d->closeOutputStream(s->nativeContext, stream), &caught);
}
bool MapStorage_getUniqueDataId(MapStorage *s, NBTString *key, int32_t *out) {
    if (SaveDataMemoryStorage_isInstance((MCObject *)s))
        return SaveDataMemoryStorage_getUniqueDataId((SaveDataMemoryStorage *)s, key, out);
    MapStorageIdCounts *m = valid(s);
    if (!m || !out || !valid_key(s->object.heap, key))
        return fail(s ? s->object.heap : NULL);
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, s->object.heap))
        return false;
    CounterEntry *entry = find(m, key);
    uint16_t bits = entry ? (uint16_t)((uint16_t)entry->value + 1u) : 0;
    int16_t local;
    memcpy(&local, &bits, sizeof(local));
    bool ok = !MCObjectHeap_failed(s->object.heap) && put(m, key, local);
    if (ok && s->saveHandler)
        ok = s->sourceDependencies ? source_save_counts(s, &scope) : save_counts(s, &scope);
    if (ok && !MCObjectHeap_failed(s->object.heap))
        *out = local;
    else
        ok = false;
    MCObjectRootScope_end(&scope);
    return ok;
}

/* The Source fields are evaluated at each expression below. Native lifetime
   scopes pin captured receivers/locals without turning them into new owners. */
static bool source_begin(MapStorage *s, MCObjectRootScope *scope) {
    MCObjectHeap *h = s ? s->object.heap : NULL;
    if (!MapStorage_isInstance((MCObject *)s) || MCObjectHeap_failed(h) ||
        (s->nativeContext && s->nativeContext->heap != h))
        return fail(h);
    if (!MCObjectRootScope_begin(scope, h) || !MCObjectRootScope_pin(scope, (MCObject *)s) ||
        !MCObjectRootScope_pin(scope, s->nativeContext)) {
        MCObjectRootScope_end(scope);
        return fail(h);
    }
    return true;
}
static bool source_ref(MapStorage *s, MCObjectRootScope *scope, MCObject *o) {
    return (!o || (o->heap == s->object.heap && MCObjectHeap_objectSize(o) >= sizeof(MCObject))) &&
                   MCObjectRootScope_pin(scope, o)
               ? true
               : fail(s->object.heap);
}
static bool source_saved(MapStorage *s, WorldSavedData *data) {
    return !data ||
           (WorldSavedData_isInstance((MCObject *)data) && data->object.heap == s->object.heap) ||
           fail(s->object.heap);
}
static NativeHashMap *source_map(MapStorage *s, MCObjectRootScope *scope) {
    NativeHashMap *map = s->loadedDataMap;
    if (!NativeHashMap_isInstance((MCObject *)map) || !source_ref(s, scope, (MCObject *)map)) {
        fail(s->object.heap);
        return NULL;
    }
    return map;
}
static NativeReferenceList *source_list(MapStorage *s, MCObjectRootScope *scope) {
    NativeReferenceList *list = s->loadedDataList;
    if (!NativeReferenceList_isInstance((MCObject *)list) ||
        !source_ref(s, scope, (MCObject *)list)) {
        fail(s->object.heap);
        return NULL;
    }
    return list;
}
static MapStorageIOResult handler_result(ISaveHandlerResult result) {
    switch (result) {
    case I_SAVE_HANDLER_OK:
        return MAP_STORAGE_IO_OK;
    case I_SAVE_HANDLER_EXCEPTION:
        return MAP_STORAGE_IO_EXCEPTION;
    default:
        return MAP_STORAGE_IO_FAILURE;
    }
}
static MapStorageIOResult saved_result(WorldSavedDataResult result) {
    switch (result) {
    case WORLD_SAVED_DATA_OK:
        return MAP_STORAGE_IO_OK;
    case WORLD_SAVED_DATA_EXCEPTION:
        return MAP_STORAGE_IO_EXCEPTION;
    default:
        return MAP_STORAGE_IO_FAILURE;
    }
}
static bool source_file(MapStorage *s, MCObjectRootScope *scope, NBTString *name, MCObject **out,
                        bool *caught) {
    ISaveHandler handler = {s->saveHandler, s->nativeSaveHandlerMethods};
    MCObject *file = NULL;
    if (!io_result(s, handler_result(ISaveHandler_getMapFileFromName(handler, name, &file)),
                   caught))
        return false;
    if (*caught)
        return true;
    if (!source_ref(s, scope, file))
        return false;
    *out = file;
    return true;
}
typedef MapStorageIOResult (*StreamFactory)(MCObject *, MCObject *, MCObject **);
static bool source_stream(MapStorage *s, MCObjectRootScope *scope, StreamFactory call,
                          MCObject *input, MCObject **out, bool *caught) {
    if (!call || !source_ref(s, scope, input))
        return fail(s->object.heap);
    MCObject *stream = NULL;
    if (!io_result(s, call(s->nativeContext, input, &stream), caught))
        return false;
    if (*caught)
        return true;
    if (!stream || !source_ref(s, scope, stream))
        return fail(s->object.heap);
    *out = stream;
    return true;
}
static bool source_read(MapStorage *s, MCObjectRootScope *scope, MCObject *stream, bool compressed,
                        NBTTagCompound **out, bool *caught) {
    const MapStorageSourceDependencies *d = s->sourceDependencies;
    MapStorageIOResult (*call)(MCObject *, MCObject *, NBTTagCompound **) =
        d ? (compressed ? d->readCompressedNBT : d->readRawNBT) : NULL;
    if (!call)
        return fail(s->object.heap);
    NBTTagCompound *tag = NULL;
    if (!io_result(s, call(s->nativeContext, stream, &tag), caught))
        return false;
    if (*caught)
        return true;
    if (!NBTTagCompound_isInstance((MCObject *)tag) || !source_ref(s, scope, (MCObject *)tag))
        return fail(s->object.heap);
    *out = tag;
    return true;
}
static bool source_close(MapStorage *s, MCObject *stream, bool input, bool *caught) {
    const MapStorageSourceDependencies *d = s->sourceDependencies;
    MapStorageIOResult (*call)(MCObject *, MCObject *) =
        d ? (input ? d->closeInputStream : d->closeOutputStream) : NULL;
    return call ? io_result(s, call(s->nativeContext, stream), caught) : fail(s->object.heap);
}
static bool source_exists(MapStorage *s, MCObject *file, bool *exists, bool *caught) {
    const MapStorageSourceDependencies *d = s->sourceDependencies;
    if (!d || !d->fileExists)
        return fail(s->object.heap);
    bool value = false;
    if (!io_result(s, d->fileExists(s->nativeContext, file, &value), caught))
        return false;
    if (!*caught)
        *exists = value;
    return true;
}
static bool source_load_counts(MapStorage *s, MCObjectRootScope *scope) {
    if (!MapStorage_nativeClearIdCounts(s))
        return false;
    if (!s->saveHandler)
        return true;
    bool caught = false, exists = false;
    MCObject *file = NULL, *input = NULL, *dataInput = NULL;
    NBTString *name = NBTString_literalASCII(s->object.heap, "idcounts");
    if (!name || !source_file(s, scope, name, &file, &caught))
        return false;
    if (caught || !file)
        return true;
    if (!source_exists(s, file, &exists, &caught))
        return false;
    if (caught || !exists)
        return true;
    const MapStorageSourceDependencies *d = s->sourceDependencies;
    if (!source_stream(s, scope, d ? d->newFileInputStream : NULL, file, &input, &caught))
        return false;
    if (caught)
        return true;
    if (!source_stream(s, scope, d ? d->newDataInputStream : NULL, input, &dataInput, &caught))
        return false;
    if (caught)
        return true;
    NBTTagCompound *tag = NULL;
    if (!source_read(s, scope, dataInput, false, &tag, &caught))
        return false;
    if (caught)
        return true;
    if (!source_close(s, dataInput, true, &caught))
        return false;
    if (caught)
        return true;
    NBTCompoundKeySet *keys = NBTTagCompound_getKeySet(tag);
    if (!keys)
        return false;
    int32_t n = NBTCompoundKeySet_size(keys);
    for (int32_t i = 0; i < n; i++) {
        NBTString *key = NBTCompoundKeySet_keyAt(keys, i);
        NBTBase *value = NBTTagCompound_getTag(tag, key);
        if (value && NBTBase_getId(value) == 2 &&
            !MapStorage_nativeImportExactShort(s, key, NBTTagCompound_getShort(tag, key)))
            return false;
        if (MCObjectHeap_failed(s->object.heap))
            return false;
    }
    return true;
}
MapStorage *MapStorage_nativeAllocate(MCObjectHeap *h, MCObject *context,
                                      const MapStorageSourceDependencies *dependencies) {
    if (context && (context->heap != h || MCObjectHeap_objectSize(context) < sizeof(MCObject))) {
        fail(h);
        return NULL;
    }
    MapStorage *s = (MapStorage *)MCObjectHeap_alloc(h, sizeof(*s), &klass);
    if (s) {
        s->nativeContext = context;
        s->sourceDependencies = dependencies;
    }
    return s;
}
bool MapStorage_construct(MapStorage *s, ISaveHandler save) {
    MCObjectRootScope scope = {0};
    if (!source_begin(s, &scope))
        return false;
    MCObjectHeap *h = s->object.heap;
    NativeHashMap *map = NativeHashMap_new(h, NATIVE_HASH_KEY_STRING);
    bool ok = map != NULL;
    if (ok) {
        s->loadedDataMap = map;
        MCObjectHeap_touch(h);
    }
    NativeReferenceList *list = ok ? NativeReferenceList_new(h) : NULL;
    ok = ok && list;
    if (ok) {
        s->loadedDataList = list;
        MCObjectHeap_touch(h);
    }
    MapStorageIdCounts *counts =
        ok ? (MapStorageIdCounts *)MCObjectHeap_alloc(h, sizeof(*counts), &counterClass) : NULL;
    ok = ok && counts;
    if (ok) {
        s->idCounts = counts;
        MCObjectHeap_touch(h);
    }
    if (ok) {
        if (save.instance && save.instance->heap != h)
            ok = fail(h);
        else {
            s->saveHandler = save.instance;
            s->nativeSaveHandlerMethods = save.methods;
            MCObjectHeap_touch(h);
            ok = source_load_counts(s, &scope);
        }
    }
    MCObjectRootScope_end(&scope);
    return ok && !MCObjectHeap_failed(h);
}
MapStorage *MapStorage_new(MCObjectHeap *h, ISaveHandler save, MCObject *context,
                           const MapStorageSourceDependencies *dependencies) {
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h))
        return NULL;
    MapStorage *s = MapStorage_nativeAllocate(h, context, dependencies);
    bool ok = s && MapStorage_construct(s, save);
    MCObjectRootScope_end(&scope);
    return ok ? s : NULL;
}
static bool source_load_file(MapStorage *s, MCObjectRootScope *scope, NativeJavaClass *clazz,
                             NBTString *name, WorldSavedData **local) {
    bool caught = false, exists = false;
    MCObject *file = NULL, *input = NULL;
    if (!source_file(s, scope, name, &file, &caught))
        return false;
    if (caught || !file)
        return true;
    if (!source_exists(s, file, &exists, &caught))
        return false;
    if (caught || !exists)
        return true;
    const MapStorageSourceDependencies *d = s->sourceDependencies;
    if (!d || !d->constructSavedData)
        return fail(s->object.heap);
    if (clazz && (!NativeJavaClass_isInstance((MCObject *)clazz) ||
                  !source_ref(s, scope, (MCObject *)clazz)))
        return fail(s->object.heap);
    WorldSavedData *data = NULL;
    if (!io_result(s, d->constructSavedData(s->nativeContext, clazz, name, &data), &caught))
        return false;
    if (caught)
        return true;
    if (!data || !source_saved(s, data) || !source_ref(s, scope, (MCObject *)data))
        return fail(s->object.heap);
    *local = data; /* Assignment precedes open/read/close/readFromNBT failures. */
    if (!source_stream(s, scope, d->newFileInputStream, file, &input, &caught))
        return false;
    if (caught)
        return true;
    NBTTagCompound *tag = NULL;
    if (!source_read(s, scope, input, true, &tag, &caught))
        return false;
    if (caught)
        return true;
    if (!source_close(s, input, true, &caught))
        return false;
    if (caught)
        return true;
    NBTTagCompound *inner = NBTTagCompound_getCompoundTag_ascii(tag, "data");
    if (!inner)
        return false;
    return io_result(s, saved_result(WorldSavedData_readFromNBT(data, inner)), &caught);
}
bool MapStorage_loadData_base(MapStorage *s, NativeJavaClass *clazz, NBTString *name,
                              WorldSavedData **out) {
    MCObjectRootScope scope = {0};
    if (!source_begin(s, &scope))
        return false;
    bool ok = out && valid_key(s->object.heap, name) && source_ref(s, &scope, (MCObject *)name);
    if (!ok)
        fail(s->object.heap);
    NativeHashMap *map = ok ? source_map(s, &scope) : NULL;
    ok = ok && map;
    WorldSavedData *data = ok ? (WorldSavedData *)NativeHashMap_get(map, (MCObject *)name) : NULL;
    ok = ok && !MCObjectHeap_failed(s->object.heap) && source_saved(s, data);
    if (ok && data)
        ok = source_ref(s, &scope, (MCObject *)data);
    else if (ok) {
        if (s->saveHandler)
            ok = source_load_file(s, &scope, clazz, name, &data);
        if (ok && data) {
            map = source_map(s, &scope);
            ok = map && NativeHashMap_put(map, (MCObject *)name, (MCObject *)data);
            NativeReferenceList *list = ok ? source_list(s, &scope) : NULL;
            ok = ok && list && NativeReferenceList_add(list, (MCObject *)data);
        }
    }
    ok = ok && !MCObjectHeap_failed(s->object.heap);
    if (ok)
        *out = data;
    MCObjectRootScope_end(&scope);
    return ok;
}
bool MapStorage_loadData(MapStorage *s, NativeJavaClass *clazz, NBTString *name,
                         WorldSavedData **out) {
    if (SaveDataMemoryStorage_isInstance((MCObject *)s))
        return SaveDataMemoryStorage_loadData((SaveDataMemoryStorage *)s, clazz, name, out);
    return MapStorage_loadData_base(s, clazz, name, out);
}
static bool source_remove_equal(MapStorage *s, MCObjectRootScope *scope, NativeReferenceList *list,
                                MCObject *query) {
    for (int32_t i = 0; i < NativeReferenceList_size(list); i++) {
        MCObject *value = NativeReferenceList_get(list, i);
        bool equal = value == NULL;
        if (MCObjectHeap_failed(s->object.heap))
            return false;
        if (query) {
            if (!source_ref(s, scope, s->nativeContext) || !source_ref(s, scope, query) ||
                !source_ref(s, scope, value))
                return false;
            const MapStorageSourceDependencies *d = s->sourceDependencies;
            if (!d || !d->savedDataEquals)
                return fail(s->object.heap);
            MapStorageIOResult result = d->savedDataEquals(s->nativeContext, query, value, &equal);
            /* remove(Object) is outside the Source catch block. */
            if (result != MAP_STORAGE_IO_OK || MCObjectHeap_failed(s->object.heap))
                return fail(s->object.heap);
            if (!source_ref(s, scope, s->nativeContext))
                return false;
        }
        if (equal) {
            NativeReferenceList_remove(list, i);
            return !MCObjectHeap_failed(s->object.heap);
        }
    }
    return !MCObjectHeap_failed(s->object.heap);
}
bool MapStorage_setData_base(MapStorage *s, NBTString *name, WorldSavedData *data) {
    MCObjectRootScope scope = {0};
    if (!source_begin(s, &scope))
        return false;
    bool ok = valid_key(s->object.heap, name) && source_saved(s, data) &&
              source_ref(s, &scope, (MCObject *)name) && source_ref(s, &scope, (MCObject *)data);
    NativeHashMap *map = ok ? source_map(s, &scope) : NULL;
    ok = ok && map;
    bool contains = ok && NativeHashMap_containsKey(map, (MCObject *)name);
    ok = ok && !MCObjectHeap_failed(s->object.heap);
    if (ok && contains) {
        /* Java captures the list receiver before evaluating map.remove(name). */
        NativeReferenceList *list = s->loadedDataList;
        map = source_map(s, &scope);
        ok = map != NULL;
        MCObject *old = ok ? NativeHashMap_remove(map, (MCObject *)name) : NULL;
        ok = ok && !MCObjectHeap_failed(s->object.heap) && source_ref(s, &scope, old);
        if (ok) {
            /* Capturing a NULL receiver does not throw until invocation,
               after the map.remove argument and its mutation have completed. */
            ok = NativeReferenceList_isInstance((MCObject *)list) &&
                 source_ref(s, &scope, (MCObject *)list);
            if (!ok)
                fail(s->object.heap);
        }
        if (ok)
            ok = source_remove_equal(s, &scope, list, old);
    }
    if (ok) {
        map = source_map(s, &scope);
        ok = map && NativeHashMap_put(map, (MCObject *)name, (MCObject *)data);
    }
    if (ok) {
        NativeReferenceList *list = source_list(s, &scope);
        ok = list && NativeReferenceList_add(list, (MCObject *)data);
    }
    MCObjectRootScope_end(&scope);
    return ok && !MCObjectHeap_failed(s->object.heap);
}
bool MapStorage_setData(MapStorage *s, NBTString *name, WorldSavedData *data) {
    if (SaveDataMemoryStorage_isInstance((MCObject *)s))
        return SaveDataMemoryStorage_setData((SaveDataMemoryStorage *)s, name, data);
    return MapStorage_setData_base(s, name, data);
}
static bool source_save_data(MapStorage *s, MCObjectRootScope *scope, WorldSavedData *data) {
    if (!s->saveHandler)
        return true;
    bool caught = false;
    MCObject *file = NULL, *output = NULL;
    if (!source_file(s, scope, data->mapName, &file, &caught))
        return false;
    if (caught || !file)
        return true;
    NBTTagCompound *inner = NBTTagCompound_new(s->object.heap);
    if (!inner)
        return false;
    if (!io_result(s, saved_result(WorldSavedData_writeToNBT(data, inner)), &caught))
        return false;
    if (caught)
        return true;
    NBTTagCompound *outer = NBTTagCompound_new(s->object.heap);
    if (!outer || !NBTTagCompound_setTag_ascii(outer, "data", (NBTBase *)inner))
        return false;
    const MapStorageSourceDependencies *d = s->sourceDependencies;
    if (!source_stream(s, scope, d ? d->newFileOutputStream : NULL, file, &output, &caught))
        return false;
    if (caught)
        return true;
    if (!d->writeCompressedNBT)
        return fail(s->object.heap);
    if (!io_result(s, d->writeCompressedNBT(s->nativeContext, outer, output), &caught))
        return false;
    if (caught)
        return true;
    return source_close(s, output, false, &caught);
}
bool MapStorage_saveAllData_base(MapStorage *s) {
    MCObjectRootScope scope = {0};
    if (!source_begin(s, &scope))
        return false;
    bool ok = true;
    for (int32_t i = 0; ok; i++) {
        NativeReferenceList *list = source_list(s, &scope);
        ok = list != NULL;
        int32_t size = ok ? NativeReferenceList_size(list) : 0;
        ok = ok && !MCObjectHeap_failed(s->object.heap);
        if (!ok || i >= size)
            break;
        list = source_list(s, &scope);
        ok = list != NULL;
        WorldSavedData *data = ok ? (WorldSavedData *)NativeReferenceList_get(list, i) : NULL;
        ok = ok && !MCObjectHeap_failed(s->object.heap) && source_saved(s, data) && data &&
             source_ref(s, &scope, (MCObject *)data);
        bool dirty = false;
        if (ok)
            ok = WorldSavedData_isDirty(data, &dirty) == WORLD_SAVED_DATA_OK;
        if (ok && dirty) {
            ok = source_save_data(s, &scope, data);
            if (ok)
                ok = WorldSavedData_setDirty(data, false) == WORLD_SAVED_DATA_OK;
        }
        if (!ok)
            fail(s->object.heap);
        /* A Java list cannot have a size beyond INT32_MAX, so the final
           increment from INT32_MAX-1 cannot overflow before the size check. */
    }
    MCObjectRootScope_end(&scope);
    return ok && !MCObjectHeap_failed(s->object.heap);
}
bool MapStorage_saveAllData(MapStorage *s) {
    if (SaveDataMemoryStorage_isInstance((MCObject *)s))
        return SaveDataMemoryStorage_saveAllData((SaveDataMemoryStorage *)s);
    return MapStorage_saveAllData_base(s);
}
static bool source_save_counts(MapStorage *s, MCObjectRootScope *scope) {
    bool caught = false;
    MCObject *file = NULL, *output = NULL, *dataOutput = NULL;
    NBTString *name = NBTString_literalASCII(s->object.heap, "idcounts");
    if (!name || !source_file(s, scope, name, &file, &caught))
        return false;
    if (caught || !file)
        return true;
    NBTTagCompound *tag = MapStorage_nativeSnapshotIdCounts(s);
    if (!tag)
        return false;
    const MapStorageSourceDependencies *d = s->sourceDependencies;
    if (!source_stream(s, scope, d ? d->newFileOutputStream : NULL, file, &output, &caught))
        return false;
    if (caught)
        return true;
    if (!source_stream(s, scope, d ? d->newDataOutputStream : NULL, output, &dataOutput, &caught))
        return false;
    if (caught)
        return true;
    if (!d->writeRawNBT)
        return fail(s->object.heap);
    if (!io_result(s, d->writeRawNBT(s->nativeContext, tag, dataOutput), &caught))
        return false;
    if (caught)
        return true;
    return source_close(s, dataOutput, false, &caught);
}
