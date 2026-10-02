#include "world/WorldSavedData.h"

static const NativeJavaClassDescriptor *const savedDataParents[] = {&NativeJavaClass_ObjectClass};
const NativeJavaClassDescriptor WorldSavedData_Class = {
    "net.minecraft.world.WorldSavedData", savedDataParents, 1, NULL};

typedef struct SavedTypeEntry {
    MCObject object;
    const WorldSavedDataNativeType *facts;
    struct SavedTypeEntry *next;
} SavedTypeEntry;
typedef struct {
    MCObject object;
    SavedTypeEntry *entries;
} SavedTypes;
static const MCObjectClass entryClass, typesClass;
static bool fail(MCObjectHeap *heap) {
    MCObjectHeap_fail(heap);
    return false;
}
static bool facts_valid(const WorldSavedDataNativeType *facts) {
    return facts && facts->objectClass && facts->objectClass->shallow_clone &&
           facts->objectClass->trace && facts->minimumSize >= sizeof(WorldSavedData);
}
static bool any_registry(const MCObject *object, void *context) {
    (void)context;
    return MCObjectHeap_objectSize(object) >= sizeof(SavedTypes);
}
static SavedTypes *registry(MCObjectHeap *heap) {
    return (SavedTypes *)MCObjectHeap_findObject(heap, &typesClass, any_registry, NULL);
}
static const WorldSavedDataNativeType *lookup(MCObjectHeap *heap, const MCObjectClass *klass) {
    SavedTypes *types = registry(heap);
    if (!types)
        return NULL;
    size_t visited = 0, limit = MCObjectHeap_liveObjects(heap);
    for (SavedTypeEntry *entry = types->entries; entry; entry = entry->next) {
        if (++visited > limit || entry->object.heap != heap || entry->object.klass != &entryClass ||
            MCObjectHeap_objectSize((MCObject *)entry) < sizeof(*entry) ||
            !facts_valid(entry->facts))
            return NULL;
        if (entry->facts->objectClass == klass)
            return entry->facts;
    }
    return NULL;
}
static void entry_trace(MCObject *object, MCObjectVisitor visit, void *context) {
    SavedTypeEntry *entry = (SavedTypeEntry *)object;
    entry->next = (SavedTypeEntry *)visit((MCObject *)entry->next, context);
}
static void types_trace(MCObject *object, MCObjectVisitor visit, void *context) {
    SavedTypes *types = (SavedTypes *)object;
    types->entries = (SavedTypeEntry *)visit((MCObject *)types->entries, context);
}
static const MCObjectClass entryClass = {"native.WorldSavedData.TypeEntry", MCObjectHeap_plainClone,
                                         entry_trace, NULL};
static const MCObjectClass typesClass = {"native.WorldSavedData.Types", MCObjectHeap_plainClone,
                                         types_trace, NULL};
