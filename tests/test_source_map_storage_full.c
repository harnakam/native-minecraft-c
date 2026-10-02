#include "world/WorldDataStorage.h"
#include "world/storage/SaveDataMemoryStorage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "full MapStorage check%u line%d: %s\n", checks, __LINE__, #x);         \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static void memory_constructor(void) {
    MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024);
    CHECK(heap);
    SaveDataMemoryStorage *memory = SaveDataMemoryStorage_nativeNewCounterProvider(heap);
    CHECK(memory);
    CHECK(memory->base.loadedDataMap);
    CHECK(memory->base.loadedDataList);
    CHECK(memory->base.idCounts);
    CHECK((MCObject *)memory->base.loadedDataMap != (MCObject *)memory->base.idCounts);
    CHECK(NativeHashMap_size(memory->base.loadedDataMap) == 0);
    CHECK(NativeReferenceList_size(memory->base.loadedDataList) == 0);
    CHECK(memory->base.saveHandler == NULL);
    CHECK(!MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}

typedef struct {
    WorldSavedData base;
    int32_t value;
} Saved;
typedef struct {
    MCObject object;
    MapStorage *storage;
    MCObject *file, *fileInput, *dataInput, *fileOutput, *dataOutput;
    NativeJavaClass *clazz;
    NBTTagCompound *raw, *compressed, *written, *writeInner;
    NBTString *lastName;
    Saved *constructed, *appendOnDirty;
    NativeHashMap *replaceMap;
    NativeReferenceList *replaceList;
    char events[512], fault;
    size_t eventCount;
    unsigned prints, constructors, equalsCalls;
    bool failure, skipCounts, missingFile, exists, replaceOnPrint, replaceOnWrite;
    int equalsMode;
    MCObject *foreignContext;
} IO;
static void saved_trace(MCObject *o, MCObjectVisitor visit, void *ctx) {
    WorldSavedData_traceFields((WorldSavedData *)o, visit, ctx);
}
static const MCObjectClass savedClass = {"test.SourceSavedData", MCObjectHeap_plainClone,
                                         saved_trace, NULL};
static bool saved_instance(const MCObject *o) {
    return o && o->klass == &savedClass && MCObjectHeap_objectSize(o) >= sizeof(Saved);
}
static const NativeJavaClassDescriptor savedDescriptor = {"test.SourceSavedData", NULL, 0,
                                                          saved_instance};
static MapStorageIOResult event(IO *io, char letter) {
    CHECK(io && io->eventCount + 1 < sizeof io->events);
    io->events[io->eventCount++] = letter;
    io->events[io->eventCount] = 0;
    MCObjectHeap_touch(io->object.heap);
    CHECK(!MCObjectHeap_collect(io->object.heap) && !MCObjectHeap_failed(io->object.heap));
    return letter != io->fault ? MAP_STORAGE_IO_OK
           : io->failure       ? MAP_STORAGE_IO_FAILURE
                               : MAP_STORAGE_IO_EXCEPTION;
}
static WorldSavedDataResult saved_status(MapStorageIOResult status) {
    return status == MAP_STORAGE_IO_OK          ? WORLD_SAVED_DATA_OK
           : status == MAP_STORAGE_IO_EXCEPTION ? WORLD_SAVED_DATA_EXCEPTION
                                                : WORLD_SAVED_DATA_FAILURE;
}
static WorldSavedDataResult saved_read(MCObject *context, WorldSavedData *data,
                                       NBTTagCompound *tag) {
    Saved *s = (Saved *)data;
    IO *io = (IO *)context;
    s->value = NBTTagCompound_getInteger_ascii(tag, "v");
    MCObjectHeap_touch(data->object.heap);
    return saved_status(event(io, 'r'));
}
static WorldSavedDataResult saved_write(MCObject *context, WorldSavedData *data,
                                        NBTTagCompound *tag) {
    IO *io = (IO *)context;
    io->writeInner = tag;
    CHECK(NBTTagCompound_setInteger_ascii(tag, "v", ((Saved *)data)->value));
    MapStorageIOResult status = event(io, 'w');
    if (io->replaceOnWrite) {
        io->storage->loadedDataList = io->replaceList;
        MCObjectHeap_touch(data->object.heap);
    }
    return saved_status(status);
}
static WorldSavedDataResult saved_dirty(MCObject *context, WorldSavedData *data, bool *out) {
    IO *io = (IO *)context;
    MapStorageIOResult status = event(io, 'd');
    if (status != MAP_STORAGE_IO_OK)
        return saved_status(status);
    if (io->appendOnDirty) {
        CHECK(NativeReferenceList_add(io->storage->loadedDataList, (MCObject *)io->appendOnDirty));
        io->appendOnDirty = NULL;
        MCObjectHeap_touch(data->object.heap);
    }
    return WorldSavedData_isDirty_base(data, out);
}
static WorldSavedDataResult saved_set(MCObject *context, WorldSavedData *data, bool value) {
    MapStorageIOResult status = event((IO *)context, 's');
    return status == MAP_STORAGE_IO_OK ? WorldSavedData_setDirty_base(data, value)
                                       : saved_status(status);
}
static const WorldSavedDataVirtualMethods savedMethods = {saved_read, saved_write, NULL, saved_set,
                                                          saved_dirty};
