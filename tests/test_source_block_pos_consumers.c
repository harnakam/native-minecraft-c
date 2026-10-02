#include "entity/DataWatcher.h"
#include "network/play/client/C07PacketPlayerDigging.h"
#include "network/play/client/C08PacketPlayerBlockPlacement.h"
#include "util/BlockPosMutableBlockPos.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr, "position consumer check %u line %d: %s\n", checks, __LINE__, #x); \
    exit(1); } } while (0)

typedef struct {
    MCObject object;
    DataWatcher *watcher;
    C08PacketPlayerBlockPlacement *placement;
    C07PacketPlayerDigging *digging;
    NativeTypedObjectArray *positions;
    BlockPosMutableBlockPos *position;
    unsigned calls;
} Owner;
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
    Owner *owner = (Owner *)o;
    owner->watcher = (DataWatcher *)v((MCObject *)owner->watcher, c);
    owner->placement = (C08PacketPlayerBlockPlacement *)v((MCObject *)owner->placement, c);
    owner->digging = (C07PacketPlayerDigging *)v((MCObject *)owner->digging, c);
    owner->positions = (NativeTypedObjectArray *)v((MCObject *)owner->positions, c);
    owner->position = (BlockPosMutableBlockPos *)v((MCObject *)owner->position, c);
}
static const MCObjectClass ownerClass = {"test.positionConsumers.Owner",
    MCObjectHeap_plainClone, trace, NULL};
