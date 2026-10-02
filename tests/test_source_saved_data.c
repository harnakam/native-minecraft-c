#include "world/WorldProvider.h"
#include "world/WorldSavedData.h"
#include "world/storage/SaveHandlerMP.h"
#include "world/storage/WorldInfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "saved data check %d line %d: %s\n", checks, __LINE__, #x);            \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
typedef struct {
    WorldSavedData base;
    int32_t value;
} SavedFixture;
static void saved_trace(MCObject *object, MCObjectVisitor visit, void *context) {
    WorldSavedData_traceFields(&((SavedFixture *)object)->base, visit, context);
}
static const MCObjectClass savedClass = {"test.SourceSavedData", MCObjectHeap_plainClone,
                                         saved_trace, NULL};
static const WorldSavedDataNativeType savedType = {&savedClass, sizeof(SavedFixture), NULL};
static MCObject *count_visit(MCObject *object, void *context) {
    ++*(int *)context;
    return object;
}
typedef struct {
    MCObject object;
    SavedFixture *receiver;
    MCObject *retained;
    /* Native callback test input, not an owning graph edge; foreign input is
       rejected at the callback return boundary before successful collection. */
    MCObject *replacementContext;
    WorldSavedDataResult result;
    int32_t calls;
    bool requested, collectBlocked, cloneBlocked, invert;
} SavedContext;
static void context_trace(MCObject *object, MCObjectVisitor visit, void *context) {
    SavedContext *c = (SavedContext *)object;
    c->receiver = (SavedFixture *)visit((MCObject *)c->receiver, context);
    c->retained = visit(c->retained, context);
}
static const MCObjectClass contextClass = {"test.SavedContext", MCObjectHeap_plainClone,
                                           context_trace, NULL};
static SavedContext *saved_context(MCObjectHeap *heap) {
    SavedContext *c = (SavedContext *)MCObjectHeap_alloc(heap, sizeof(*c), &contextClass);
    CHECK(c);
    return c;
}
static SavedContext *enter_callback(MCObject *object, WorldSavedData *data) {
    SavedContext *c = (SavedContext *)object;
    CHECK(c && c->receiver == (SavedFixture *)data);
    CHECK(object->heap == data->object.heap);
    ++c->calls;
    if (c->requested) {
        c->collectBlocked = !MCObjectHeap_collect(object->heap);
        MCObjectHeap *copy = MCObjectHeap_clone(object->heap);
        c->cloneBlocked = copy == NULL;
        MCObjectHeap_free(copy);
    }
    return c;
}
static void replace_context(SavedContext *context, WorldSavedData *data) {
    if (context->replacementContext) {
        data->nativeContext = context->replacementContext;
        MCObjectHeap_touch(data->object.heap);
    }
}
static WorldSavedDataResult read_data(MCObject *object, WorldSavedData *data, NBTTagCompound *tag) {
    SavedContext *c = enter_callback(object, data);
    c->receiver->value = tag ? NBTTagCompound_getInteger_ascii(tag, "value") : 17;
    MCObjectHeap_touch(object->heap);
    replace_context(c, data);
    return c->result;
}
static WorldSavedDataResult write_data(MCObject *object, WorldSavedData *data,
                                       NBTTagCompound *tag) {
    SavedContext *c = enter_callback(object, data);
    if (!tag || !NBTTagCompound_setInteger_ascii(tag, "value", c->receiver->value))
        return WORLD_SAVED_DATA_FAILURE;
    replace_context(c, data);
    return c->result;
}
static WorldSavedDataResult set_dirty(MCObject *object, WorldSavedData *data, bool value) {
    SavedContext *c = enter_callback(object, data);
    WorldSavedDataResult result = WorldSavedData_setDirty_base(data, c->invert ? !value : value);
    replace_context(c, data);
    return result == WORLD_SAVED_DATA_OK ? c->result : result;
}
static WorldSavedDataResult get_dirty(MCObject *object, WorldSavedData *data, bool *out) {
    SavedContext *c = enter_callback(object, data);
    WorldSavedDataResult result = WorldSavedData_isDirty_base(data, out);
    replace_context(c, data);
    return result == WORLD_SAVED_DATA_OK ? c->result : result;
}
static const WorldSavedDataVirtualMethods savedMethods = {.readFromNBT = read_data,
                                                          .writeToNBT = write_data,
                                                          .setDirty = set_dirty,
                                                          .isDirty = get_dirty};