static const WorldSavedDataNativeType savedType = {&savedClass, sizeof(Saved), &savedMethods};
static Saved *saved_new(MCObjectHeap *heap, IO *io, NBTString *name, int32_t value) {
    Saved *s = (Saved *)WorldSavedData_nativeAllocate(heap, &savedType, (MCObject *)io);
    CHECK(s);
    CHECK(WorldSavedData_construct(&s->base, name));
    s->value = value;
    MCObjectHeap_touch(heap);
    return s;
}
static void io_trace(MCObject *o, MCObjectVisitor v, void *c) {
    IO *io = (IO *)o;
    io->storage = (MapStorage *)v((MCObject *)io->storage, c);
    io->file = v(io->file, c);
    io->fileInput = v(io->fileInput, c);
    io->dataInput = v(io->dataInput, c);
    io->fileOutput = v(io->fileOutput, c);
    io->dataOutput = v(io->dataOutput, c);
    io->clazz = (NativeJavaClass *)v((MCObject *)io->clazz, c);
    io->raw = (NBTTagCompound *)v((MCObject *)io->raw, c);
    io->compressed = (NBTTagCompound *)v((MCObject *)io->compressed, c);
    io->written = (NBTTagCompound *)v((MCObject *)io->written, c);
    io->writeInner = (NBTTagCompound *)v((MCObject *)io->writeInner, c);
    io->lastName = (NBTString *)v((MCObject *)io->lastName, c);
    io->constructed = (Saved *)v((MCObject *)io->constructed, c);
    io->appendOnDirty = (Saved *)v((MCObject *)io->appendOnDirty, c);
    io->replaceMap = (NativeHashMap *)v((MCObject *)io->replaceMap, c);
    io->replaceList = (NativeReferenceList *)v((MCObject *)io->replaceList, c);
    /* foreignContext is solely a rejected native-boundary test input. It is
       never an owning field of the successful source graph. */
}
static const MCObjectClass ioClass = {"test.MapStorageIO", MCObjectHeap_plainClone, io_trace, NULL};
static const MCObjectClass tokenClass = {"test.NativeFileStream", MCObjectHeap_plainClone, NULL,
                                         NULL};
static bool io_instance(const MCObject *o) {
    return o && o->klass == &ioClass && MCObjectHeap_objectSize(o) >= sizeof(IO);
}
static ISaveHandlerResult file_name(MCObject *o, NBTString *name, MCObject **out) {
    IO *io = (IO *)o;
    io->lastName = name;
    MapStorageIOResult r = event(io, 'F');
    if (r == MAP_STORAGE_IO_OK)
        *out = io->missingFile || (io->skipCounts && NBTString_equalsASCII(name, "idcounts"))
                   ? NULL
                   : io->file;
    return r == MAP_STORAGE_IO_OK          ? I_SAVE_HANDLER_OK
           : r == MAP_STORAGE_IO_EXCEPTION ? I_SAVE_HANDLER_EXCEPTION
                                           : I_SAVE_HANDLER_FAILURE;
}
static const ISaveHandlerMethods handlerMethods = {.isInstance = io_instance,
                                                   .getMapFileFromName = file_name};
