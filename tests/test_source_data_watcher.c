#include "entity/DataWatcher.h"
#include "nbt/NBTTagCompound.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "watcher check %u line %d: %s\n", checks, __LINE__, #x);               \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
typedef struct {
    MCObject object;
    DataWatcher *watcher;
    unsigned calls;
    int32_t last;
    bool fail, inspect;
    WatchableObjectList *edit;
} Owner;
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
    Owner *p = (Owner *)o;
    p->watcher = (DataWatcher *)v((MCObject *)p->watcher, c);
    p->edit = (WatchableObjectList *)v((MCObject *)p->edit, c);
}
static const MCObjectClass ownerClass = {"test.DataWatcher.owner", MCObjectHeap_plainClone, trace,
                                         NULL};
static bool notified(MCObject *context, MCObject *o, int32_t id) {
    Owner *p = (Owner *)o;
    CHECK(context == o && MCObjectHeap_hasBorrowers(o->heap));
    if (p->inspect) {
        CHECK(id == 0 && DataWatcher_getWatchableObjectByte(p->watcher, id) == 1);
        CHECK(!DataWatcher_hasObjectChanged(p->watcher) &&
              !WatchableObject_isWatched(DataWatcher_nativeGetWatchedObject(p->watcher, id)));
    }
    p->calls++;
    p->last = id;
    MCObjectHeap_touch(o->heap);
    if (p->edit)
        WatchableObjectList_add(p->edit, NULL);
    return !p->fail;
}
static const DataWatcherDependencies deps = {notified, NULL};
static Owner *owner(MCObjectHeap *h) {
    Owner *o = (Owner *)MCObjectHeap_alloc(h, sizeof(*o), &ownerClass);
    CHECK(o);
    o->watcher = DataWatcher_new(h, (MCObject *)o, &deps, (MCObject *)o);
    CHECK(o->watcher);
    return o;
}
static void state_and_alias(void) {
    MCObjectHeap *h = MCObjectHeap_new(8 * 1024 * 1024);
    CHECK(h);
    MCObjectRootScope scope = {0};
    CHECK(MCObjectRootScope_begin(&scope, h));
    Owner *o = owner(h);
    DataWatcher *w = o->watcher;
    CHECK(DataWatcher_getIsBlank(w) && !DataWatcher_hasObjectChanged(w) &&
          !DataWatcher_getAllWatched(w) && !DataWatcher_getChanged(w));
    ItemStack *s = ItemStack_new(h, ItemStack_registryItem(387), 0, 7);
    CHECK(s && DataWatcher_addObject(w, 10, (MCObject *)s));
    CHECK(!DataWatcher_getIsBlank(w) && !DataWatcher_hasObjectChanged(w));
    WatchableObject *watch = DataWatcher_nativeGetWatchedObject(w, 10);
    CHECK(watch && WatchableObject_isWatched(watch) &&
          WatchableObject_getObject(watch) == (MCObject *)s);
    WatchableObjectList *all = DataWatcher_getAllWatched(w);
    CHECK(all && WatchableObjectList_get(all, 0) == watch &&
          DataWatcher_getWatchableObjectItemStack(w, 10) == s && !DataWatcher_getChanged(w));
    CHECK(DataWatcher_updateObject(w, 10, (MCObject *)s) && o->calls == 0 &&
          !DataWatcher_hasObjectChanged(w));
    ItemStack *copy = ItemStack_copy(h, s);
    CHECK(copy && DataWatcher_updateObject(w, 10, (MCObject *)copy) && o->calls == 1 &&
          o->last == 10);
    CHECK(WatchableObjectList_get(all, 0) == watch &&
          WatchableObject_getObject(watch) == (MCObject *)copy);
    WatchableObjectList *changed = DataWatcher_getChanged(w);
    CHECK(changed && WatchableObjectList_get(changed, 0) == watch &&
          !WatchableObject_isWatched(watch) && !DataWatcher_hasObjectChanged(w));
    CHECK(DataWatcher_setObjectWatched(w, 10) && DataWatcher_hasObjectChanged(w));
    DataWatcher_func_111144_e(w);
    CHECK(!DataWatcher_hasObjectChanged(w) && WatchableObject_isWatched(watch) &&
          !DataWatcher_getChanged(w));
    WatchableObjectList *input = WatchableObjectList_new(h);
    WatchableObject *incoming = WatchableObject_new(h, 2, 10, (MCObject *)s);
    CHECK(input && incoming && WatchableObjectList_add(input, incoming));
    CHECK(DataWatcher_updateWatchedObjectsFromList(w, input) && o->calls == 2 &&
          DataWatcher_getWatchableObjectItemStack(w, 10) == s &&
          WatchableObject_getObjectType(watch) == 5 && DataWatcher_hasObjectChanged(w));
    CHECK(DataWatcher_getChanged(w) && !WatchableObject_isWatched(watch));
    CHECK(DataWatcher_updateWatchedObjectsFromList(w, input) && o->calls == 3 &&
          DataWatcher_hasObjectChanged(w) && !DataWatcher_getChanged(w));
    CHECK(DataWatcher_addObjectByDataType(w, 10, 99));
    CHECK(DataWatcher_nativeGetWatchedObject(w, 10) != watch &&
          WatchableObject_getObjectType(DataWatcher_nativeGetWatchedObject(w, 10)) == 99 &&
          WatchableObject_getObject(watch) == (MCObject *)s);
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, h, (MCObject *)o));
    MCObjectRootScope_end(&scope);
    MCObjectHeap *branch = MCObjectHeap_clone(h);
    CHECK(branch);
    MCObjectRoot clone = {0};
    CHECK(MCObjectRoot_rebind(&clone, branch, &root));
    Owner *co = (Owner *)MCObjectRoot_get(&clone);
    CHECK(co != o && co->watcher != w &&
          DataWatcher_nativeGetWatchedObject(co->watcher, 10) !=
              DataWatcher_nativeGetWatchedObject(w, 10));
    MCObjectHeap_free(branch);
    MCObjectRoot_drop(&root);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void values(void) {
    MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
    CHECK(h);
    MCObjectRootScope scope = {0};
    CHECK(MCObjectRootScope_begin(&scope, h));
    Owner *o = owner(h);
    DataWatcher *w = o->watcher;
    CHECK(DataWatcher_addObject(w, 0, DataWatcher_boxByte(h, 255)));
    CHECK(DataWatcher_addObject(w, 1, DataWatcher_boxShort(h, 65535)));
    CHECK(DataWatcher_addObject(w, 2, DataWatcher_boxInt(h, INT32_MIN)));
    CHECK(DataWatcher_addObject(w, 3, DataWatcher_boxFloat(h, -0.0f)));
    CHECK(DataWatcher_addObject(w, 4, (MCObject *)NBTString_fromUTF8(h, "日本語")));
    CHECK(DataWatcher_addObject(w, 6, (MCObject *)DataWatcher_blockPos(h, 1, 2, 3)));
    CHECK(DataWatcher_addObject(w, 7, (MCObject *)DataWatcher_rotations(h, -0.0f, NAN, INFINITY)));
    CHECK(DataWatcher_getWatchableObjectByte(w, 0) == -1 &&
          DataWatcher_getWatchableObjectShort(w, 1) == -1 &&
          DataWatcher_getWatchableObjectInt(w, 2) == INT32_MIN &&
          signbit(DataWatcher_getWatchableObjectFloat(w, 3)));
    CHECK(DataWatcher_updateObject(w, 0, DataWatcher_boxByte(h, -1)) && o->calls == 0);
    CHECK(DataWatcher_updateObject(w, 3, DataWatcher_boxFloat(h, 0.0f)) && o->calls == 1);
    CHECK(DataWatcher_updateObject(w, 3, DataWatcher_boxFloat(h, NAN)) && o->calls == 2);
    CHECK(DataWatcher_updateObject(w, 3, DataWatcher_boxFloat(h, NAN)) && o->calls == 2);
    CHECK(DataWatcher_updateObject(w, 4, (MCObject *)NBTString_fromUTF8(h, "日本語")) &&
          o->calls == 2);
    CHECK(DataWatcher_updateObject(w, 6, (MCObject *)DataWatcher_blockPos(h, 1, 2, 3)) &&
          o->calls == 2);
    CHECK(
        DataWatcher_updateObject(w, 7, (MCObject *)DataWatcher_rotations(h, 0.0f, NAN, INFINITY)) &&
        o->calls == 3);
    CHECK(DataWatcher_getWatchableObjectRotations(w, 7)->x == 0.0f);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectRootScope_end(&scope);
    MCObjectHeap_free(h);
}
static void wire(void) {
    MCObjectHeap *h = MCObjectHeap_new(8 * 1024 * 1024);
    CHECK(h);
    MCObjectRootScope scope = {0};
    CHECK(MCObjectRootScope_begin(&scope, h));
    Owner *o = owner(h);
    DataWatcher *w = o->watcher;
    CHECK(DataWatcher_addObject(w, 0, DataWatcher_boxByte(h, -128)));
    CHECK(DataWatcher_addObject(w, 1, DataWatcher_boxShort(h, -32768)));
    CHECK(DataWatcher_addObject(w, 2, DataWatcher_boxInt(h, INT32_MIN)));
    CHECK(DataWatcher_addObject(w, 3, DataWatcher_boxFloat(h, -0.0f)));
    uint16_t units[] = {0, 0xd800, 0x3042};
    CHECK(DataWatcher_addObject(w, 4, (MCObject *)NBTString_fromUTF16(h, units, 3)));
    ItemStack *s = ItemStack_new(h, ItemStack_registryItem(1), 0, -1);
    CHECK(s && DataWatcher_addObject(w, 5, (MCObject *)s));
    CHECK(
        DataWatcher_addObject(w, 6, (MCObject *)DataWatcher_blockPos(h, -1, INT32_MIN, INT32_MAX)));
    CHECK(DataWatcher_addObject(w, 7, (MCObject *)DataWatcher_rotations(h, 1.0f, -0.0f, INFINITY)));
    mc_buf b;
    mc_buf_init(&b);
    PacketBuffer p;
    CHECK(PacketBuffer_init(&p, h, &b) && DataWatcher_writeTo(w, &p));
    static const uint8_t golden[] = {
        0x00, 0x80, 0x21, 0x80, 0x00, 0x42, 0x80, 0,    0,    0,    0x63, 0x80, 0,    0,
        0,    0x84, 5,    0,    0x3f, 0xe3, 0x81, 0x82, 0xa5, 0,    1,    0,    0,    0,
        0,    0xc6, 0xff, 0xff, 0xff, 0xff, 0x80, 0,    0,    0,    0x7f, 0xff, 0xff, 0xff,
        0xe7, 0x3f, 0x80, 0,    0,    0x80, 0,    0,    0,    0x7f, 0x80, 0,    0,    0x7f};
    CHECK(b.len == sizeof(golden) && memcmp(b.data, golden, sizeof(golden)) == 0);
    WatchableObjectList *decoded = NULL;
    CHECK(DataWatcher_readWatchedListFromPacketBuffer(&p, &decoded) &&
          WatchableObjectList_size(decoded) == 8 && b.pos == b.len);
    ItemStack *read = (ItemStack *)WatchableObject_getObject(WatchableObjectList_get(decoded, 5));
    CHECK(read && read != s && read->stackSize == 0 && read->itemDamage == 0);
    NBTString *str = (NBTString *)WatchableObject_getObject(WatchableObjectList_get(decoded, 4));
    CHECK(NBTString_length(str) == 3 && NBTString_units(str)[0] == 0 &&
          NBTString_units(str)[1] == '?' && NBTString_units(str)[2] == 0x3042);
    mc_buf_clear(&b);
    CHECK(DataWatcher_writeWatchedListToPacketBuffer(NULL, &p) && b.len == 1 && b.data[0] == 127);
    WatchableObjectList *old = decoded;
    CHECK(DataWatcher_readWatchedListFromPacketBuffer(&p, &decoded) && !decoded);
    mc_buf_clear(&b);
    CHECK(DataWatcher_addObject(w, 31, DataWatcher_boxFloat(h, 1.0f)));
    CHECK(DataWatcher_writeTo(w, &p));
    decoded = old;
    CHECK(DataWatcher_readWatchedListFromPacketBuffer(&p, &decoded) && b.pos + 4 + 1 == b.len);
    CHECK(!MCObjectHeap_failed(h));
    mc_buf_free(&b);
    MCObjectRootScope_end(&scope);
    MCObjectHeap_free(h);
}
static void failure_order(void) {
    MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
    CHECK(h);
    MCObjectRootScope scope = {0};
    CHECK(MCObjectRootScope_begin(&scope, h));
    Owner *o = owner(h);
    DataWatcher *w = o->watcher;
    CHECK(DataWatcher_addObject(w, 0, DataWatcher_boxByte(h, 0)) &&
          DataWatcher_setObjectWatched(w, 0) && DataWatcher_getChanged(w));
    WatchableObject *watch = DataWatcher_nativeGetWatchedObject(w, 0);
    o->fail = true;
    o->inspect = true;
    MCObject *next = DataWatcher_boxByte(h, 1);
    CHECK(!DataWatcher_updateObject(w, 0, next) && MCObjectHeap_failed(h));
    CHECK(WatchableObject_getObject(watch) == next && !WatchableObject_isWatched(watch) &&
          !DataWatcher_hasObjectChanged(w) && o->calls == 1);
    MCObjectRootScope_end(&scope);
    MCObjectHeap_free(h);
    for (int scenario = 0; scenario < 5; scenario++) {
        h = MCObjectHeap_new(1024 * 1024);
        CHECK(h);
        o = owner(h);
        w = o->watcher;
        CHECK(DataWatcher_addObject(w, -1, DataWatcher_boxInt(h, 7)));
        if (scenario == 0)
            CHECK(!DataWatcher_addObject(w, 32, DataWatcher_boxInt(h, 0)));
        if (scenario == 1)
            CHECK(!DataWatcher_addObject(w, -1, DataWatcher_boxInt(h, 0)));
        if (scenario == 2)
            CHECK(!DataWatcher_addObject(w, 1, NULL));
        if (scenario == 3) {
            DataWatcher_getWatchableObjectByte(w, -1);
            CHECK(MCObjectHeap_failed(h));
        }
        if (scenario == 4) {
            DataWatcher_getWatchableObjectInt(w, 999);
            CHECK(MCObjectHeap_failed(h));
        }
        CHECK(MCObjectHeap_failed(h));
        MCObjectHeap_free(h);
    }
}
static void collection_failure(void) {
    MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
    CHECK(h);
    Owner *o = owner(h);
    DataWatcher *w = o->watcher;
    CHECK(DataWatcher_addObject(w, 0, DataWatcher_boxByte(h, 0)) &&
          DataWatcher_setObjectWatched(w, 0) && DataWatcher_getChanged(w));
    WatchableObjectList *input = WatchableObjectList_new(h);
    CHECK(input &&
          WatchableObjectList_add(input, WatchableObject_new(h, 0, 0, DataWatcher_boxByte(h, 1))));
    o->edit = input;
    o->inspect = true;
    CHECK(!DataWatcher_updateWatchedObjectsFromList(w, input) && MCObjectHeap_failed(h));
    CHECK(o->calls == 1 && DataWatcher_getWatchableObjectByte(w, 0) == 1 &&
          !DataWatcher_hasObjectChanged(w));
    MCObjectHeap_free(h);
}
static void hash_order(const char *path) {
    FILE *in = fopen(path, "r");
    CHECK(in);
    int cases;
    CHECK(fscanf(in, "%d", &cases) == 1);
    for (int c = 0; c < cases; c++) {
        MCObjectHeap *h = MCObjectHeap_new(8 * 1024 * 1024);
        CHECK(h);
        Owner *o = owner(h);
        int count;
        CHECK(fscanf(in, "%d", &count) == 1);
        for (int i = 0; i < count; i++) {
            int id;
            CHECK(fscanf(in, "%d", &id) == 1 && DataWatcher_addObjectByDataType(o->watcher, id, 5));
        }
        WatchableObjectList *all = DataWatcher_getAllWatched(o->watcher);
        int unique;
        CHECK(fscanf(in, "%d", &unique) == 1 && WatchableObjectList_size(all) == unique);
        for (int i = 0; i < unique; i++) {
            int id;
            CHECK(fscanf(in, "%d", &id) == 1 &&
                  WatchableObject_getDataValueId(WatchableObjectList_get(all, i)) == id);
        }
        CHECK(!MCObjectHeap_failed(h));
        MCObjectHeap_free(h);
    }
    CHECK(fclose(in) == 0);
}
static void collection_goldens(
    void) { /* Numeric JDK8 Integer-key tree-bin iteration fact, independently probed. */
    static const int32_t inserted[] = {458759,  917518, 262148,  0,      1048592, 589833, 65537,
                                       1114129, 851981, 1245203, 196611, 655370,  786444, 1310740,
                                       1179666, 131074, 720907,  327685, 524296,  983055, 393222};
    static const int32_t expected[] = {458759, 917518, 262148,  327685,  393222,  0,      1048592,
                                       983055, 589833, 524296,  65537,   1114129, 851981, 786444,
                                       720907, 655370, 1245203, 1179666, 1310740, 196611, 131074};
    MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
    CHECK(h);
    Owner *o = owner(h);
    for (size_t i = 0; i < sizeof(inserted) / sizeof(*inserted); i++)
        CHECK(DataWatcher_addObjectByDataType(o->watcher, inserted[i], 5));
    WatchableObjectList *list = DataWatcher_getAllWatched(o->watcher);
    CHECK(WatchableObjectList_size(list) == 21);
    for (int i = 0; i < 21; i++)
        CHECK(WatchableObject_getDataValueId(WatchableObjectList_get(list, i)) == expected[i]);
    Owner *regular = owner(h);
    for (int i = 31; i >= 0; i--)
        CHECK(DataWatcher_addObjectByDataType(regular->watcher, i, 5));
    list = DataWatcher_getAllWatched(regular->watcher);
    CHECK(WatchableObjectList_size(list) == 32);
    for (int i = 0; i < 32; i++)
        CHECK(WatchableObject_getDataValueId(WatchableObjectList_get(list, i)) == i);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void charset_goldens(void) {
    static const struct {
        uint8_t bytes[4];
        unsigned count;
        uint16_t units[4];
        unsigned length;
    } cases[] = {{{0}, 1, {0}, 1},
                 {{0xc0, 0x80}, 2, {0xfffd, 0xfffd}, 2},
                 {{0xe0, 0x80, 0x80}, 3, {0xfffd, 0xfffd, 0xfffd}, 3},
                 {{0xed, 0xa0, 0x80}, 3, {0xfffd}, 1},
                 {{0xe2, 0x82}, 2, {0xfffd}, 1},
                 {{0xe2, 0x28, 0xa1}, 3, {0xfffd, '(', 0xfffd}, 3},
                 {{0xf0, 0x90, 0x80, 0x80}, 4, {0xd800, 0xdc00}, 2},
                 {{0xf4, 0x8f, 0xbf, 0xbf}, 4, {0xdbff, 0xdfff}, 2},
                 {{0xf4, 0x90, 0x80, 0x80}, 4, {0xfffd, 0xfffd, 0xfffd, 0xfffd}, 4},
                 {{0xff, 0xc2, 'A'}, 3, {0xfffd, 0xfffd, 'A'}, 3}};
    MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
    CHECK(h);
    mc_buf b;
    mc_buf_init(&b);
    PacketBuffer p;
    CHECK(PacketBuffer_init(&p, h, &b));
    for (size_t c = 0; c < sizeof(cases) / sizeof(*cases); c++) {
        mc_buf_clear(&b);
        mc_put_varint(&b, (int32_t)cases[c].count);
        mc_put_bytes(&b, cases[c].bytes, cases[c].count);
        NBTString *s = NULL;
        CHECK(PacketBuffer_readStringFromBuffer(&p, 32767, &s) &&
              NBTString_length(s) == cases[c].length);
        CHECK(memcmp(NBTString_units(s), cases[c].units, cases[c].length * sizeof(uint16_t)) == 0 &&
              b.pos == b.len);
    }
    uint16_t original[] = {0xd800, 0xd800, 0xdc00, 0xdc00};
    NBTString *s = NBTString_fromUTF16(h, original, 4);
    CHECK(s);
    mc_buf_clear(&b);
    CHECK(PacketBuffer_writeString(&p, s));
    static const uint8_t encoded[] = {6, '?', 0xf0, 0x90, 0x80, 0x80, '?'};
    CHECK(b.len == sizeof(encoded) && memcmp(b.data, encoded, sizeof(encoded)) == 0);
    mc_buf_clear(&b);
    mc_put_varint(&b, 2);
    mc_put_u8(&b, 'a');
    mc_put_u8(&b, 'b');
    NBTString *saved = s;
    CHECK(!PacketBuffer_readStringFromBuffer(&p, 1, &saved) && saved == s && b.pos == b.len &&
          b.failed);
    CHECK(!MCObjectHeap_failed(h));
    mc_buf_free(&b);
    MCObjectHeap_free(h);
}
static void charset_vectors(const char *path) {
    FILE *in = fopen(path, "r");
    CHECK(in);
    MCObjectHeap *h = MCObjectHeap_new(32 * 1024 * 1024);
    CHECK(h);
    PacketBuffer p;
    mc_buf b;
    mc_buf_init(&b);
    CHECK(PacketBuffer_init(&p, h, &b));
    int cases;
    CHECK(fscanf(in, "%d", &cases) == 1);
    for (int c = 0; c < cases; c++) {
        int count, length;
        CHECK(fscanf(in, "%d", &count) == 1 && count >= 0 && count <= 8);
        mc_buf_clear(&b);
        mc_put_varint(&b, count);
        for (int i = 0; i < count; i++) {
            unsigned value;
            CHECK(fscanf(in, "%x", &value) == 1);
            mc_put_u8(&b, (uint8_t)value);
        }
        NBTString *text = NULL;
        CHECK(PacketBuffer_readStringFromBuffer(&p, 32767, &text));
        CHECK(fscanf(in, "%d", &length) == 1 && NBTString_length(text) == (size_t)length);
        for (int i = 0; i < length; i++) {
            unsigned value;
            CHECK(fscanf(in, "%x", &value) == 1 && NBTString_units(text)[i] == value);
        }
        CHECK(b.pos == b.len && !MCObjectHeap_failed(h));
    }
    CHECK(fclose(in) == 0);
    mc_buf_free(&b);
    MCObjectHeap_free(h);
}
static void varint_goldens(void) {
    static const struct {
        uint8_t bytes[7];
        size_t size, position;
        bool success;
        int32_t value;
    } cases[] = {{{0x80, 0x80, 0x80, 0x80, 0x10}, 5, 5, true, 0},
                 {{0x80, 0x80, 0x80, 0x80, 0x70}, 5, 5, true, 0},
                 {{0xff, 0xff, 0xff, 0xff, 0x7f}, 5, 5, true, -1},
                 {{0x80, 0x80, 0x80, 0x80, 0x1f}, 5, 5, true, -268435456},
                 {{0x80, 0x80, 0x80, 0x80, 0x80, 0}, 6, 6, false, 0},
                 {{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0}, 7, 6, false, 0}};
    MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
    CHECK(h);
    mc_buf b;
    mc_buf_init(&b);
    PacketBuffer p;
    CHECK(PacketBuffer_init(&p, h, &b));
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
        mc_buf_clear(&b);
        mc_put_bytes(&b, cases[i].bytes, cases[i].size);
        int32_t value = 123;
        bool ok = PacketBuffer_readVarIntFromBuffer(&p, &value);
        CHECK(ok == cases[i].success && b.pos == cases[i].position);
        CHECK(value == (ok ? cases[i].value : 123) && b.failed == !ok);
    }
    static const int32_t values[] = {0, -1, 1, 127, 128, INT32_MIN, INT32_MAX};
    for (size_t i = 0; i < sizeof(values) / sizeof(*values); i++) {
        mc_buf_clear(&b);
        CHECK(PacketBuffer_writeVarIntToBuffer(&p, values[i]));
        int32_t value = 0;
        CHECK(PacketBuffer_readVarIntFromBuffer(&p, &value) && value == values[i] &&
              b.pos == b.len);
    }
    mc_buf_clear(&b);
    mc_put_bytes(&b, cases[0].bytes, cases[0].size);
    NBTString *empty = NULL;
    CHECK(PacketBuffer_readStringFromBuffer(&p, 32767, &empty) && empty &&
          NBTString_length(empty) == 0 && b.pos == 5);
    CHECK(!MCObjectHeap_failed(h));
    mc_buf_free(&b);
    MCObjectHeap_free(h);
}
int main(int argc, char **argv) {
    state_and_alias();
    values();
    wire();
    failure_order();
    collection_failure();
    collection_goldens();
    charset_goldens();
    varint_goldens();
    if (argc > 1)
        hash_order(argv[1]);
    if (argc > 2)
        charset_vectors(argv[2]);
    printf("Source DataWatcher: %u checks passed\n", checks);
    return 0;
}