static const MCObjectClass virtualClass = {"test.VirtualSavedData", MCObjectHeap_plainClone,
                                           saved_trace, NULL};
static const WorldSavedDataNativeType virtualType = {&virtualClass, sizeof(SavedFixture),
                                                     &savedMethods};
static SavedFixture *virtual_data(MCObjectHeap *heap, SavedContext *context) {
    SavedFixture *p =
        (SavedFixture *)WorldSavedData_nativeAllocate(heap, &virtualType, (MCObject *)context);
    CHECK(p);
    context->receiver = p;
    CHECK(WorldSavedData_construct(&p->base, NULL));
    return p;
}

static void source_constructor(void) {
    MCObjectHeap *heap = MCObjectHeap_new(65536);
    CHECK(heap);
    WorldSavedData *data = WorldSavedData_nativeAllocate(heap, &savedType, NULL);
    CHECK(data);
    /* Java allocation defaults precede the base constructor, which has no
       dirty initializer: a previous subclass/fixture write must survive it. */
    CHECK(data->mapName == NULL && !data->dirty);
    data->dirty = true;
    CHECK(WorldSavedData_construct(data, NULL));
    CHECK(data->mapName == NULL && data->dirty);
    NBTString *name = NBTString_fromASCII(heap, "map_4");
    CHECK(name);
    CHECK(WorldSavedData_construct(data, name));
    CHECK(data->mapName == name && data->dirty);
    CHECK(!MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}
static void fieldless_mp_constructor(void) {
    MCObjectHeap *heap = MCObjectHeap_new(65536);
    CHECK(heap);
    size_t before = MCObjectHeap_liveObjects(heap);
    SaveHandlerMP *handler = SaveHandlerMP_new(heap);
    CHECK(handler);
    CHECK(MCObjectHeap_liveObjects(heap) == before + 1);
    CHECK(MCObjectHeap_objectSize((MCObject *)handler) == sizeof(MCObject));
    CHECK(SaveHandlerMP_isInstance((MCObject *)handler));
    CHECK(!MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}
static void source_virtual_methods(void) {
    MCObjectHeap *heap = MCObjectHeap_new(65536);
    CHECK(heap);
    SavedContext *c = saved_context(heap);
    SavedFixture *p = virtual_data(heap, c);
    bool dirty = true;
    CHECK(WorldSavedData_isDirty(&p->base, &dirty) == WORLD_SAVED_DATA_OK && !dirty);
    CHECK(c->calls == 1);
    c->requested = true;
    c->invert = true;
    CHECK(WorldSavedData_markDirty(&p->base) == WORLD_SAVED_DATA_OK);
    CHECK(c->calls == 2 && !p->base.dirty && c->collectBlocked && c->cloneBlocked);
    c->invert = false;
    CHECK(WorldSavedData_markDirty_base(&p->base) == WORLD_SAVED_DATA_OK && p->base.dirty);
    NBTTagCompound *tag = NBTTagCompound_new(heap);
    CHECK(tag);
    CHECK(NBTTagCompound_setInteger_ascii(tag, "value", 41));
    CHECK(WorldSavedData_readFromNBT(&p->base, tag) == WORLD_SAVED_DATA_OK && p->value == 41);
    CHECK(WorldSavedData_readFromNBT(&p->base, NULL) == WORLD_SAVED_DATA_OK && p->value == 17);
    p->value = 73;
    CHECK(WorldSavedData_writeToNBT(&p->base, tag) == WORLD_SAVED_DATA_OK);
    CHECK(NBTTagCompound_getInteger_ascii(tag, "value") == 73);
    c->result = WORLD_SAVED_DATA_EXCEPTION;
    CHECK(NBTTagCompound_setInteger_ascii(tag, "value", 91));
    CHECK(WorldSavedData_readFromNBT(&p->base, tag) == WORLD_SAVED_DATA_EXCEPTION &&
          p->value == 91);
    CHECK(!MCObjectHeap_failed(heap));
    p->value = 117;
    CHECK(WorldSavedData_writeToNBT(&p->base, tag) == WORLD_SAVED_DATA_EXCEPTION);
    CHECK(NBTTagCompound_getInteger_ascii(tag, "value") == 117 && !MCObjectHeap_failed(heap));
    dirty = false;
    CHECK(WorldSavedData_isDirty(&p->base, &dirty) == WORLD_SAVED_DATA_EXCEPTION && !dirty);
    CHECK(WorldSavedData_setDirty(&p->base, false) == WORLD_SAVED_DATA_EXCEPTION && !p->base.dirty);
    c->result = WORLD_SAVED_DATA_FAILURE;
    CHECK(NBTTagCompound_setInteger_ascii(tag, "value", 131));
    CHECK(WorldSavedData_readFromNBT(&p->base, tag) == WORLD_SAVED_DATA_FAILURE);
    CHECK(p->value == 131 && MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    int32_t calls = c->calls;
    CHECK(WorldSavedData_markDirty(&p->base) == WORLD_SAVED_DATA_FAILURE && c->calls == calls);
    MCObjectHeap_free(heap);
}
static void abstract_and_native_guards(void) {
    for (int mode = 0; mode < 7; ++mode) {
        MCObjectHeap *heap = MCObjectHeap_new(65536);
        CHECK(heap);
        WorldSavedData *p = WorldSavedData_nativeAllocate(heap, &savedType, NULL);
        CHECK(p);
        CHECK(WorldSavedData_construct(p, NULL));
        if (mode == 0)
            CHECK(WorldSavedData_readFromNBT(p, NULL) == WORLD_SAVED_DATA_FAILURE);
        if (mode == 1)
            CHECK(WorldSavedData_writeToNBT(p, NULL) == WORLD_SAVED_DATA_FAILURE);
        if (mode == 2)
            CHECK(WorldSavedData_isDirty(p, NULL) == WORLD_SAVED_DATA_FAILURE);
        if (mode == 3) {
            MCObject *small = MCObjectHeap_alloc(heap, sizeof(MCObject), &savedClass);
            CHECK(small);
            CHECK(!WorldSavedData_isInstance(small));
            CHECK(WorldSavedData_setDirty((WorldSavedData *)small, true) ==
                  WORLD_SAVED_DATA_FAILURE);
        }
        if (mode == 4) {
            static const MCObjectClass wrong = {"test.Unrelated", MCObjectHeap_plainClone, NULL,
                                                NULL};
            MCObject *other = MCObjectHeap_alloc(heap, sizeof(SavedFixture), &wrong);
            CHECK(other);
            CHECK(!WorldSavedData_isInstance(other));
            CHECK(WorldSavedData_construct((WorldSavedData *)other, NULL) == false);
        }
        if (mode == 5) {
            static const WorldSavedDataNativeType conflicting = {&savedClass,
                                                                 sizeof(WorldSavedData), NULL};
            CHECK(!WorldSavedData_nativeRegisterType(heap, &conflicting));
        }
        if (mode == 6) {
            MCObject *small = MCObjectHeap_alloc(heap, sizeof(MCObject), &savedClass);
            CHECK(small);
            int visits = 0;
            WorldSavedData_traceFields((WorldSavedData *)small, count_visit, &visits);
            CHECK(MCObjectHeap_failed(heap) && visits == 0);
        }
        CHECK(MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
    }
    MCObjectHeap *heap = MCObjectHeap_new(65536), *foreign = MCObjectHeap_new(65536);
    CHECK(heap && foreign);
    WorldSavedData *p = WorldSavedData_nativeAllocate(heap, &savedType, NULL);
    CHECK(p);
    NBTString *name = NBTString_fromASCII(heap, "old"),
              *other = NBTString_fromASCII(foreign, "foreign");
    CHECK(name && other);
    CHECK(WorldSavedData_construct(p, name));
    CHECK(!WorldSavedData_construct(p, other));
    CHECK(p->mapName == name && MCObjectHeap_failed(heap) && !MCObjectHeap_failed(foreign));
    MCObjectHeap_free(heap);
    MCObjectHeap_free(foreign);
}
static void callback_context_boundary(void) {
    for (int operation = 0; operation < 4; ++operation)
        for (int status = WORLD_SAVED_DATA_OK; status <= WORLD_SAVED_DATA_FAILURE; ++status)
            for (int foreignInput = 0; foreignInput < 2; ++foreignInput) {
                MCObjectHeap *heap = MCObjectHeap_new(65536), *foreign = MCObjectHeap_new(65536);
                CHECK(heap && foreign);
                SavedContext *context = saved_context(heap);
                SavedFixture *data = virtual_data(heap, context);
                SavedContext *replacement = saved_context(foreignInput ? foreign : heap);
                replacement->receiver = data;
                context->replacementContext = (MCObject *)replacement;
                context->result = (WorldSavedDataResult)status;
                data->base.dirty = true;
                data->value = 47;
                NBTTagCompound *tag = NBTTagCompound_new(heap);
                CHECK(tag && NBTTagCompound_setInteger_ascii(tag, "value", 83));
                MCObjectRoot root = {0};
                CHECK(MCObjectRoot_init(&root, heap, (MCObject *)data));
                bool output = false;
                WorldSavedDataResult result;
                if (operation == 0)
                    result = WorldSavedData_readFromNBT(&data->base, tag);
                else if (operation == 1)
                    result = WorldSavedData_writeToNBT(&data->base, tag);
                else if (operation == 2)
                    result = WorldSavedData_setDirty(&data->base, false);
                else
                    result = WorldSavedData_isDirty(&data->base, &output);
                WorldSavedDataResult expected =
                    foreignInput ? WORLD_SAVED_DATA_FAILURE : (WorldSavedDataResult)status;
                CHECK(result == expected);
                CHECK(data->base.nativeContext == (MCObject *)replacement && context->calls == 1);
                CHECK(data->value == (operation == 0 ? 83 : 47));
                CHECK(data->base.dirty == (operation != 2));
                CHECK(NBTTagCompound_getInteger_ascii(tag, "value") == (operation == 1 ? 47 : 83));
                CHECK(output == (operation == 3 && expected == WORLD_SAVED_DATA_OK));
                CHECK(MCObjectHeap_failed(heap) == (expected == WORLD_SAVED_DATA_FAILURE));
                CHECK(!MCObjectHeap_failed(foreign) && !MCObjectHeap_hasBorrowers(heap));
                if (expected == WORLD_SAVED_DATA_FAILURE)
                    CHECK(WorldSavedData_markDirty(&data->base) == WORLD_SAVED_DATA_FAILURE &&
                          context->calls == 1);
                else {
                    CHECK(MCObjectHeap_collect(heap));
                    data = (SavedFixture *)MCObjectRoot_get(&root);
                    CHECK(data->base.nativeContext == (MCObject *)replacement &&
                          replacement->receiver == data);
                }
                MCObjectRoot_drop(&root);
                MCObjectHeap_free(heap);
                MCObjectHeap_free(foreign);
            }
}
static void saved_data_graph(void) {
    MCObjectHeap *heap = MCObjectHeap_new(65536);
    CHECK(heap);
    SavedContext *c = saved_context(heap);
    SavedFixture *p = virtual_data(heap, c);
    NBTString *name = NBTString_fromASCII(heap, "map_shared");
    CHECK(name);
    CHECK(WorldSavedData_construct(&p->base, name));
    c->retained = (MCObject *)name;
    p->value = 35;
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, heap, (MCObject *)p));
    CHECK(MCObjectHeap_collect(heap));
    size_t live = MCObjectHeap_liveObjects(heap);
    CHECK(live >= 5);
    MCObjectHeap *branch = MCObjectHeap_clone(heap);
    CHECK(branch);
    MCObjectRoot branchRoot = {0};
    CHECK(MCObjectRoot_rebind(&branchRoot, branch, &root));
    SavedFixture *copy = (SavedFixture *)MCObjectRoot_get(&branchRoot);
    CHECK(copy != p);
    CHECK(WorldSavedData_isInstance((MCObject *)copy));
    SavedContext *copyContext = (SavedContext *)copy->base.nativeContext;
    CHECK(copyContext != c && copyContext->receiver == copy &&
          copyContext->retained == (MCObject *)copy->base.mapName);
    CHECK(copy->base.mapName != name && NBTString_equalsASCII(copy->base.mapName, "map_shared"));
    CHECK(WorldSavedData_markDirty(&copy->base) == WORLD_SAVED_DATA_OK && copy->base.dirty &&
          !p->base.dirty);
    CHECK(MCObjectHeap_adopt(heap, branch));
    MCObjectHeap_free(branch);
    copy = (SavedFixture *)MCObjectRoot_get(&root);
    CHECK(copy->base.object.heap == heap);
    CHECK(WorldSavedData_isInstance((MCObject *)copy));
    CHECK(MCObjectHeap_collect(heap) && MCObjectHeap_liveObjects(heap) == live);
    MCObjectRoot_drop(&root);
    CHECK(MCObjectHeap_collect(heap));
    /* Only the native per-heap class-fact registry and its one entry remain. */
    CHECK(MCObjectHeap_liveObjects(heap) == 2);
    SavedFixture *again = (SavedFixture *)WorldSavedData_nativeAllocate(heap, &virtualType, NULL);
    CHECK(again);
    CHECK(WorldSavedData_isInstance((MCObject *)again));
    CHECK(MCObjectHeap_liveObjects(heap) == 3);
    MCObjectHeap_free(heap);
}
static void genuine_mp_methods(void) {
    MCObjectHeap *heap = MCObjectHeap_new(131072);
    CHECK(heap);
    SaveHandlerMP *p = SaveHandlerMP_new(heap);
    CHECK(p);
    ISaveHandler handler = SaveHandlerMP_asSaveHandler(p);
    CHECK(ISaveHandler_nativeIsValid(handler) && handler.instance == (MCObject *)p);
    WorldInfo *info = (WorldInfo *)p;
    MCObject *object = (MCObject *)p;
    CHECK(SaveHandlerMP_loadWorldInfo(p, &info) == I_SAVE_HANDLER_OK && info == NULL);
    CHECK(SaveHandlerMP_getChunkLoader(p, NULL, &object) == I_SAVE_HANDLER_OK && object == NULL);
    object = (MCObject *)p;
    CHECK(SaveHandlerMP_getPlayerNBTManager(p, &object) == I_SAVE_HANDLER_OK && object == NULL);
    object = (MCObject *)p;
    CHECK(SaveHandlerMP_getWorldDirectory(p, &object) == I_SAVE_HANDLER_OK && object == NULL);
    object = (MCObject *)p;
    CHECK(SaveHandlerMP_getMapFileFromName(p, NULL, &object) == I_SAVE_HANDLER_OK &&
          object == NULL);
    CHECK(SaveHandlerMP_checkSessionLock(p) == I_SAVE_HANDLER_OK &&
          SaveHandlerMP_flush(p) == I_SAVE_HANDLER_OK);
    CHECK(SaveHandlerMP_saveWorldInfoWithPlayer(p, NULL, NULL) == I_SAVE_HANDLER_OK);
    CHECK(SaveHandlerMP_saveWorldInfo(p, NULL) == I_SAVE_HANDLER_OK);
    NBTString *name = NULL, *second = NULL;
    CHECK(SaveHandlerMP_getWorldDirectoryName(p, &name) == I_SAVE_HANDLER_OK);
    CHECK(name && NBTString_equalsASCII(name, "none"));
    CHECK(ISaveHandler_getWorldDirectoryName(handler, &second) == I_SAVE_HANDLER_OK &&
          second == name);
    SaveHandlerMP *other = SaveHandlerMP_new(heap);
    CHECK(other);
    CHECK(SaveHandlerMP_getWorldDirectoryName(other, &second) == I_SAVE_HANDLER_OK &&
          second == name);
    WorldInfo *real = WorldInfo_nativeAllocate(heap, NULL, NULL);
    CHECK(real);
    real->spawnX = 27;
    NBTTagCompound *tag = NBTTagCompound_new(heap);
    CHECK(tag);
    CHECK(NBTTagCompound_setInteger_ascii(tag, "keep", 33));
    WorldProvider *provider = WorldProvider_nativeAllocate(heap, NULL, NULL);
    CHECK(provider);
    provider->dimensionId = -1;
    CHECK(SaveHandlerMP_saveWorldInfoWithPlayer(p, real, tag) == I_SAVE_HANDLER_OK);
    CHECK(SaveHandlerMP_saveWorldInfo(p, real) == I_SAVE_HANDLER_OK);
    CHECK(real->spawnX == 27 && NBTTagCompound_getInteger_ascii(tag, "keep") == 33);
    object = (MCObject *)p;
    CHECK(ISaveHandler_getChunkLoader(handler, provider, &object) == I_SAVE_HANDLER_OK &&
          object == NULL);
    CHECK(provider->dimensionId == -1);
    CHECK(!MCObjectHeap_failed(heap));
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, heap, (MCObject *)p));
    /* A returned native literal is borrowed. Retain it before GC rather than
       compare a reclaimed address that the destination allocator can reuse. */
    MCObjectRoot nameRoot = {0};
    CHECK(MCObjectRoot_init(&nameRoot, heap, (MCObject *)name));
    CHECK(MCObjectHeap_collect(heap));
    MCObjectHeap *branch = MCObjectHeap_clone(heap);
    CHECK(branch);
    MCObjectRoot br = {0};
    CHECK(MCObjectRoot_rebind(&br, branch, &root));
    SaveHandlerMP *copy = (SaveHandlerMP *)MCObjectRoot_get(&br);
    CHECK(copy != p && SaveHandlerMP_isInstance((MCObject *)copy));
    NBTString *copyName = NULL;
    CHECK(SaveHandlerMP_getWorldDirectoryName(copy, &copyName) == I_SAVE_HANDLER_OK);
    CHECK(copyName != name && NBTString_equalsASCII(copyName, "none"));
    MCObjectRoot branchName = {0};
    CHECK(MCObjectRoot_rebind(&branchName, branch, &nameRoot));
    CHECK(MCObjectRoot_get(&branchName) == (MCObject *)copyName);
    CHECK(MCObjectHeap_adopt(heap, branch));
    MCObjectHeap_free(branch);
    copy = (SaveHandlerMP *)MCObjectRoot_get(&root);
    CHECK(ISaveHandler_flush(SaveHandlerMP_asSaveHandler(copy)) == I_SAVE_HANDLER_OK);
    MCObjectRoot_drop(&root);
    MCObjectRoot_drop(&nameRoot);
    CHECK(MCObjectHeap_collect(heap));
    MCObjectHeap_free(heap);
}
static void allocation_prefixes(void) {
    bool sawFailure = false, sawSuccess = false;
    for (size_t budget = 0; budget <= 1024; budget += 8) {
        MCObjectHeap *heap = MCObjectHeap_new(budget);
        CHECK(heap);
        WorldSavedData *p = WorldSavedData_nativeAllocate(heap, &savedType, NULL);
        if (p) {
            sawSuccess = true;
            CHECK(WorldSavedData_construct(p, NULL));
            CHECK(!p->dirty && p->mapName == NULL);
        } else {
            sawFailure = true;
            CHECK(MCObjectHeap_failed(heap));
        }
        CHECK(!MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
    }
    CHECK(sawFailure && sawSuccess);
    MCObjectHeap *heap = MCObjectHeap_new(sizeof(MCObject) - 1);
    CHECK(heap);
    CHECK(!SaveHandlerMP_new(heap) && MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}
typedef struct {
    MCObject object;
    MCObject *file, *lastReceiver;
    NBTString *lastName;
    ISaveHandlerResult result;
    int32_t calls;
    bool blocked;
} HandlerFixture;
static void handler_trace(MCObject *object, MCObjectVisitor visit, void *context) {
    HandlerFixture *h = (HandlerFixture *)object;
    h->file = visit(h->file, context);
    h->lastReceiver = visit(h->lastReceiver, context);
    h->lastName = (NBTString *)visit((MCObject *)h->lastName, context);
}
static const MCObjectClass handlerClass = {"test.SaveHandler", MCObjectHeap_plainClone,
                                           handler_trace, NULL};
static const MCObjectClass fileClass = {"test.ManagedFileView", MCObjectHeap_plainClone, NULL,
                                        NULL};
static bool handler_is_instance(const MCObject *object) {
    return object && object->klass == &handlerClass &&
           MCObjectHeap_objectSize(object) >= sizeof(HandlerFixture);
}
static ISaveHandlerResult handler_get_file(MCObject *object, NBTString *name, MCObject **output) {
    HandlerFixture *h = (HandlerFixture *)object;
    ++h->calls;
    h->lastReceiver = object;
    h->lastName = name;
    h->blocked = !MCObjectHeap_collect(object->heap);
    *output = h->file;
    MCObjectHeap_touch(object->heap);
    return h->result;
}
static const ISaveHandlerMethods handlerMethods = {.isInstance = handler_is_instance,
                                                   .getMapFileFromName = handler_get_file};
static void interface_status_and_lifetime(void) {
    for (int mode = 0; mode < 6; ++mode) {
        MCObjectHeap *heap = MCObjectHeap_new(65536), *foreign = MCObjectHeap_new(65536);
        CHECK(heap && foreign);
        HandlerFixture *p = (HandlerFixture *)MCObjectHeap_alloc(heap, sizeof(*p), &handlerClass);
        CHECK(p);
        p->file = MCObjectHeap_alloc(mode == 3 ? foreign : heap, sizeof(MCObject), &fileClass);
        CHECK(p->file);
        ISaveHandler handler = {(MCObject *)p, &handlerMethods};
        MCObject *output = (MCObject *)p;
        NBTString *name = NBTString_fromASCII(mode == 4 ? foreign : heap, "map_3");
        CHECK(name);
        p->result = mode == 1   ? I_SAVE_HANDLER_EXCEPTION
                    : mode == 2 ? I_SAVE_HANDLER_FAILURE
                                : I_SAVE_HANDLER_OK;
        if (mode == 5) {
            CHECK(ISaveHandler_flush(handler) == I_SAVE_HANDLER_FAILURE && p->calls == 0);
        } else {
            ISaveHandlerResult result = ISaveHandler_getMapFileFromName(handler, name, &output);
            if (mode == 0)
                CHECK(result == I_SAVE_HANDLER_OK && output == p->file &&
                      !MCObjectHeap_failed(heap));
            if (mode == 1)
                CHECK(result == I_SAVE_HANDLER_EXCEPTION && output == (MCObject *)p &&
                      !MCObjectHeap_failed(heap));
            if (mode == 2 || mode == 3 || mode == 4)
                CHECK(result == I_SAVE_HANDLER_FAILURE && output == (MCObject *)p &&
                      MCObjectHeap_failed(heap));
            if (mode == 4)
                CHECK(p->calls == 0 && p->lastName == NULL);
            else
                CHECK(p->calls == 1 && p->lastReceiver == (MCObject *)p && p->lastName == name &&
                      p->blocked);
        }
        CHECK(!MCObjectHeap_failed(foreign) && !MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
        MCObjectHeap_free(foreign);
    }
    MCObjectHeap *heap = MCObjectHeap_new(65536);
    CHECK(heap);
    HandlerFixture *p = (HandlerFixture *)MCObjectHeap_alloc(heap, sizeof(*p), &handlerClass);
    CHECK(p);
    NBTString *name = NBTString_fromASCII(heap, "map_4");
    CHECK(name);
    p->file = MCObjectHeap_alloc(heap, sizeof(MCObject), &fileClass);
    CHECK(p->file);
    ISaveHandler handler = {(MCObject *)p, &handlerMethods};
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, heap, (MCObject *)p));
    MCObject *file = NULL;
    CHECK(ISaveHandler_getMapFileFromName(handler, name, &file) == I_SAVE_HANDLER_OK);
    CHECK(file == p->file && p->lastReceiver == (MCObject *)p);
    CHECK(MCObjectHeap_collect(heap));
    MCObjectHeap *branch = MCObjectHeap_clone(heap);
    CHECK(branch);
    MCObjectRoot br = {0};
    CHECK(MCObjectRoot_rebind(&br, branch, &root));
    HandlerFixture *copy = (HandlerFixture *)MCObjectRoot_get(&br);
    CHECK(copy != p && copy->file != file && copy->lastReceiver == (MCObject *)copy);
    CHECK(copy->lastName != name && NBTString_equalsASCII(copy->lastName, "map_4"));
    CHECK(ISaveHandler_getMapFileFromName((ISaveHandler){(MCObject *)copy, &handlerMethods}, NULL,
                                          &file) == I_SAVE_HANDLER_OK);
    CHECK(copy->lastName == NULL && file == copy->file);
    CHECK(MCObjectHeap_adopt(heap, branch));
    MCObjectHeap_free(branch);
    copy = (HandlerFixture *)MCObjectRoot_get(&root);
    CHECK(ISaveHandler_nativeIsValid((ISaveHandler){(MCObject *)copy, &handlerMethods}));
    MCObjectRoot_drop(&root);
    CHECK(MCObjectHeap_collect(heap) && MCObjectHeap_liveObjects(heap) == 0);
    MCObjectHeap_free(heap);
    for (int mode = 0; mode < 3; ++mode) {
        heap = MCObjectHeap_new(65536);
        CHECK(heap);
        SaveHandlerMP *mp = SaveHandlerMP_new(heap);
        CHECK(mp);
        ISaveHandler view = SaveHandlerMP_asSaveHandler(mp);
        file = (MCObject *)mp;
        if (mode == 0)
            CHECK(ISaveHandler_getWorldDirectory(view, NULL) == I_SAVE_HANDLER_FAILURE);
        if (mode == 1) {
            MCObject *small = MCObjectHeap_alloc(heap, sizeof(MCObject), &handlerClass);
            CHECK(small);
            CHECK(!ISaveHandler_nativeIsValid((ISaveHandler){small, &handlerMethods}));
            CHECK(ISaveHandler_getMapFileFromName((ISaveHandler){small, &handlerMethods}, NULL,
                                                  &file) == I_SAVE_HANDLER_FAILURE &&
                  file == (MCObject *)mp);
        }
        if (mode == 2) {
            CHECK(!ISaveHandler_nativeIsValid((ISaveHandler){(MCObject *)mp, &handlerMethods}));
            CHECK(ISaveHandler_getMapFileFromName((ISaveHandler){(MCObject *)mp, &handlerMethods},
                                                  NULL, &file) == I_SAVE_HANDLER_FAILURE &&
                  file == (MCObject *)mp);
        }
        CHECK(MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
    }
    CHECK(ISaveHandler_flush((ISaveHandler){0}) == I_SAVE_HANDLER_FAILURE);
}
int main(void) {
    source_constructor();
    fieldless_mp_constructor();
    source_virtual_methods();
    abstract_and_native_guards();
    callback_context_boundary();
    saved_data_graph();
    genuine_mp_methods();
    allocation_prefixes();
    interface_status_and_lifetime();
    printf("source saved data: %d checks passed\n", checks);
    return 0;
}