static MapStorageIOResult exists(MCObject *o, MCObject *file, bool *out) {
    IO *io = (IO *)o;
    CHECK(file == io->file);
    MapStorageIOResult r = event(io, 'E');
    if (r == MAP_STORAGE_IO_OK)
        *out = io->exists;
    return r;
}
static MapStorageIOResult construct(MCObject *o, NativeJavaClass *clazz, NBTString *name,
                                    WorldSavedData **out) {
    IO *io = (IO *)o;
    ++io->constructors;
    MapStorageIOResult r = event(io, 'K');
    if (r != MAP_STORAGE_IO_OK)
        return r;
    if (clazz != io->clazz)
        return MAP_STORAGE_IO_EXCEPTION;
    io->constructed = saved_new(o->heap, io, name, 7);
    *out = &io->constructed->base;
    return MAP_STORAGE_IO_OK;
}
static MapStorageIOResult file_input(MCObject *o, MCObject *file, MCObject **out) {
    IO *io = (IO *)o;
    CHECK(file == io->file);
    MapStorageIOResult r = event(io, 'I');
    if (r == MAP_STORAGE_IO_OK)
        *out = io->fileInput;
    return r;
}
static MapStorageIOResult data_input(MCObject *o, MCObject *input, MCObject **out) {
    IO *io = (IO *)o;
    CHECK(input == io->fileInput);
    MapStorageIOResult r = event(io, 'D');
    if (r == MAP_STORAGE_IO_OK)
        *out = io->dataInput;
    return r;
}
static MapStorageIOResult raw_read(MCObject *o, MCObject *input, NBTTagCompound **out) {
    IO *io = (IO *)o;
    CHECK(input == io->dataInput);
    MapStorageIOResult r = event(io, 'R');
    if (r == MAP_STORAGE_IO_OK)
        *out = io->raw;
    return r;
}
static MapStorageIOResult compressed_read(MCObject *o, MCObject *input, NBTTagCompound **out) {
    IO *io = (IO *)o;
    CHECK(input == io->fileInput);
    MapStorageIOResult r = event(io, 'G');
    if (r == MAP_STORAGE_IO_OK)
        *out = io->compressed;
    return r;
}
static MapStorageIOResult input_close(MCObject *o, MCObject *input) {
    IO *io = (IO *)o;
    CHECK(input == io->fileInput || input == io->dataInput);
    return event(io, 'X');
}
static MapStorageIOResult file_output(MCObject *o, MCObject *file, MCObject **out) {
    IO *io = (IO *)o;
    CHECK(file == io->file);
    MapStorageIOResult r = event(io, 'O');
    if (r == MAP_STORAGE_IO_OK)
        *out = io->fileOutput;
    return r;
}
static MapStorageIOResult data_output(MCObject *o, MCObject *input, MCObject **out) {
    IO *io = (IO *)o;
    CHECK(input == io->fileOutput);
    MapStorageIOResult r = event(io, 'T');
    if (r == MAP_STORAGE_IO_OK)
        *out = io->dataOutput;
    return r;
}
static MapStorageIOResult raw_write(MCObject *o, NBTTagCompound *tag, MCObject *output) {
    IO *io = (IO *)o;
    CHECK(output == io->dataOutput);
    io->written = tag;
    return event(io, 'U');
}
static MapStorageIOResult compressed_write(MCObject *o, NBTTagCompound *tag, MCObject *output) {
    IO *io = (IO *)o;
    CHECK(output == io->fileOutput);
    io->written = tag;
    return event(io, 'Z');
}
static MapStorageIOResult output_close(MCObject *o, MCObject *output) {
    IO *io = (IO *)o;
    CHECK(output == io->fileOutput || output == io->dataOutput);
    return event(io, 'C');
}
static MapStorageIOResult equals_data(MCObject *o, MCObject *query, MCObject *stored, bool *out) {
    IO *io = (IO *)o;
    ++io->equalsCalls;
    CHECK(query);
    MapStorageIOResult r = event(io, 'q');
    if (r != MAP_STORAGE_IO_OK)
        return r;
    *out = io->equalsMode ? io->equalsMode > 0
           : NBTString_isInstance(query)
               ? NBTString_equals((NBTString *)query,
                                  NBTString_isInstance(stored) ? (NBTString *)stored : NULL)
               : query == stored;
    if (io->foreignContext) {
        io->storage->nativeContext = io->foreignContext;
        MCObjectHeap_touch(o->heap);
    }
    return MAP_STORAGE_IO_OK;
}
static bool print_exception(MCObject *o) {
    IO *io = (IO *)o;
    ++io->prints;
    CHECK(event(io, 'P') == MAP_STORAGE_IO_OK);
    if (io->replaceOnPrint) {
        io->storage->loadedDataMap = io->replaceMap;
        io->storage->loadedDataList = io->replaceList;
        MCObjectHeap_touch(o->heap);
    }
    if (io->foreignContext) {
        io->storage->nativeContext = io->foreignContext;
        MCObjectHeap_touch(o->heap);
    }
    return true;
}
static const MapStorageSourceDependencies dependencies = {
    exists,           construct,    file_input,  data_input,     raw_read,
    compressed_read,  input_close,  file_output, data_output,    raw_write,
    compressed_write, output_close, equals_data, print_exception};