static bool notified(MCObject *context, MCObject *o, int32_t id) {
    Owner *owner = (Owner *)o;
    CHECK(context == o && id == 6 && MCObjectHeap_hasBorrowers(o->heap));
    ++owner->calls;
    return true;
}
static const DataWatcherDependencies dependencies = {notified, NULL};
static MCObjectHeap *heap(void) {
    MCObjectHeap *h = MCObjectHeap_new(4 * 1024 * 1024); CHECK(h); return h;
}
static Owner *owner_new(MCObjectHeap *h) {
    Owner *o = (Owner *)MCObjectHeap_alloc(h, sizeof(*o), &ownerClass); CHECK(o);
    o->watcher = DataWatcher_new(h, (MCObject *)o, &dependencies, (MCObject *)o);
    CHECK(o->watcher); return o;
}
static void coords(BlockPos *p, int32_t x, int32_t y, int32_t z) {
    int32_t n = 99;
    CHECK(Vec3i_getX((Vec3i *)p, &n) == NATIVE_ARRAY_OK && n == x);
    CHECK(Vec3i_getY((Vec3i *)p, &n) == NATIVE_ARRAY_OK && n == y);
    CHECK(Vec3i_getZ((Vec3i *)p, &n) == NATIVE_ARRAY_OK && n == z);
}
static void change(BlockPosMutableBlockPos *p, int32_t x, int32_t y, int32_t z) {
    BlockPosMutableBlockPos *out = NULL;
    CHECK(BlockPosMutableBlockPos_set(p, x, y, z, &out) == NATIVE_ARRAY_OK && out == p);
}
static void class_and_arrays(void) {
    MCObjectHeap *h = heap();
    Vec3i *v = Vec3i_newInt(h, 1, 2, 3);
    BlockPos *p = BlockPos_newInt(h, 1, 2, 3);
    BlockPosMutableBlockPos *m = BlockPosMutableBlockPos_newInt(h, 4, 5, 6);
    CHECK(v && p && m);
    /* getClass before any explicit class literal/static request. */
    NativeJavaClass *vc = NativeJavaClass_getClass(h, (MCObject *)v);
    NativeJavaClass *pc = NativeJavaClass_getClass(h, (MCObject *)p);
    NativeJavaClass *mc = NativeJavaClass_getClass(h, (MCObject *)m);
    CHECK(vc && pc && mc);
    CHECK(vc->descriptor == &Vec3i_Class && pc->descriptor == &BlockPos_Class &&
          mc->descriptor == &BlockPosMutableBlockPos_Class);
    CHECK(Vec3i_isInstance((MCObject *)m) && BlockPos_isInstance((MCObject *)m));
    CHECK(!Vec3i_isRuntimeClass((MCObject *)m) && !BlockPos_isRuntimeClass((MCObject *)m));
    CHECK(BlockPosMutableBlockPos_isRuntimeClass((MCObject *)m));
    bool yes = false;
    CHECK(NativeJavaClass_isAssignableFrom(vc, pc, &yes) && yes);
    CHECK(NativeJavaClass_isAssignableFrom(pc, mc, &yes) && yes);
    CHECK(NativeJavaClass_isAssignableFrom(mc, pc, &yes) && !yes);
    NativeTypedObjectArray *array = NULL;
    CHECK(NativeTypedObjectArray_new(h, pc, 2, &array) == NATIVE_ARRAY_OK && array);
    CHECK(NativeTypedObjectArray_set(array, 0, (MCObject *)p) == NATIVE_ARRAY_OK);
    CHECK(NativeTypedObjectArray_set(array, 1, (MCObject *)m) == NATIVE_ARRAY_OK);
    MCObject *out = NULL;
    CHECK(NativeTypedObjectArray_get(array, 1, &out) == NATIVE_ARRAY_OK && out == (MCObject *)m);
    NativeTypedObjectArray *mutableArray = NULL;
    CHECK(NativeTypedObjectArray_new(h, mc, 1, &mutableArray) == NATIVE_ARRAY_OK);
    CHECK(NativeTypedObjectArray_set(mutableArray, 0, (MCObject *)m) == NATIVE_ARRAY_OK);
    CHECK(NativeTypedObjectArray_set(mutableArray, 0, (MCObject *)p) == NATIVE_ARRAY_EXCEPTION);
    CHECK(!MCObjectHeap_failed(h));
    CHECK(NativeTypedObjectArray_set(array, 1, (MCObject *)v) == NATIVE_ARRAY_EXCEPTION);
    CHECK(!MCObjectHeap_failed(h));
    CHECK(NativeTypedObjectArray_get(array, 1, &out) == NATIVE_ARRAY_OK && out == (MCObject *)m);
    coords((BlockPos *)m, 4, 5, 6);
    CHECK(m->blockPos.vec3i.x == 0 && m->blockPos.vec3i.y == 0 && m->blockPos.vec3i.z == 0);
    MCObjectHeap_free(h);
}
static void watcher_equal_and_manual_wire(void) {
    MCObjectHeap *h = heap(); Owner *o = owner_new(h);
    BlockPos *p = BlockPos_newInt(h, 7, 8, 9); CHECK(p);
    CHECK(DataWatcher_addObject(o->watcher, 6, (MCObject *)p));
    BlockPosMutableBlockPos *m = BlockPosMutableBlockPos_newInt(h, 7, 8, 9); CHECK(m);
    CHECK(DataWatcher_updateObject(o->watcher, 6, (MCObject *)m));
    CHECK(o->calls == 0 && !DataWatcher_hasObjectChanged(o->watcher));
    CHECK(WatchableObject_getObject(DataWatcher_nativeGetWatchedObject(o->watcher, 6)) == (MCObject *)p);
    change(m, -4, 200, INT32_MIN);
    CHECK(DataWatcher_updateObject(o->watcher, 6, (MCObject *)m));
    CHECK(o->calls == 1 && DataWatcher_hasObjectChanged(o->watcher));
    DataWatcher_func_111144_e(o->watcher);
    BlockPos *same = BlockPos_newInt(h, -4, 200, INT32_MIN); CHECK(same);
    CHECK(DataWatcher_updateObject(o->watcher, 6, (MCObject *)same) && o->calls == 1);
    CHECK(WatchableObject_getObject(DataWatcher_nativeGetWatchedObject(o->watcher, 6)) == (MCObject *)m);
    Vec3i *plain = Vec3i_newInt(h, -4, 200, INT32_MIN); CHECK(plain);
    CHECK(DataWatcher_updateObject(o->watcher, 6, (MCObject *)plain) && o->calls == 1);
    CHECK(WatchableObject_getObject(DataWatcher_nativeGetWatchedObject(o->watcher, 6)) == (MCObject *)m);
    /* Same reference uses ObjectUtils' identity short circuit after mutation. */
    change(m, INT32_MAX, -1, 17);
    CHECK(DataWatcher_updateObject(o->watcher, 6, (MCObject *)m) && o->calls == 1);
    CHECK(!DataWatcher_hasObjectChanged(o->watcher));
    CHECK(DataWatcher_addObjectByDataType(o->watcher, 6, 6));
    CHECK(DataWatcher_updateObject(o->watcher, 6, (MCObject *)m) && o->calls == 2);
    mc_buf b = {0}; PacketBuffer io; CHECK(PacketBuffer_init(&io, h, &b));
    CHECK(DataWatcher_writeTo(o->watcher, &io) && b.len == 14);
    CHECK(mc_get_u8(&b) == 0xc6 && mc_get_i32(&b) == INT32_MAX &&
          mc_get_i32(&b) == -1 && mc_get_i32(&b) == 17 && mc_get_u8(&b) == 127);
    b.pos = 0;
    WatchableObjectList *list = NULL;
    CHECK(DataWatcher_readWatchedListFromPacketBuffer(&io, &list) && list);
    MCObject *decoded = WatchableObject_getObject(WatchableObjectList_get(list, 0));
    CHECK(BlockPos_isRuntimeClass(decoded) && !BlockPosMutableBlockPos_isInstance(decoded));
    coords((BlockPos *)decoded, INT32_MAX, -1, 17);
    mc_buf_free(&b); MCObjectHeap_free(h);
    h = heap(); o = owner_new(h); m = BlockPosMutableBlockPos_newInt(h, 1, 2, 3); CHECK(m);
    CHECK(!DataWatcher_addObject(o->watcher, 6, (MCObject *)m));
    CHECK(MCObjectHeap_failed(h) && !DataWatcher_nativeGetWatchedObject(o->watcher, 6));
    MCObjectHeap_free(h);
}
static void packet_alias_wire(void) {
    MCObjectHeap *h = heap();
    BlockPosMutableBlockPos *m = BlockPosMutableBlockPos_newInt(h, 1, 2, 3); CHECK(m);
    C08PacketPlayerBlockPlacement *p = C08PacketPlayerBlockPlacement_new(h,
        (DataWatcherBlockPos *)m, 4, NULL, .25f, .5f, .75f);
    CHECK(p && p->position == (DataWatcherBlockPos *)m);
    C07PacketPlayerDigging *d = C07PacketPlayerDigging_new(h,
        C07PacketPlayerDigging_action(3), (DataWatcherBlockPos *)m, C07PacketPlayerDigging_facing(1));
    CHECK(d && d->position == p->position);
    change(m, -33554432, -2048, 33554431);
    mc_buf b = {0}; PacketBuffer io; CHECK(PacketBuffer_init(&io, h, &b));
    CHECK(C08PacketPlayerBlockPlacement_writePacketData(p, &io) && b.len == 14);
    int32_t x, y, z; mc_get_position(&b, &x, &y, &z);
    CHECK(x == -33554432 && y == -2048 && z == 33554431);
    CHECK(mc_get_u8(&b) == 4 && mc_get_i16(&b) == -1 && mc_get_u8(&b) == 4 &&
          mc_get_u8(&b) == 8 && mc_get_u8(&b) == 12);
    mc_buf_clear(&b);
    change(m, INT32_MIN, INT32_MAX, -1);
    CHECK(C07PacketPlayerDigging_writePacketData(d, &io) && b.len == 10);
    CHECK(mc_get_varint(&b) == 3); mc_get_position(&b, &x, &y, &z);
    CHECK(x == 0 && y == -1 && z == -1 && mc_get_u8(&b) == 1);
    b.pos = 0;
    C07PacketPlayerDigging *read = C07PacketPlayerDigging_new_empty(h); CHECK(read);
    CHECK(C07PacketPlayerDigging_readPacketData(read, &io));
    CHECK(BlockPos_isRuntimeClass((MCObject *)read->position) && read->position != d->position);
    coords((BlockPos *)read->position, 0, -1, -1);
    mc_buf_free(&b); MCObjectHeap_free(h);
}
static void lifetime_aliases(void) {
    MCObjectHeap *h = heap(); Owner *o = owner_new(h);
    o->position = BlockPosMutableBlockPos_newInt(h, 20, 30, 40); CHECK(o->position);
    CHECK(DataWatcher_addObjectByDataType(o->watcher, 6, 6));
    CHECK(DataWatcher_updateObject(o->watcher, 6, (MCObject *)o->position));
    o->placement = C08PacketPlayerBlockPlacement_new(h, (DataWatcherBlockPos *)o->position,
        2, NULL, 0, 0, 0); CHECK(o->placement);
    o->digging = C07PacketPlayerDigging_new(h, C07PacketPlayerDigging_action(4),
        (DataWatcherBlockPos *)o->position, C07PacketPlayerDigging_facing(0)); CHECK(o->digging);
    NativeJavaClass *component = NativeJavaClass_literal(h, &BlockPos_Class); CHECK(component);
    CHECK(NativeTypedObjectArray_new(h, component, 1, &o->positions) == NATIVE_ARRAY_OK);
    CHECK(NativeTypedObjectArray_set(o->positions, 0, (MCObject *)o->position) == NATIVE_ARRAY_OK);
    MCObjectRoot root = {0}; CHECK(MCObjectRoot_init(&root, h, (MCObject *)o));
    CHECK(MCObjectHeap_collect(h));
    MCObjectHeap *working = MCObjectHeap_clone(h); CHECK(working);
    MCObjectRoot other = {0}; CHECK(MCObjectRoot_rebind(&other, working, &root));
    Owner *copy = (Owner *)MCObjectRoot_get(&other); CHECK(copy && copy != o);
    CHECK(copy->position != o->position && copy->placement->position == (DataWatcherBlockPos *)copy->position);
    CHECK(copy->digging->position == copy->placement->position);
    CHECK(WatchableObject_getObject(DataWatcher_nativeGetWatchedObject(copy->watcher, 6)) == (MCObject *)copy->position);
    MCObject *arrayValue = NULL;
    CHECK(NativeTypedObjectArray_get(copy->positions, 0, &arrayValue) == NATIVE_ARRAY_OK &&
          arrayValue == (MCObject *)copy->position);
    change(copy->position, -20, -30, -40);
    /* Do not open a mutable borrow on the parent after cloning: the heap's
       adoption fence intentionally treats that scope as a changed generation. */
    CHECK(o->position->x == 20 && o->position->y == 30 && o->position->z == 40);
    CHECK(MCObjectHeap_adopt(h, working)); MCObjectHeap_free(working);
    o = (Owner *)MCObjectRoot_get(&root); coords((BlockPos *)o->position, -20, -30, -40);
    CHECK(MCObjectHeap_collect(h));
    mc_buf b = {0}; PacketBuffer io; CHECK(PacketBuffer_init(&io, h, &b));
    CHECK(C08PacketPlayerBlockPlacement_writePacketData(o->placement, &io));
    int x, y, z; mc_get_position(&b, &x, &y, &z); CHECK(x == -20 && y == -30 && z == -40);
    mc_buf_free(&b); MCObjectRoot_drop(&root); MCObjectHeap_free(h);
}
static void malformed_position_prefix(void) {
    for (unsigned which = 0; which < 4; ++which) {
        MCObjectHeap *h = heap(); Owner *o = owner_new(h);
        BlockPos *valid = BlockPos_newInt(h, 1, 2, 3); CHECK(valid);
        MCObject *bad = which == 0 ? MCObjectHeap_alloc(h, sizeof(MCObject), BlockPos_nativeClass()) :
                        which == 1 ? MCObjectHeap_alloc(h, sizeof(MCObject), BlockPosMutableBlockPos_nativeClass()) :
                        which == 2 ? (MCObject *)Vec3i_newInt(h, 1, 2, 3) : NULL;
        CHECK(which == 3 || bad);
        CHECK(DataWatcher_addObjectByDataType(o->watcher, 6, 6));
        CHECK(WatchableObject_setObject(DataWatcher_nativeGetWatchedObject(o->watcher, 6), bad));
        mc_buf b = {0}; PacketBuffer io; CHECK(PacketBuffer_init(&io, h, &b));
        CHECK(!DataWatcher_writeTo(o->watcher, &io));
        CHECK(b.failed && b.len == 1 && b.data[0] == 0xc6 && !MCObjectHeap_hasBorrowers(h));
        mc_buf_free(&b); MCObjectHeap_free(h);
        h = heap(); valid = BlockPos_newInt(h, 1, 2, 3); CHECK(valid);
        C07PacketPlayerDigging *d = C07PacketPlayerDigging_new(h, C07PacketPlayerDigging_action(4),
            (DataWatcherBlockPos *)valid, C07PacketPlayerDigging_facing(0)); CHECK(d);
        bad = which == 0 ? MCObjectHeap_alloc(h, sizeof(MCObject), BlockPos_nativeClass()) :
              which == 1 ? MCObjectHeap_alloc(h, sizeof(MCObject), BlockPosMutableBlockPos_nativeClass()) :
              which == 2 ? (MCObject *)Vec3i_newInt(h, 1, 2, 3) : NULL;
        CHECK(which == 3 || bad); d->position = (DataWatcherBlockPos *)bad;
        CHECK(PacketBuffer_init(&io, h, &b));
        CHECK(!C07PacketPlayerDigging_writePacketData(d, &io));
        CHECK(b.failed && b.len == 1 && b.data[0] == 4 && !MCObjectHeap_hasBorrowers(h));
        mc_buf_free(&b); MCObjectHeap_free(h);
        h = heap(); valid = BlockPos_newInt(h, 1, 2, 3); CHECK(valid);
        C08PacketPlayerBlockPlacement *p = C08PacketPlayerBlockPlacement_new(h,
            (DataWatcherBlockPos *)valid, 3, NULL, 0, 0, 0); CHECK(p);
        bad = which == 0 ? MCObjectHeap_alloc(h, sizeof(MCObject), BlockPos_nativeClass()) :
              which == 1 ? MCObjectHeap_alloc(h, sizeof(MCObject), BlockPosMutableBlockPos_nativeClass()) :
              which == 2 ? (MCObject *)Vec3i_newInt(h, 1, 2, 3) : NULL;
        CHECK(which == 3 || bad); p->position = (DataWatcherBlockPos *)bad;
        CHECK(PacketBuffer_init(&io, h, &b)); mc_put_u8(&b, 0x55);
        CHECK(!C08PacketPlayerBlockPlacement_writePacketData(p, &io));
        CHECK(b.failed && b.len == 1 && b.data[0] == 0x55 && !MCObjectHeap_hasBorrowers(h));
        mc_buf_free(&b); MCObjectHeap_free(h);
    }
}
static void reads_and_guards(void) {
    /* Source type6 captures all three readInt locals before constructing the
       BlockPos. Failed decode retains output and the completed buffer prefix. */
    static const uint8_t bytes[] = {0xc6, 0xff, 0xff, 0xff, 0xff,
        0x80, 0, 0, 0, 0x7f, 0xff, 0xff, 0xff, 127};
    for (size_t length = 1; length < sizeof(bytes); ++length) {
        MCObjectHeap *h = heap();
        WatchableObjectList *sentinel = WatchableObjectList_new(h); CHECK(sentinel);
        WatchableObjectList *out = sentinel;
        size_t before = MCObjectHeap_liveObjects(h);
        mc_buf b = {(uint8_t *)bytes, length, length, 0, false}; PacketBuffer io;
        CHECK(PacketBuffer_init(&io, h, &b));
        CHECK(!DataWatcher_readWatchedListFromPacketBuffer(&io, &out));
        CHECK(out == sentinel && b.failed && b.pos <= length);
        CHECK(!MCObjectHeap_failed(h) && !MCObjectHeap_hasBorrowers(h));
        /* One temporary list, without a coordinate object on incomplete ints. */
        if (length < 13) CHECK(MCObjectHeap_liveObjects(h) == before + 1);
        MCObjectHeap_free(h);
    }
    for (unsigned which = 0; which < 3; ++which) {
        MCObjectHeap *h = heap();
        const MCObjectClass *klass = which == 0 ? Vec3i_nativeClass() :
            which == 1 ? BlockPos_nativeClass() : BlockPosMutableBlockPos_nativeClass();
        MCObject *tiny = MCObjectHeap_alloc(h, sizeof(MCObject), klass); CHECK(tiny);
        CHECK(!NativeJavaClass_getClass(h, tiny) && MCObjectHeap_failed(h));
        MCObjectHeap_free(h);
    }
    MCObjectHeap *h = heap();
    MCObject fake = {h, BlockPos_nativeClass()};
    CHECK(!NativeJavaClass_getClass(h, &fake) && MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    for (unsigned packetKind = 0; packetKind < 2; ++packetKind) {
        h = heap(); MCObjectHeap *foreign = heap();
        BlockPosMutableBlockPos *m = BlockPosMutableBlockPos_newInt(foreign, 1, 2, 3); CHECK(m);
        if (!packetKind)
            CHECK(!C08PacketPlayerBlockPlacement_new(h, (DataWatcherBlockPos *)m, 1, NULL, 0, 0, 0));
        else
            CHECK(!C07PacketPlayerDigging_new(h, C07PacketPlayerDigging_action(3),
                (DataWatcherBlockPos *)m, C07PacketPlayerDigging_facing(0)));
        CHECK(MCObjectHeap_failed(h) && !MCObjectHeap_failed(foreign));
        CHECK(!MCObjectHeap_hasBorrowers(h) && !MCObjectHeap_hasBorrowers(foreign));
        MCObjectHeap_free(h); MCObjectHeap_free(foreign);
    }
    h = heap();
    fake = (MCObject){h, BlockPos_nativeClass()};
    C08PacketPlayerBlockPlacement *p = C08PacketPlayerBlockPlacement_new_empty(h); CHECK(p);
    p->position = (DataWatcherBlockPos *)&fake;
    mc_buf b = {0}; PacketBuffer io; CHECK(PacketBuffer_init(&io, h, &b));
    CHECK(!C08PacketPlayerBlockPlacement_writePacketData(p, &io) && b.len == 0 && b.failed);
    CHECK(MCObjectHeap_failed(h) && !MCObjectHeap_hasBorrowers(h));
    mc_buf_free(&b); MCObjectHeap_free(h);
}
static void current_watched_edge_guard(void) {
    /* A native graph corruption fixture writes the actual private field's
       object representation. The ordinary setter rejects foreign references;
       serialization must also check the current field after its header. */
    typedef struct {
        MCObject object;
        int32_t objectType, dataValueId;
        MCObject *watchedObject;
        bool watched;
    } WatchableLayout;
    for (unsigned which = 0; which < 2; ++which) {
        MCObjectHeap *h = heap(), *foreign = heap(); Owner *o = owner_new(h);
        BlockPos *p = BlockPos_newInt(h, 1, 2, 3); CHECK(p);
        CHECK(DataWatcher_addObject(o->watcher, 6, (MCObject *)p));
        BlockPosMutableBlockPos *other = BlockPosMutableBlockPos_newInt(foreign, 7, 8, 9);
        CHECK(other);
        MCObject fake = {h, BlockPosMutableBlockPos_nativeClass()};
        MCObject *value = which ? &fake : (MCObject *)other;
        WatchableObject *watch = DataWatcher_nativeGetWatchedObject(o->watcher, 6); CHECK(watch);
        memcpy((uint8_t *)watch + offsetof(WatchableLayout, watchedObject), &value, sizeof(value));
        mc_buf b = {0}; PacketBuffer io; CHECK(PacketBuffer_init(&io, h, &b));
        CHECK(!DataWatcher_writeTo(o->watcher, &io));
        CHECK(b.failed && b.len == 1 && b.data[0] == 0xc6);
        CHECK(MCObjectHeap_failed(h) && !MCObjectHeap_failed(foreign));
        CHECK(!MCObjectHeap_hasBorrowers(h) && !MCObjectHeap_hasBorrowers(foreign));
        mc_buf_free(&b); MCObjectHeap_free(h); MCObjectHeap_free(foreign);
    }
}
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "class")) class_and_arrays();
    else if (argc == 2 && !strcmp(argv[1], "watcher")) watcher_equal_and_manual_wire();
    else if (argc == 2 && !strcmp(argv[1], "packet")) packet_alias_wire();
    else if (argc == 2 && !strcmp(argv[1], "watcher-edge")) current_watched_edge_guard();
    else {
        class_and_arrays(); watcher_equal_and_manual_wire(); packet_alias_wire();
        lifetime_aliases(); malformed_position_prefix(); reads_and_guards(); current_watched_edge_guard();
    }
    printf("source position consumers: %u checks\n", checks); return 0;
}