bool WorldSavedData_nativeRegisterType(MCObjectHeap *heap, const WorldSavedDataNativeType *facts) {
    if (!heap || MCObjectHeap_failed(heap) || !facts_valid(facts))
        return fail(heap);
    const WorldSavedDataNativeType *old = lookup(heap, facts->objectClass);
    if (old)
        return old == facts || fail(heap);
    SavedTypes *types = registry(heap);
    if (!types) {
        types = (SavedTypes *)MCObjectHeap_alloc(heap, sizeof(*types), &typesClass);
        if (!types)
            return false;
        MCObjectRoot root = {0};
        if (!MCObjectRoot_init(&root, heap, (MCObject *)types))
            return fail(heap);
    }
    SavedTypeEntry *entry = (SavedTypeEntry *)MCObjectHeap_alloc(heap, sizeof(*entry), &entryClass);
    if (!entry)
        return false;
    entry->facts = facts;
    entry->next = types->entries;
    types->entries = entry;
    MCObjectHeap_touch(heap);
    return true;
}
WorldSavedData *WorldSavedData_nativeAllocate(MCObjectHeap *heap,
                                              const WorldSavedDataNativeType *facts,
                                              MCObject *context) {
    if (context && (context->heap != heap || MCObjectHeap_objectSize(context) < sizeof(MCObject))) {
        fail(heap);
        return NULL;
    }
    if (!WorldSavedData_nativeRegisterType(heap, facts))
        return NULL;
    WorldSavedData *data =
        (WorldSavedData *)MCObjectHeap_alloc(heap, facts->minimumSize, facts->objectClass);
    if (data)
        data->nativeContext = context;
    return data;
}
bool WorldSavedData_isInstance(const MCObject *object) {
    if (!object || MCObjectHeap_objectSize(object) < sizeof(MCObject))
        return false;
    const WorldSavedDataNativeType *facts = lookup(object->heap, object->klass);
    return facts && MCObjectHeap_objectSize(object) >= facts->minimumSize;
}
void WorldSavedData_traceFields(WorldSavedData *data, MCObjectVisitor visit, void *context) {
    /* During graph clone the type registry may not yet be remapped. Validate
       allocation capacity directly before reading inherited payload fields. */
    if (!data || !visit || MCObjectHeap_objectSize((MCObject *)data) < sizeof(*data)) {
        fail(data ? data->object.heap : NULL);
        return;
    }
    data->mapName = (NBTString *)visit((MCObject *)data->mapName, context);
    data->nativeContext = visit(data->nativeContext, context);
}
static bool begin(WorldSavedData *data, MCObjectRootScope *scope) {
    MCObjectHeap *heap = data ? data->object.heap : NULL;
    if (!WorldSavedData_isInstance((MCObject *)data) || MCObjectHeap_failed(heap))
        return fail(heap);
    if (data->nativeContext && (data->nativeContext->heap != heap ||
                                MCObjectHeap_objectSize(data->nativeContext) < sizeof(MCObject)))
        return fail(heap);
    if (!MCObjectRootScope_begin(scope, heap) || !MCObjectRootScope_pin(scope, (MCObject *)data) ||
        !MCObjectRootScope_pin(scope, data->nativeContext)) {
        MCObjectRootScope_end(scope);
        return fail(heap);
    }
    return true;
}
static const WorldSavedDataVirtualMethods *methods(WorldSavedData *data) {
    const WorldSavedDataNativeType *facts = lookup(data->object.heap, data->object.klass);
    return facts ? facts->virtualMethods : NULL;
}
static WorldSavedDataResult finish(WorldSavedData *data, MCObjectRootScope *scope,
                                   WorldSavedDataResult result) {
    MCObjectHeap *heap = data->object.heap;
    /* Native callback context is a managed strong edge. Validate the fresh
       value even when the last callback changed it; retain Source effects. */
    if (data->nativeContext && (data->nativeContext->heap != heap ||
                                MCObjectHeap_objectSize(data->nativeContext) < sizeof(MCObject)))
        fail(heap);
    if (result != WORLD_SAVED_DATA_OK && result != WORLD_SAVED_DATA_EXCEPTION)
        fail(heap);
    if (MCObjectHeap_failed(heap))
        result = WORLD_SAVED_DATA_FAILURE;
    MCObjectRootScope_end(scope);
    return result;
}
bool WorldSavedData_construct(WorldSavedData *data, NBTString *name) {
    MCObjectRootScope scope = {0};
    if (!begin(data, &scope))
        return false;
    MCObjectHeap *heap = data->object.heap;
    bool ok = !name || (NBTString_isInstance((MCObject *)name) && ((MCObject *)name)->heap == heap);
    if (ok) {
        data->mapName = name;
        MCObjectHeap_touch(heap);
    } else
        fail(heap);
    MCObjectRootScope_end(&scope);
    return ok;
}
static WorldSavedDataResult nbt_dispatch(WorldSavedData *data, NBTTagCompound *tag, bool write) {
    MCObjectRootScope scope = {0};
    if (!begin(data, &scope))
        return WORLD_SAVED_DATA_FAILURE;
    MCObjectHeap *heap = data->object.heap;
    if (tag && (!NBTTagCompound_isInstance((MCObject *)tag) || ((MCObject *)tag)->heap != heap)) {
        fail(heap);
        return finish(data, &scope, WORLD_SAVED_DATA_FAILURE);
    }
    if (!MCObjectRootScope_pin(&scope, (MCObject *)tag))
        return finish(data, &scope, WORLD_SAVED_DATA_FAILURE);
    const WorldSavedDataVirtualMethods *v = methods(data);
    WorldSavedDataResult (*call)(MCObject *, WorldSavedData *, NBTTagCompound *) =
        v ? (write ? v->writeToNBT : v->readFromNBT) : NULL;
    WorldSavedDataResult result =
        call ? call(data->nativeContext, data, tag) : WORLD_SAVED_DATA_FAILURE;
    return finish(data, &scope, result);
}
WorldSavedDataResult WorldSavedData_readFromNBT(WorldSavedData *data, NBTTagCompound *tag) {
    return nbt_dispatch(data, tag, false);
}
WorldSavedDataResult WorldSavedData_writeToNBT(WorldSavedData *data, NBTTagCompound *tag) {
    return nbt_dispatch(data, tag, true);
}
WorldSavedDataResult WorldSavedData_setDirty_base(WorldSavedData *data, bool dirty) {
    MCObjectRootScope scope = {0};
    if (!begin(data, &scope))
        return WORLD_SAVED_DATA_FAILURE;
    data->dirty = dirty;
    MCObjectHeap_touch(data->object.heap);
    return finish(data, &scope, WORLD_SAVED_DATA_OK);
}
WorldSavedDataResult WorldSavedData_isDirty_base(WorldSavedData *data, bool *output) {
    MCObjectRootScope scope = {0};
    if (!begin(data, &scope))
        return WORLD_SAVED_DATA_FAILURE;
    if (!output)
        return finish(data, &scope, WORLD_SAVED_DATA_FAILURE);
    *output = data->dirty;
    return finish(data, &scope, WORLD_SAVED_DATA_OK);
}
WorldSavedDataResult WorldSavedData_setDirty(WorldSavedData *data, bool dirty) {
    MCObjectRootScope scope = {0};
    if (!begin(data, &scope))
        return WORLD_SAVED_DATA_FAILURE;
    const WorldSavedDataVirtualMethods *v = methods(data);
    WorldSavedDataResult result = v && v->setDirty ? v->setDirty(data->nativeContext, data, dirty)
                                                   : WorldSavedData_setDirty_base(data, dirty);
    return finish(data, &scope, result);
}
WorldSavedDataResult WorldSavedData_isDirty(WorldSavedData *data, bool *output) {
    MCObjectRootScope scope = {0};
    if (!begin(data, &scope))
        return WORLD_SAVED_DATA_FAILURE;
    if (!output)
        return finish(data, &scope, WORLD_SAVED_DATA_FAILURE);
    const WorldSavedDataVirtualMethods *v = methods(data);
    bool dirty = false;
    WorldSavedDataResult result = v && v->isDirty ? v->isDirty(data->nativeContext, data, &dirty)
                                                  : WorldSavedData_isDirty_base(data, &dirty);
    result = finish(data, &scope, result);
    if (result == WORLD_SAVED_DATA_OK)
        *output = dirty;
    return result;
}
WorldSavedDataResult WorldSavedData_markDirty_base(WorldSavedData *data) {
    MCObjectRootScope scope = {0};
    if (!begin(data, &scope))
        return WORLD_SAVED_DATA_FAILURE;
    WorldSavedDataResult result = WorldSavedData_setDirty(data, true);
    return finish(data, &scope, result);
}
WorldSavedDataResult WorldSavedData_markDirty(WorldSavedData *data) {
    MCObjectRootScope scope = {0};
    if (!begin(data, &scope))
        return WORLD_SAVED_DATA_FAILURE;
    const WorldSavedDataVirtualMethods *v = methods(data);
    WorldSavedDataResult result = v && v->markDirty ? v->markDirty(data->nativeContext, data)
                                                    : WorldSavedData_markDirty_base(data);
    return finish(data, &scope, result);
}