static NBTString *text(MCObjectHeap *h, const char *s) {
    NBTString *out = NBTString_fromASCII(h, s);
    CHECK(out);
    return out;
}
static IO *fixture(MCObjectHeap *heap, MCObjectRoot *root) {
    IO *io = (IO *)MCObjectHeap_alloc(heap, sizeof(*io), &ioClass);
    CHECK(io);
    io->file = MCObjectHeap_alloc(heap, sizeof(MCObject), &tokenClass);
    io->fileInput = MCObjectHeap_alloc(heap, sizeof(MCObject), &tokenClass);
    io->dataInput = MCObjectHeap_alloc(heap, sizeof(MCObject), &tokenClass);
    io->fileOutput = MCObjectHeap_alloc(heap, sizeof(MCObject), &tokenClass);
    io->dataOutput = MCObjectHeap_alloc(heap, sizeof(MCObject), &tokenClass);
    CHECK(io->file && io->fileInput && io->dataInput && io->fileOutput && io->dataOutput);
    io->raw = NBTTagCompound_new(heap);
    io->compressed = NBTTagCompound_new(heap);
    CHECK(io->raw && io->compressed);
    NBTTagCompound *inner = NBTTagCompound_new(heap);
    CHECK(inner);
    CHECK(NBTTagCompound_setInteger_ascii(inner, "v", 37));
    CHECK(NBTTagCompound_setTag_ascii(io->compressed, "data", (NBTBase *)inner));
    io->clazz = NativeJavaClass_literal(heap, &savedDescriptor);
    CHECK(io->clazz);
    io->skipCounts = true;
    io->exists = true;
    io->storage = MapStorage_nativeAllocate(heap, (MCObject *)io, &dependencies);
    CHECK(io->storage);
    CHECK(MapStorage_construct(io->storage, (ISaveHandler){(MCObject *)io, &handlerMethods}));
    CHECK(strcmp(io->events, "F") == 0);
    io->eventCount = 0;
    io->events[0] = 0;
    CHECK(MCObjectRoot_init(root, heap, (MCObject *)io->storage));
    return io;
}
static void reset(IO *io) {
    io->eventCount = 0;
    io->events[0] = 0;
    io->prints = 0;
    io->equalsCalls = 0;
}
static void cache_and_memory(void) {
    MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
    MCObjectRoot root = {0};
    IO *io = fixture(h, &root);
    NBTString *a = text(h, "a"), *b = text(h, "b"), *equal = text(h, "a");
    Saved *first = saved_new(h, io, a, 11), *second = saved_new(h, io, a, 22);
    CHECK(MapStorage_setData(io->storage, a, &first->base));
    CHECK(MapStorage_setData(io->storage, b, &first->base));
    CHECK(NativeReferenceList_size(io->storage->loadedDataList) == 2);
    CHECK(MapStorage_setData(io->storage, equal, &second->base));
    CHECK(strcmp(io->events, "q") == 0 && io->equalsCalls == 1);
    CHECK(NativeReferenceList_get(io->storage->loadedDataList, 0) == (MCObject *)first);
    CHECK(NativeReferenceList_get(io->storage->loadedDataList, 1) == (MCObject *)second);
    WorldSavedData *out = NULL;
    reset(io);
    CHECK(MapStorage_loadData(io->storage, NULL, a, &out) && out == &second->base &&
          io->events[0] == 0);
    CHECK(MapStorage_setData(io->storage, NULL, NULL));
    CHECK(MapStorage_loadData(io->storage, NULL, NULL, &out) &&
          out == NULL); /* NULL cache entry is a miss. */
    CHECK(strcmp(io->events, "FEKP") == 0);
    reset(io);
    CHECK(MapStorage_setData(io->storage, NULL, &first->base));
    CHECK(io->equalsCalls == 0); /* remove(NULL) bypasses virtual equals. */
    io->equalsMode = -1;
    reset(io);
    CHECK(MapStorage_setData(io->storage, b, &second->base));
    CHECK(io->equalsCalls == 3 && NativeReferenceList_size(io->storage->loadedDataList) == 4);
    SaveDataMemoryStorage *memory = SaveDataMemoryStorage_new(h);
    CHECK(memory);
    CHECK(MapStorage_setData(&memory->base, a, &first->base));
    CHECK(MapStorage_setData(&memory->base, b, &first->base));
    CHECK(MapStorage_setData(&memory->base, equal, &second->base));
    CHECK(MapStorage_setData(&memory->base, NULL, NULL));
    CHECK(MapStorage_loadData(&memory->base, NULL, a, &out) && out == &second->base);
    CHECK(MapStorage_loadData(&memory->base, NULL, NULL, &out) && out == NULL);
    CHECK(NativeReferenceList_size(memory->base.loadedDataList) == 0);
    first->base.dirty = true;
    second->base.dirty = true;
    reset(io);
    memory->base.loadedDataList = NULL;
    memory->base.idCounts = NULL;
    CHECK(MapStorage_saveAllData(&memory->base) && first->base.dirty && second->base.dirty &&
          io->events[0] == 0);
    int32_t id = 99;
    CHECK(MapStorage_getUniqueDataId(&memory->base, NULL, &id) && id == 0);
    MCObjectRoot_drop(&root);
    MCObjectHeap_free(h);
}
static void load_prefixes(void) {
    const char letters[] = {0, 'F', 'E', 'K', 'I', 'G', 'X', 'r'};
    const char *events[] = {"FEKIGXr", "FP",     "FEP",     "FEKP",
                            "FEKIP",   "FEKIGP", "FEKIGXP", "FEKIGXrP"};
    for (size_t i = 0; i < sizeof letters; i++)
        for (int fatal = 0; fatal < 2; fatal++) {
            if (!letters[i] && fatal)
                continue;
            MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
            MCObjectRoot root = {0};
            IO *io = fixture(h, &root);
            NBTString *name = text(h, "map_2");
            io->fault = letters[i];
            io->failure = fatal;
            WorldSavedData *out = (WorldSavedData *)io;
            bool ok = MapStorage_loadData(io->storage, io->clazz, name, &out);
            CHECK(ok == !fatal);
            CHECK(MCObjectHeap_failed(h) == (fatal != 0));
            if (!fatal) {
                CHECK(strcmp(io->events, events[i]) == 0);
                CHECK(io->prints == (letters[i] != 0));
                if (i > 0 && i < 4)
                    CHECK(out == NULL && NativeHashMap_size(io->storage->loadedDataMap) == 0);
                else {
                    CHECK(out == &io->constructed->base);
                    CHECK(io->constructed->value == (!letters[i] || letters[i] == 'r' ? 37 : 7));
                    CHECK(NativeHashMap_get(io->storage->loadedDataMap, (MCObject *)name) ==
                          (MCObject *)out);
                    CHECK(NativeReferenceList_size(io->storage->loadedDataList) == 1);
                    io->fault = 0;
                    reset(io);
                    CHECK(MapStorage_loadData(io->storage, NULL, name, &out) &&
                          out == &io->constructed->base && io->events[0] == 0);
                }
            } else
                CHECK(out == (WorldSavedData *)io && io->prints == 0 &&
                      io->storage->loadedDataList->size == 0);
            CHECK(!MCObjectHeap_hasBorrowers(h));
            MCObjectRoot_drop(&root);
            MCObjectHeap_free(h);
        }
    for (int missing = 0; missing < 2; missing++) {
        MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
        MCObjectRoot root = {0};
        IO *io = fixture(h, &root);
        io->missingFile = missing != 0;
        io->exists = false;
        WorldSavedData *out = (WorldSavedData *)io;
        CHECK(MapStorage_loadData(io->storage, NULL, NULL, &out) && out == NULL &&
              io->constructors == 0);
        CHECK(strcmp(io->events, missing ? "F" : "FE") == 0);
        MCObjectRoot_drop(&root);
        MCObjectHeap_free(h);
    }
    MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
    MCObjectRoot root = {0};
    IO *io = fixture(h, &root);
    NativeHashMap *oldMap = io->storage->loadedDataMap;
    NativeReferenceList *oldList = io->storage->loadedDataList;
    io->replaceMap = NativeHashMap_new(h, NATIVE_HASH_KEY_STRING);
    io->replaceList = NativeReferenceList_new(h);
    CHECK(io->replaceMap && io->replaceList);
    io->replaceOnPrint = true;
    io->fault = 'r';
    WorldSavedData *out = NULL;
    NBTString *name = text(h, "replace");
    CHECK(MapStorage_loadData(io->storage, io->clazz, name, &out) && out == &io->constructed->base);
    CHECK(NativeHashMap_size(oldMap) == 0 && NativeReferenceList_size(oldList) == 0);
    CHECK(NativeHashMap_get(io->replaceMap, (MCObject *)name) == (MCObject *)out &&
          NativeReferenceList_size(io->replaceList) == 1);
    MCObjectRoot_drop(&root);
    MCObjectHeap_free(h);
}
static void save_prefixes(void) {
    const char letters[] = {0, 'F', 'w', 'O', 'Z', 'C'};
    const char *events[] = {"dFwOZCs", "dFPs", "dFwPs", "dFwOPs", "dFwOZPs", "dFwOZCPs"};
    for (size_t i = 0; i < sizeof letters; i++)
        for (int fatal = 0; fatal < 2; fatal++) {
            if (!letters[i] && fatal)
                continue;
            MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
            MCObjectRoot root = {0};
            IO *io = fixture(h, &root);
            Saved *s = saved_new(h, io, text(h, "stored-name"), 19);
            s->base.dirty = true;
            CHECK(MapStorage_setData(io->storage, text(h, "different-cache-key"), &s->base));
            io->fault = letters[i];
            io->failure = fatal;
            CHECK(MapStorage_saveAllData(io->storage) == !fatal);
            CHECK(s->base.dirty == (fatal != 0) && MCObjectHeap_failed(h) == (fatal != 0));
            CHECK(NBTString_equalsASCII(io->lastName, "stored-name"));
            if (!fatal) {
                CHECK(strcmp(io->events, events[i]) == 0);
                CHECK(io->prints == (letters[i] != 0));
            } else
                CHECK(io->prints == 0);
            if (!letters[i])
                CHECK(NBTTagCompound_getInteger_ascii(
                          NBTTagCompound_getCompoundTag_ascii(io->written, "data"), "v") == 19);
            CHECK(!MCObjectHeap_hasBorrowers(h));
            MCObjectRoot_drop(&root);
            MCObjectHeap_free(h);
        }
    for (int noHandler = 0; noHandler < 2; noHandler++) {
        MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
        MCObjectRoot root = {0};
        IO *io = fixture(h, &root);
        Saved *s = saved_new(h, io, NULL, 0);
        s->base.dirty = true;
        CHECK(MapStorage_setData(io->storage, NULL, &s->base));
        if (noHandler)
            io->storage->saveHandler = NULL;
        else
            io->missingFile = true;
        CHECK(MapStorage_saveAllData(io->storage) && !s->base.dirty &&
              strcmp(io->events, noHandler ? "ds" : "dFs") == 0);
        MCObjectRoot_drop(&root);
        MCObjectHeap_free(h);
    }
}
static void live_list_and_equals(void) {
    MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
    MCObjectRoot root = {0};
    IO *io = fixture(h, &root);
    Saved *first = saved_new(h, io, text(h, "first"), 1),
          *second = saved_new(h, io, text(h, "second"), 2);
    first->base.dirty = true;
    second->base.dirty = true;
    CHECK(MapStorage_setData(io->storage, first->base.mapName, &first->base));
    io->appendOnDirty = second;
    io->missingFile = true;
    CHECK(MapStorage_saveAllData(io->storage) && !first->base.dirty && !second->base.dirty);
    CHECK(strcmp(io->events, "dFsdFs") == 0 &&
          NativeReferenceList_size(io->storage->loadedDataList) == 2);
    reset(io);
    first->base.dirty = true;
    CHECK(NativeReferenceList_add(io->storage->loadedDataList, (MCObject *)first));
    CHECK(MapStorage_saveAllData(io->storage) && strcmp(io->events, "dFsdd") == 0);
    io->missingFile = false;
    first->base.dirty = true;
    second->base.dirty = true;
    io->replaceList = NativeReferenceList_new(h);
    CHECK(io->replaceList);
    CHECK(NativeReferenceList_add(io->replaceList, (MCObject *)first));
    CHECK(NativeReferenceList_add(io->replaceList, (MCObject *)second));
    io->replaceOnWrite = true;
    reset(io);
    CHECK(MapStorage_saveAllData(io->storage) && !first->base.dirty && !second->base.dirty);
    CHECK(strcmp(io->events, "dFwOZCsdFwOZCs") == 0);
    /* No checkcast occurs for the removed map value: String query dispatches
       its real equals and leaves unrelated saved-data occurrences in place. */
    io->replaceOnWrite = false;
    NBTString *key = text(h, "corrupt"), *query = text(h, "query");
    CHECK(NativeHashMap_put(io->storage->loadedDataMap, (MCObject *)key, (MCObject *)query));
    reset(io);
    CHECK(MapStorage_setData(io->storage, key, &first->base));
    CHECK(io->equalsCalls == 2 && NativeReferenceList_size(io->storage->loadedDataList) == 3);
    MCObjectRoot_drop(&root);
    MCObjectHeap_free(h);
    MCObjectHeap *other = MCObjectHeap_new(1024);
    CHECK(other);
    MCObject *foreign = MCObjectHeap_alloc(other, sizeof(MCObject), &tokenClass);
    CHECK(foreign);
    h = MCObjectHeap_new(1024 * 1024);
    io = fixture(h, &root);
    first = saved_new(h, io, text(h, "one"), 1);
    second = saved_new(h, io, text(h, "two"), 2);
    CHECK(MapStorage_setData(io->storage, first->base.mapName, &first->base));
    CHECK(MapStorage_setData(io->storage, second->base.mapName, &second->base));
    io->equalsMode = -1;
    io->foreignContext = foreign;
    CHECK(!MapStorage_setData(io->storage, first->base.mapName, &second->base));
    CHECK(io->equalsCalls == 1 && MCObjectHeap_failed(h));
    CHECK(io->storage->loadedDataList->size == 2); /* remove prefix, no reappend. */
    CHECK(!MCObjectHeap_hasBorrowers(h));
    MCObjectRoot_drop(&root);
    MCObjectHeap_free(h);
    MCObjectHeap_free(other);
}
static void raw_counts_and_graph(void) {
    MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
    MCObjectRoot root = {0};
    IO *io = fixture(h, &root);
    CHECK(NBTTagCompound_setShort_ascii(io->raw, "map", -4));
    CHECK(NBTTagCompound_setInteger_ascii(io->raw, "ignored", 100));
    io->skipCounts = false;
    reset(io);
    CHECK(MapStorage_construct(io->storage, (ISaveHandler){(MCObject *)io, &handlerMethods}));
    CHECK(strcmp(io->events, "FEIDRX") == 0 && MapStorage_nativeIdCountSize(io->storage) == 1);
    int32_t id = 99;
    reset(io);
    CHECK(MapStorage_getUniqueDataId(io->storage, text(h, "map"), &id) && id == -3);
    CHECK(strcmp(io->events, "FOTUC") == 0 &&
          NBTTagCompound_getShort_ascii(io->written, "map") == -3);
    CHECK(!NBTTagCompound_hasKey_ascii(io->written, "data"));
    Saved *s = saved_new(h, io, text(h, "shared"), 3);
    CHECK(MapStorage_setData(io->storage, s->base.mapName, &s->base));
    CHECK(MapStorage_setData(io->storage, text(h, "alias"), &s->base));
    CHECK(MCObjectHeap_collect(h));
    MCObjectHeap *copy = MCObjectHeap_clone(h);
    CHECK(copy);
    MCObjectRoot branch = {0};
    CHECK(MCObjectRoot_rebind(&branch, copy, &root));
    MapStorage *storage = (MapStorage *)MCObjectRoot_get(&branch);
    IO *work = (IO *)storage->nativeContext;
    CHECK(work != io && work->storage == storage && storage->saveHandler == (MCObject *)work);
    Saved *cloned = (Saved *)NativeReferenceList_get(storage->loadedDataList, 0);
    CHECK(cloned != s && cloned->base.nativeContext == (MCObject *)work &&
          WorldSavedData_isInstance((MCObject *)cloned));
    CHECK(NativeReferenceList_get(storage->loadedDataList, 1) == (MCObject *)cloned);
    CHECK(NativeHashMap_get(storage->loadedDataMap, (MCObject *)cloned->base.mapName) ==
          (MCObject *)cloned);
    cloned->value = 44;
    MCObjectHeap_touch(copy);
    CHECK(s->value == 3);
    CHECK(MCObjectHeap_canAdopt(h, copy) && MCObjectHeap_adopt(h, copy));
    MCObjectHeap_free(copy);
    storage = (MapStorage *)MCObjectRoot_get(&root);
    work = (IO *)storage->nativeContext;
    cloned = (Saved *)NativeReferenceList_get(storage->loadedDataList, 0);
    CHECK(cloned->value == 44 && cloned->base.nativeContext == (MCObject *)work &&
          work->storage == storage);
    CHECK(MCObjectHeap_collect(h) && !MCObjectHeap_failed(h));
    MCObjectRoot_drop(&root);
    CHECK(MCObjectHeap_collect(h));
    CHECK(MCObjectHeap_liveObjects(h) < 10); /* immutable native class fact roots remain. */
    MCObjectHeap_free(h);
}
static void constructor_failure_prefixes(void) {
    unsigned partialMap = 0, partialList = 0, succeeded = 0;
    for (size_t budget = 128; budget < 4096; budget += 16) {
        MCObjectHeap *h = MCObjectHeap_new(budget);
        CHECK(h);
        MapStorage *s = MapStorage_nativeAllocate(h, NULL, NULL);
        if (s) {
            bool ok = MapStorage_construct(s, (ISaveHandler){NULL, NULL});
            if (ok) {
                CHECK(s->loadedDataMap && s->loadedDataList && s->idCounts);
                ++succeeded;
            } else {
                CHECK(MCObjectHeap_failed(h));
                CHECK(!s->idCounts || s->loadedDataList);
                CHECK(!s->loadedDataList || s->loadedDataMap);
                CHECK(s->saveHandler == NULL);
                if (s->loadedDataMap && !s->loadedDataList)
                    ++partialMap;
                if (s->loadedDataList && !s->idCounts)
                    ++partialList;
            }
        } else
            CHECK(MCObjectHeap_failed(h));
        CHECK(!MCObjectHeap_hasBorrowers(h));
        MCObjectHeap_free(h);
    }
    CHECK(partialMap && partialList && succeeded);
}
static void world_delegation(void) {
    MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
    MCObjectRoot root = {0};
    IO *io = fixture(h, &root);
    World *world = World_nativeAllocate(h, NULL, NULL);
    CHECK(world);
    world->mapStorage = (MapStorage *)SaveDataMemoryStorage_new(h);
    CHECK(world->mapStorage);
    Saved *first = saved_new(h, io, text(h, "first"), 1),
          *second = saved_new(h, io, text(h, "second"), 2);
    NBTString *key = text(h, "key");
    WorldSavedData *out = NULL;
    CHECK(World_setItemData(world, key, &first->base));
    CHECK(World_loadItemData(world, NULL, key, &out) && out == &first->base);
    CHECK(World_setItemData(world, text(h, "key"), &second->base));
    CHECK(World_loadItemData(world, NULL, key, &out) && out == &second->base);
    CHECK(first != second && first->value == 1);
    CHECK(NativeReferenceList_size(world->mapStorage->loadedDataList) == 0);
    world->mapStorage = io->storage;
    CHECK(World_setItemData(world, key, &first->base));
    CHECK(World_loadItemData(world, NULL, key, &out) && out == &first->base);
    CHECK(NativeReferenceList_size(io->storage->loadedDataList) == 1);
    MCObjectRoot_drop(&root);
    MCObjectHeap_free(h);
}
static void final_callback_owner_guards(void) {
    MCObjectHeap *other = MCObjectHeap_new(1024);
    CHECK(other);
    MCObject *foreign = MCObjectHeap_alloc(other, sizeof(MCObject), &tokenClass);
    CHECK(foreign);
    for (int printer = 0; printer < 2; printer++) {
        MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
        MCObjectRoot root = {0};
        IO *io = fixture(h, &root);
        NBTString *key = text(h, "terminal");
        if (printer) {
            io->fault = 'r';
            io->foreignContext = foreign;
            WorldSavedData *out = (WorldSavedData *)io;
            CHECK(!MapStorage_loadData(io->storage, io->clazz, key, &out) &&
                  out == (WorldSavedData *)io);
            CHECK(io->prints == 1 && io->constructed->value == 37 &&
                  io->storage->loadedDataList->size == 0);
        } else {
            Saved *s = saved_new(h, io, key, 1);
            CHECK(MapStorage_setData(io->storage, key, &s->base));
            io->equalsMode = 1;
            io->foreignContext = foreign;
            CHECK(!MapStorage_setData(io->storage, key, &s->base));
            CHECK(io->equalsCalls == 1 && io->storage->loadedDataList->size == 1);
        }
        CHECK(MCObjectHeap_failed(h) && !MCObjectHeap_failed(other) &&
              !MCObjectHeap_hasBorrowers(h));
        CHECK(io->storage->nativeContext == foreign); /* rejected prefix remains. */
        MCObjectRoot_drop(&root);
        MCObjectHeap_free(h);
    }
    MCObjectHeap_free(other);
}
static void native_import_class_guard(void) {
    MCObjectHeap *h = MCObjectHeap_new(131072);
    CHECK(h);
    MapStorage *s = MapStorage_new(h, (ISaveHandler){NULL, NULL}, NULL, NULL);
    CHECK(s);
    CHECK(MapStorage_nativeImportExactShort(s, text(h, "map"), 5));
    NBTBase *wrong = (NBTBase *)MCObjectHeap_alloc(h, sizeof(NBTBase), &tokenClass);
    CHECK(wrong);
    wrong->type = 10;
    CHECK(!NBTTagCompound_isInstance((MCObject *)wrong));
    CHECK(!MapStorage_nativeImportIdCounts(s, (NBTTagCompound *)wrong, true));
    CHECK(MCObjectHeap_failed(h));
    int32_t next = -1;
    CHECK(MapStorage_nativeGetMapNextProjectionDiagnostic(s, &next) && next == 6);
    CHECK(!MCObjectHeap_hasBorrowers(h));
    MCObjectHeap_free(h);
}
static bool tracking_string_hash(MCObject *context, MCObject *key, int32_t *out) {
    CHECK(NBTString_isInstance(key));
    CHECK(event((IO *)context, 'h') == MAP_STORAGE_IO_OK);
    *out = NBTString_hashCode((NBTString *)key);
    return true;
}
static bool tracking_string_equals(MCObject *context, MCObject *query, MCObject *stored,
                                   bool *out) {
    (void)context;
    CHECK(NBTString_isInstance(query) && NBTString_isInstance(stored));
    *out = NBTString_equals((NBTString *)query, (NBTString *)stored);
    return true;
}
static const NativeHashKeyMethods trackingStringKeys = {tracking_string_hash,
                                                        tracking_string_equals};
