#include "world/storage/SaveDataMemoryStorage.h"
static const MCObjectClass klass = {"net.minecraft.world.storage.SaveDataMemoryStorage",
                                    MCObjectHeap_plainClone, MapStorage_nativeTraceFields, NULL};
bool SaveDataMemoryStorage_isInstance(const MCObject *o) {
    return o && o->klass == &klass && MCObjectHeap_objectSize(o) >= sizeof(SaveDataMemoryStorage);
}
SaveDataMemoryStorage *SaveDataMemoryStorage_nativeAllocate(MCObjectHeap *h) {
    return (SaveDataMemoryStorage *)MCObjectHeap_alloc(h, sizeof(SaveDataMemoryStorage), &klass);
}
bool SaveDataMemoryStorage_construct(SaveDataMemoryStorage *s) {
    if (!SaveDataMemoryStorage_isInstance((MCObject *)s)) {
        MCObjectHeap_fail(s ? s->base.object.heap : NULL);
        return false;
    }
    return MapStorage_construct(&s->base, (ISaveHandler){NULL, NULL});
}
SaveDataMemoryStorage *SaveDataMemoryStorage_new(MCObjectHeap *h) {
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h))
        return NULL;
    SaveDataMemoryStorage *s = SaveDataMemoryStorage_nativeAllocate(h);
    bool ok = s && SaveDataMemoryStorage_construct(s);
    MCObjectRootScope_end(&scope);
    return ok ? s : NULL;
}
SaveDataMemoryStorage *SaveDataMemoryStorage_nativeNewCounterProvider(MCObjectHeap *h) {
    return SaveDataMemoryStorage_new(h);
}
static bool begin(SaveDataMemoryStorage *s, MCObjectRootScope *scope) {
    MCObjectHeap *h = s ? s->base.object.heap : NULL;
    if (!SaveDataMemoryStorage_isInstance((MCObject *)s) || MCObjectHeap_failed(h)) {
        MCObjectHeap_fail(h);
        return false;
    }
    if (!MCObjectRootScope_begin(scope, h) || !MCObjectRootScope_pin(scope, (MCObject *)s)) {
        MCObjectRootScope_end(scope);
        MCObjectHeap_fail(h);
        return false;
    }
    return true;
}
static bool key_valid(MCObjectHeap *h, NBTString *key) {
    if (!key || (NBTString_isInstance((MCObject *)key) && ((MCObject *)key)->heap == h))
        return true;
    MCObjectHeap_fail(h);
    return false;
}
bool SaveDataMemoryStorage_loadData(SaveDataMemoryStorage *s, NativeJavaClass *clazz,
                                    NBTString *key, WorldSavedData **output) {
    (void)clazz;
    MCObjectRootScope scope = {0};
    if (!begin(s, &scope))
        return false;
    MCObjectHeap *h = s->base.object.heap;
    NativeHashMap *map = s->base.loadedDataMap;
    bool ok = output && key_valid(h, key) && NativeHashMap_isInstance((MCObject *)map) &&
              ((MCObject *)map)->heap == h;
    WorldSavedData *data = ok ? (WorldSavedData *)NativeHashMap_get(map, (MCObject *)key) : NULL;
    ok = ok && !MCObjectHeap_failed(h) &&
         (!data || (WorldSavedData_isInstance((MCObject *)data) && data->object.heap == h));
    if (ok)
        *output = data;
    else
        MCObjectHeap_fail(h);
    MCObjectRootScope_end(&scope);
    return ok;
}
bool SaveDataMemoryStorage_setData(SaveDataMemoryStorage *s, NBTString *key, WorldSavedData *data) {
    MCObjectRootScope scope = {0};
    if (!begin(s, &scope))
        return false;
    MCObjectHeap *h = s->base.object.heap;
    NativeHashMap *map = s->base.loadedDataMap;
    bool ok = key_valid(h, key) &&
              (!data || (WorldSavedData_isInstance((MCObject *)data) && data->object.heap == h)) &&
              NativeHashMap_isInstance((MCObject *)map) && ((MCObject *)map)->heap == h;
    if (ok)
        ok = NativeHashMap_put(map, (MCObject *)key, (MCObject *)data);
    if (!ok)
        MCObjectHeap_fail(h);
    MCObjectRootScope_end(&scope);
    return ok;
}
bool SaveDataMemoryStorage_saveAllData(SaveDataMemoryStorage *s) {
    MCObjectRootScope scope = {0};
    if (!begin(s, &scope))
        return false;
    /* This empty Source override deliberately reads no superclass fields. */
    MCObjectRootScope_end(&scope);
    return true;
}
bool SaveDataMemoryStorage_getUniqueDataId(SaveDataMemoryStorage *s, NBTString *key, int32_t *out) {
    MCObjectHeap *h = s ? s->base.object.heap : NULL;
    if (!SaveDataMemoryStorage_isInstance((MCObject *)s) || !out || MCObjectHeap_failed(h) ||
        (key && (!NBTString_isInstance((MCObject *)key) || ((MCObject *)key)->heap != h))) {
        MCObjectHeap_fail(h);
        return false;
    }
    /* Original override ignores the String and all superclass counter state. */
    *out = 0;
    return true;
}