static void null_list_argument_order(void) {
    MCObjectHeap *h = MCObjectHeap_new(131072);
    MCObjectRoot root = {0};
    IO *io = fixture(h, &root);
    io->storage->loadedDataMap = NativeHashMap_newWithKeys(h, &trackingStringKeys, (MCObject *)io);
    CHECK(io->storage->loadedDataMap);
    NBTString *key = text(h, "key");
    Saved *s = saved_new(h, io, key, 1);
    CHECK(MapStorage_setData(io->storage, key, &s->base));
    io->storage->loadedDataList = NULL;
    reset(io);
    CHECK(!MapStorage_setData(io->storage, key, &s->base));
    /* Same real String hash contract records containsKey then map.remove.
       NULL list invocation occurs only after the argument evaluation. */
    CHECK(strcmp(io->events, "hh") == 0 && MCObjectHeap_failed(h));
    CHECK(io->equalsCalls == 0 && !MCObjectHeap_hasBorrowers(h));
    MCObjectRoot_drop(&root);
    MCObjectHeap_free(h);
}
int main(void) {
    memory_constructor();
    cache_and_memory();
    load_prefixes();
    save_prefixes();
    live_list_and_equals();
    raw_counts_and_graph();
    constructor_failure_prefixes();
    world_delegation();
    final_callback_owner_guards();
    native_import_class_guard();
    null_list_argument_order();
    printf("full Source MapStorage: %u checks passed\n", checks);
    return 0;
}
