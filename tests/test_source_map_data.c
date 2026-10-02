#include "nbt/NBTInternal.h"
#include "network/play/server/S34PacketMaps.h"
#include "util/MCGameplayPlayer.h"
#include "util/Vec4b.h"
#include "world/storage/MapData.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        checks++;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "check %u line %d: %s\n", checks, __LINE__, #x);                       \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
typedef struct Recorder {
    MCObject object;
    MapData *map;
    NativeReferenceList *replacement;
    int calls, failAt, mode;
    char events[512];
} Recorder;
static void trace_recorder(MCObject *o, MCObjectVisitor v, void *c) {
    Recorder *r = (Recorder *)o;
    r->map = (MapData *)v((MCObject *)r->map, c);
    r->replacement = (NativeReferenceList *)v((MCObject *)r->replacement, c);
}
static const MCObjectClass recorderClass = {"test.MapData.Recorder", MCObjectHeap_plainClone,
                                            trace_recorder, NULL};
static Recorder *recorder(MCObjectHeap *h) {
    Recorder *r = (Recorder *)MCObjectHeap_alloc(h, sizeof(*r), &recorderClass);
    CHECK(r);
    return r;
}
static MCObjectHeap *heap_new(void) {
    MCObjectHeap *h = MCObjectHeap_new(8u * 1024u * 1024u);
    CHECK(h);
    return h;
}
static MapData *map_new(MCObjectHeap *h) {
    MapData *m = MapData_new(h, NULL, NULL, NULL);
    CHECK(m);
    return m;
}
static NativeByteArray *array(MCObjectHeap *h, int length, int salt) {
    if (length < 0)
        return NULL;
    NativeByteArray *a = NativeByteArray_new(h, length);
    CHECK(a);
    for (int i = 0; i < length; i++)
        a->values[i] = nbt_i8((uint8_t)(i * 37 + salt));
    return a;
}
static NBTTagCompound *input(MCObjectHeap *h, int width, int height, int scale,
                             NativeByteArray *bytes) {
    NBTTagCompound *t = NBTTagCompound_new(h);
    CHECK(t);
    CHECK(NBTTagCompound_setByte_ascii(t, "dimension", -1));
    CHECK(NBTTagCompound_setInteger_ascii(t, "xCenter", -123456789));
    CHECK(NBTTagCompound_setInteger_ascii(t, "zCenter", 987654321));
    CHECK(NBTTagCompound_setByte_ascii(t, "scale", nbt_i8((uint8_t)scale)));
    CHECK(NBTTagCompound_setShort_ascii(t, "width", nbt_i16((uint16_t)width)));
    CHECK(NBTTagCompound_setShort_ascii(t, "height", nbt_i16((uint16_t)height)));
    CHECK(NBTTagCompound_setByteArray(t, NBTString_literalASCII(h, "colors"), bytes));
    return t;
}
static WorldSavedDataResult event(Recorder *r, const char *prefix, const char *key) {
    char item[80];
    snprintf(item, sizeof(item), "%s:%s;", prefix, key);
    CHECK(strlen(r->events) + strlen(item) < sizeof(r->events));
    strcat(r->events, item);
    r->calls++;
    return r->calls == r->failAt ? WORLD_SAVED_DATA_EXCEPTION : WORLD_SAVED_DATA_OK;
}
static WorldSavedDataResult get_number(MCObject *c, NBTTagCompound *t, const char *key, int kind,
                                       int32_t *out) {
    Recorder *r = (Recorder *)c;
    WorldSavedDataResult result = event(r,
                                        kind == 1   ? "getByte"
                                        : kind == 2 ? "getShort"
                                                    : "getInt",
                                        key);
    if (result != WORLD_SAVED_DATA_OK)
        return result;
    *out = kind == 1   ? NBTTagCompound_getByte_ascii(t, key)
           : kind == 2 ? NBTTagCompound_getShort_ascii(t, key)
                       : NBTTagCompound_getInteger_ascii(t, key);
    return WORLD_SAVED_DATA_OK;
}
static WorldSavedDataResult get_array(MCObject *c, NBTTagCompound *t, const char *key,
                                      NativeByteArray **out) {
    Recorder *r = (Recorder *)c;
    WorldSavedDataResult result = event(r, "getByteArray", key);
    if (result == WORLD_SAVED_DATA_OK)
        *out = NBTTagCompound_getByteArray(t, NBTString_literalASCII(c->heap, key));
    return result;
}
static WorldSavedDataResult set_number(MCObject *c, NBTTagCompound *t, const char *key, int kind,
                                       int32_t value) {
    Recorder *r = (Recorder *)c;
    WorldSavedDataResult result = event(r,
                                        kind == 1   ? "setByte"
                                        : kind == 2 ? "setShort"
                                                    : "setInt",
                                        key);
    if (result != WORLD_SAVED_DATA_OK)
        return result;
    if (r->mode == 1 && r->calls == 1) {
        r->map->xCenter = 4455;
        r->map->scale = -128;
    }
    bool ok = kind == 1   ? NBTTagCompound_setByte_ascii(t, key, nbt_i8((uint8_t)value))
              : kind == 2 ? NBTTagCompound_setShort_ascii(t, key, nbt_i16((uint16_t)value))
                          : NBTTagCompound_setInteger_ascii(t, key, value);
    CHECK(ok);
    return WORLD_SAVED_DATA_OK;
}
static WorldSavedDataResult set_array(MCObject *c, NBTTagCompound *t, const char *key,
                                      NativeByteArray *value) {
    WorldSavedDataResult result = event((Recorder *)c, "setByteArray", key);
    if (result == WORLD_SAVED_DATA_OK)
        CHECK(NBTTagCompound_setByteArray(t, NBTString_literalASCII(c->heap, key), value));
    return result;
}
static WorldSavedDataResult set_dirty(MCObject *c, MapData *m, bool dirty) {
    Recorder *r = (Recorder *)c;
    CHECK(dirty);
    WorldSavedDataResult result = event(r, "setDirty", "true");
    if (result == WORLD_SAVED_DATA_OK) {
        m->base.dirty = dirty;
        if (r->mode == 2)
            m->playersArrayList = r->replacement;
    }
    return result;
}
static WorldSavedDataResult update_info(MCObject *c, MapInfo *i, int32_t x, int32_t y) {
    Recorder *r = (Recorder *)c;
    WorldSavedDataResult result = event(r, "update", "info");
    if (result != WORLD_SAVED_DATA_OK)
        return result;
    if (r->mode == 3)
        r->map->playersArrayList = r->replacement;
    if (r->mode == 4)
        CHECK(NativeReferenceList_add(r->map->playersArrayList, (MCObject *)i));
    return MapInfo_update_base(i, x, y);
}
static const MapDataDependencies recording = {get_number, get_array, set_number, set_array,
                                              set_dirty};
static const MapInfoDependencies recordingInfo = {update_info};
static void defaults_and_aliases(void) {
    MCObjectHeap *h = heap_new();
    MapData *m = map_new(h), *other = map_new(h);
    CHECK(MapData_isInstance((MCObject *)m));
    CHECK(WorldSavedData_isInstance((MCObject *)m));
    CHECK(m->colors->length == 16384 && m->colors != other->colors && m->colors->values[0] == 0 &&
          m->colors->values[16383] == 0);
    CHECK(m->playersArrayList != other->playersArrayList &&
          m->playersHashMap != other->playersHashMap && m->mapDecorations != other->mapDecorations);
    CHECK(m->xCenter == 0 && m->zCenter == 0 && m->dimension == 0 && m->scale == 0 &&
          !m->base.dirty && !m->base.mapName);
    NBTString *name = NBTString_literalASCII(h, "map_9");
    MapData *named = MapData_nativeAllocate(h, NULL, NULL);
    CHECK(named);
    named->xCenter = 91;
    named->scale = -7;
    named->base.dirty = true;
    CHECK(MapData_construct(named, name));
    CHECK(named->base.mapName == name && named->xCenter == 91 && named->scale == -7 &&
          named->base.dirty);
    NativeJavaClass *klass = MapData_nativeClass(h),
                    *base = NativeJavaClass_literal(h, &WorldSavedData_Class);
    CHECK(klass && base);
    bool assign = false;
    CHECK(NativeJavaClass_isAssignableFrom(base, klass, &assign) && assign);
    CHECK(NativeJavaClass_getClass(h, (MCObject *)m) == klass);
    MapInfo *i = MapInfo_nativeAllocate(h, NULL, NULL);
    CHECK(i);
    i->field_176109_i = -9;
    i->field_82569_d = 71;
    CHECK(MapInfo_construct(i, m, NULL));
    CHECK(i->outer == m && i->entityplayerObj == NULL);
    CHECK(i->field_176105_d && i->minX == 0 && i->minY == 0 && i->maxX == 127 && i->maxY == 127 &&
          i->field_176109_i == -9 && i->field_82569_d == 71);
    CHECK(MapInfo_update(i, INT_MIN, INT_MAX) == WORLD_SAVED_DATA_OK);
    CHECK(i->minX == INT_MIN && i->maxY == INT_MAX);
    i->field_176105_d = false;
    CHECK(MapInfo_update(i, -5, 129) == WORLD_SAVED_DATA_OK);
    CHECK(i->field_176105_d && i->minX == -5 && i->minY == 129 && i->maxX == -5 && i->maxY == 129 &&
          i->field_176109_i == -9);
    CHECK(MapInfo_new(h, NULL, NULL, NULL, NULL));
    MCObjectHeap_free(h);
}
static void exact_read_write(void) {
    const int lengths[] = {-1, 0, 1, 127, 16383, 16384, 16385, 20000};
    const int scales[] = {-128, -1, 0, 4, 127};
    for (size_t n = 0; n < sizeof(lengths) / sizeof(*lengths); n++)
        for (size_t k = 0; k < sizeof(scales) / sizeof(*scales); k++) {
            MCObjectHeap *h = heap_new();
            MapData *m = map_new(h);
            NativeByteArray *bytes = array(h, lengths[n], 17);
            NBTTagCompound *t = input(h, 128, 128, scales[k], bytes);
            NativeByteArray *old = m->colors;
            m->base.dirty = true;
            CHECK(WorldSavedData_readFromNBT(&m->base, t) == WORLD_SAVED_DATA_OK);
            CHECK(m->colors == bytes && m->colors != old && m->dimension == -1 &&
                  m->xCenter == -123456789 && m->zCenter == 987654321);
            CHECK(m->scale == (scales[k] < 0 ? 0 : scales[k] > 4 ? 4 : scales[k]) && m->base.dirty);
            CHECK(NBTTagCompound_getByteArray(t, NBTString_literalASCII(h, "colors")) == bytes);
            NBTTagCompound *out = NBTTagCompound_new(h);
            CHECK(out);
            CHECK(NBTTagCompound_setInteger_ascii(out, "foreign", 123));
            m->scale = 127;
            CHECK(WorldSavedData_writeToNBT(&m->base, out) == WORLD_SAVED_DATA_OK);
            CHECK(NBTTagCompound_getByteArray(out, NBTString_literalASCII(h, "colors")) == bytes);
            CHECK(NBTTagCompound_getShort_ascii(out, "width") == 128 &&
                  NBTTagCompound_getShort_ascii(out, "height") == 128);
            CHECK(NBTTagCompound_getByte_ascii(out, "scale") == 127 &&
                  NBTTagCompound_getInteger_ascii(out, "foreign") == 123 && m->base.dirty);
            if (bytes && bytes->length) {
                bytes->values[0] = -99;
                CHECK(m->colors->values[0] == -99);
                m->colors->values[bytes->length - 1] = 63;
                CHECK(bytes->values[bytes->length - 1] == 63);
            }
            CHECK(!MCObjectHeap_failed(h));
            MCObjectHeap_free(h);
        }
}
static void malformed_reads(void) {
    const int dims[][2] = {{-32768, 1}, {1, -32768}, {0, 129},   {129, 0},   {1, 1},
                           {2, 2},      {127, 127},  {129, 129}, {256, 256}, {32767, 32767}};
    const int lengths[] = {-1, 0, 1, 16384, 20000};
    for (size_t d = 0; d < sizeof(dims) / sizeof(*dims); d++)
        for (size_t l = 0; l < sizeof(lengths) / sizeof(*lengths); l++) {
            MCObjectHeap *h = heap_new();
            MapData *m = map_new(h);
            NativeByteArray *old = m->colors, *bytes = array(h, lengths[l], 23);
            NBTTagCompound *t = input(h, dims[d][0], dims[d][1], 127, bytes);
            WorldSavedDataResult r = MapData_readFromNBT(m, t);
            CHECK(r == WORLD_SAVED_DATA_OK || r == WORLD_SAVED_DATA_EXCEPTION);
            CHECK(!MCObjectHeap_failed(h));
            CHECK(m->colors && m->colors != old && m->colors != bytes &&
                  m->colors->length == 16384);
            CHECK(m->dimension == -1 && m->scale == 4 && m->xCenter == -123456789 &&
                  m->zCenter == 987654321);
            if (dims[d][0] <= 0 || dims[d][1] <= 0)
                CHECK(r == WORLD_SAVED_DATA_OK);
            if (dims[d][0] == 1 && dims[d][1] == 1 && lengths[l] > 0)
                CHECK(r == WORLD_SAVED_DATA_OK && m->colors->values[63 + 63 * 128] == 23);
            if (dims[d][0] == 2 && dims[d][1] == 2 && lengths[l] == 1) {
                CHECK(r == WORLD_SAVED_DATA_EXCEPTION);
                CHECK(m->colors->values[63 + 63 * 128] == 23 &&
                      m->colors->values[64 + 63 * 128] == 0);
            }
            if (dims[d][0] == 256 && dims[d][1] == 256)
                CHECK(r == WORLD_SAVED_DATA_EXCEPTION && m->colors->values[0] == 0);
            MCObjectHeap_free(h);
        }
}
static void failure_prefixes(void) {
    for (int fail = 1; fail <= 7; fail++) {
        MCObjectHeap *h = heap_new();
        Recorder *r = recorder(h);
        MapData *m = MapData_new(h, NULL, &recording, (MCObject *)r);
        CHECK(m);
        r->map = m;
        r->failAt = fail;
        m->dimension = 5;
        m->xCenter = 700;
        m->zCenter = -800;
        m->scale = 2;
        NativeByteArray *old = m->colors;
        NBTTagCompound *t = input(h, 2, 2, 127, array(h, 16384, 3));
        CHECK(MapData_readFromNBT(m, t) == WORLD_SAVED_DATA_EXCEPTION);
        CHECK(r->calls == fail && m->colors == old && !MCObjectHeap_failed(h));
        CHECK(m->dimension == (fail > 1 ? -1 : 5));
        CHECK(m->xCenter == (fail > 2 ? -123456789 : 700));
        CHECK(m->zCenter == (fail > 3 ? 987654321 : -800));
        CHECK(m->scale == (fail > 4 ? 4 : 2));
        r->calls = 0;
        r->events[0] = 0;
        NBTTagCompound *out = NBTTagCompound_new(h);
        CHECK(out);
        CHECK(NBTTagCompound_setInteger_ascii(out, "foreign", 5));
        CHECK(MapData_writeToNBT(m, out) == WORLD_SAVED_DATA_EXCEPTION);
        CHECK(r->calls == fail && out->count == fail && !MCObjectHeap_failed(h));
        MCObjectHeap_free(h);
    }
    MCObjectHeap *h = heap_new();
    Recorder *r = recorder(h);
    MapData *m = MapData_new(h, NULL, &recording, (MCObject *)r);
    CHECK(m);
    r->map = m;
    r->mode = 1;
    NBTTagCompound *out = NBTTagCompound_new(h);
    CHECK(out);
    m->dimension = 7;
    m->xCenter = 9;
    m->scale = 2;
    CHECK(MapData_writeToNBT(m, out) == WORLD_SAVED_DATA_OK);
    CHECK(NBTTagCompound_getByte_ascii(out, "dimension") == 7 &&
          NBTTagCompound_getInteger_ascii(out, "xCenter") == 4455 &&
          NBTTagCompound_getByte_ascii(out, "scale") == -128);
    CHECK(strcmp(r->events, "setByte:dimension;setInt:xCenter;setInt:zCenter;setByte:scale;"
                            "setShort:width;setShort:height;setByteArray:colors;") == 0);
    int calls = r->calls;
    CHECK(MapData_readFromNBT(m, NULL) == WORLD_SAVED_DATA_EXCEPTION);
    CHECK(MapData_writeToNBT(m, NULL) == WORLD_SAVED_DATA_EXCEPTION);
    CHECK(r->calls == calls && !MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void players_dirty_and_packets(void) {
    MCObjectHeap *h = heap_new();
    MapData *m = map_new(h);
    MapInfo *i = NULL, *j = NULL;
    CHECK(MapData_getMapInfo(m, NULL, &i) == WORLD_SAVED_DATA_OK && i && i->outer == m &&
          !i->entityplayerObj);
    CHECK(MapData_getMapInfo(m, NULL, &j) == WORLD_SAVED_DATA_OK && j == i &&
          NativeReferenceList_size(m->playersArrayList) == 1);
    MCGameplayPlayer *p = MCGameplayPlayer_nativeAllocate(h),
                     *equal = MCGameplayPlayer_nativeAllocate(h);
    CHECK(p && equal);
    ((Entity *)p)->entityId = 71;
    ((Entity *)equal)->entityId = 71;
    CHECK(MapData_getMapInfo(m, p, &i) == WORLD_SAVED_DATA_OK);
    CHECK(MapData_getMapInfo(m, equal, &j) == WORLD_SAVED_DATA_OK && j == i &&
          j->entityplayerObj == p);
    ((Entity *)equal)->entityId = 72;
    CHECK(MapData_getMapInfo(m, equal, &j) == WORLD_SAVED_DATA_OK && j != i);
    CHECK(MapData_updateMapData(m, -7, 500) == WORLD_SAVED_DATA_OK && m->base.dirty &&
          i->minX == -7 && i->maxY == 500 && j->minX == -7);
    S34PacketMaps *sentinel = (S34PacketMaps *)m, *packet = sentinel;
    CHECK(MapData_getMapPacket(m, NULL, NULL, NULL, &packet) == WORLD_SAVED_DATA_EXCEPTION &&
          packet == sentinel);
    MapInfo *nullinfo = (MapInfo *)NativeHashMap_get(m->playersHashMap, NULL);
    CHECK(!nullinfo->field_176105_d);
    CHECK(MapData_getMapPacket(m, NULL, NULL, NULL, &packet) == WORLD_SAVED_DATA_EXCEPTION &&
          nullinfo->field_176109_i == 1);
    for (int n = 1; n <= 4; n++) {
        packet = sentinel;
        CHECK(MapData_getMapPacket(m, NULL, NULL, NULL, &packet) == WORLD_SAVED_DATA_OK && !packet);
    }
    CHECK(nullinfo->field_176109_i == 5);
    CHECK(
        NativeHashMap_put(m->playersHashMap, NULL, (MCObject *)NBTString_literalASCII(h, "wrong")));
    i = (MapInfo *)m;
    CHECK(MapData_getMapInfo(m, NULL, &i) == WORLD_SAVED_DATA_EXCEPTION && i == (MapInfo *)m &&
          !MCObjectHeap_failed(h));
    CHECK(NativeHashMap_put(m->playersHashMap, NULL, NULL));
    CHECK(MapData_getMapInfo(m, NULL, &i) == WORLD_SAVED_DATA_OK && i);
    CHECK(NativeReferenceList_add(m->playersArrayList, NULL));
    CHECK(MapData_updateMapData(m, 2, 3) == WORLD_SAVED_DATA_EXCEPTION && m->base.dirty &&
          !MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h = heap_new();
    m = map_new(h);
    m->playersArrayList = NULL;
    i = (MapInfo *)m;
    CHECK(MapData_getMapInfo(m, NULL, &i) == WORLD_SAVED_DATA_EXCEPTION && i == (MapInfo *)m &&
          !MCObjectHeap_failed(h));
    j = (MapInfo *)NativeHashMap_get(m->playersHashMap, NULL);
    CHECK(j && j->outer == m);
    CHECK(MapData_getMapInfo(m, NULL, &i) == WORLD_SAVED_DATA_OK && i == j);
    MCObjectHeap_free(h);
}
static void iterator_captures(void) {
    MCObjectHeap *h = heap_new();
    Recorder *r = recorder(h);
    MapData *m = MapData_new(h, NULL, &recording, (MCObject *)r);
    CHECK(m);
    r->map = m;
    MapInfo *a = MapInfo_new(h, m, NULL, &recordingInfo, (MCObject *)r),
            *b = MapInfo_new(h, m, NULL, NULL, NULL);
    CHECK(a && b);
    CHECK(NativeReferenceList_add(m->playersArrayList, (MCObject *)a));
    CHECK(NativeReferenceList_add(m->playersArrayList, (MCObject *)b));
    NativeReferenceList *old = m->playersArrayList;
    r->replacement = NativeReferenceList_new(h);
    CHECK(r->replacement);
    r->mode = 3;
    CHECK(MapData_updateMapData(m, -100, 400) == WORLD_SAVED_DATA_OK);
    CHECK(m->playersArrayList != old && a->minX == -100 && b->maxY == 400);
    r->mode = 2;
    m->playersArrayList = old;
    CHECK(MapData_updateMapData(m, -200, 500) == WORLD_SAVED_DATA_OK);
    CHECK(m->playersArrayList == r->replacement && a->minX == -100 && b->maxY == 400);
    MCObjectHeap_free(h);
    h = heap_new();
    r = recorder(h);
    m = MapData_new(h, NULL, NULL, NULL);
    CHECK(m);
    r->map = m;
    r->mode = 4;
    a = MapInfo_new(h, m, NULL, &recordingInfo, (MCObject *)r);
    CHECK(a);
    b = MapInfo_new(h, m, NULL, NULL, NULL);
    CHECK(b);
    CHECK(NativeReferenceList_add(m->playersArrayList, (MCObject *)a));
    CHECK(NativeReferenceList_add(m->playersArrayList, (MCObject *)b));
    CHECK(MapData_updateMapData(m, -10, 200) == WORLD_SAVED_DATA_FAILURE);
    CHECK(MCObjectHeap_failed(h) && m->base.dirty && a->minX == -10 && b->minX == 0);
    MCObjectHeap_free(h);
}
static void centers_and_decorations(void) {
    MCObjectHeap *h = heap_new();
    MapData *m = map_new(h);
    CHECK(MapData_calculateMapCenter(m, 0, 0, 0) == WORLD_SAVED_DATA_OK && m->xCenter == 0 &&
          m->zCenter == 0);
    CHECK(MapData_calculateMapCenter(m, -64.01, 64, 0) == WORLD_SAVED_DATA_OK &&
          m->xCenter == -128 && m->zCenter == 128);
    CHECK(MapData_calculateMapCenter(m, 0, 0, 1) == WORLD_SAVED_DATA_OK && m->xCenter == 64 &&
          m->zCenter == 64);
    CHECK(MapData_calculateMapCenter(m, 1, -1, 25) == WORLD_SAVED_DATA_OK && m->xCenter == -64 &&
          m->zCenter == -64);
    CHECK(MapData_calculateMapCenter(m, INFINITY, NAN, INT_MIN) == WORLD_SAVED_DATA_OK);
    m->xCenter = 0;
    m->zCenter = 0;
    m->scale = 0;
    NBTString *id = NBTString_literalASCII(h, "marker");
    CHECK(MapData_updateDecorations(m, 257, NULL, id, 0, 0, -10) == WORLD_SAVED_DATA_OK);
    Vec4b *v = (Vec4b *)NativeLinkedHashMap_get(m->mapDecorations, (MCObject *)id);
    CHECK(v && v->field_176117_a == 1 && v->field_176114_d == 0);
    CHECK(MapData_updateDecorations(m, 0, NULL, NULL, -63, 63, 0) == WORLD_SAVED_DATA_OK);
    v = (Vec4b *)NativeLinkedHashMap_get(m->mapDecorations, NULL);
    CHECK(v && v->field_176115_b == -125 && v->field_176116_c == 126);
    CHECK(MapData_updateDecorations(m, 0, NULL, id, -64, 319, 0) == WORLD_SAVED_DATA_OK);
    v = (Vec4b *)NativeLinkedHashMap_get(m->mapDecorations, (MCObject *)id);
    CHECK(v && v->field_176117_a == 6 && v->field_176115_b == -128 && v->field_176116_c == 127);
    CHECK(MapData_updateDecorations(m, 0, NULL, id, 320, 0, 0) == WORLD_SAVED_DATA_OK &&
          !NativeLinkedHashMap_containsKey(m->mapDecorations, (MCObject *)id));
    CHECK(MapData_updateDecorations(m, 0, NULL, id, 0, 0, 0) == WORLD_SAVED_DATA_OK);
    CHECK(MapData_updateDecorations(m, 0, NULL, id, NAN, 0, 0) == WORLD_SAVED_DATA_OK &&
          !NativeLinkedHashMap_containsKey(m->mapDecorations, (MCObject *)id));
    m->dimension = -1;
    CHECK(MapData_updateDecorations(m, 0, NULL, id, 0, 0, 0) == WORLD_SAVED_DATA_EXCEPTION &&
          !MCObjectHeap_failed(h));
    CHECK(MapData_updateDecorations(m, 0, NULL, id, 64, 0, 0) == WORLD_SAVED_DATA_OK);
    MCObjectHeap_free(h);
}
static void lifetime_and_boundaries(void) {
    MCObjectHeap *h = heap_new();
    Recorder *r = recorder(h);
    MapData *m = MapData_new(h, NULL, &recording, (MCObject *)r);
    CHECK(m);
    r->map = m;
    MapInfo *i = NULL;
    CHECK(MapData_getMapInfo(m, NULL, &i) == WORLD_SAVED_DATA_OK);
    i->nativeContext = (MCObject *)r;
    i->nativeDependencies = &recordingInfo;
    NBTTagCompound *t = input(h, 128, 128, 0, array(h, 1, 37));
    CHECK(MapData_readFromNBT(m, t) == WORLD_SAVED_DATA_OK);
    CHECK(MapData_updateDecorations(m, 0, NULL, NULL, 1, 2, 3) == WORLD_SAVED_DATA_OK);
    MCObjectRoot mapRoot = {0}, tagRoot = {0}, infoRoot = {0};
    CHECK(MCObjectRoot_init(&mapRoot, h, (MCObject *)m));
    CHECK(MCObjectRoot_init(&tagRoot, h, (MCObject *)t));
    CHECK(MCObjectRoot_init(&infoRoot, h, (MCObject *)i));
    CHECK(MCObjectHeap_collect(h));
    MCObjectHeap *clone = MCObjectHeap_clone(h);
    CHECK(clone);
    MCObjectRoot copiedMap = {0}, copiedTag = {0}, copiedInfo = {0};
    CHECK(MCObjectRoot_rebind(&copiedMap, clone, &mapRoot));
    CHECK(MCObjectRoot_rebind(&copiedTag, clone, &tagRoot));
    CHECK(MCObjectRoot_rebind(&copiedInfo, clone, &infoRoot));
    MapData *cm = (MapData *)MCObjectRoot_get(&copiedMap);
    NBTTagCompound *ct = (NBTTagCompound *)MCObjectRoot_get(&copiedTag);
    MapInfo *ci = (MapInfo *)MCObjectRoot_get(&copiedInfo);
    CHECK(cm != m && cm->colors != m->colors &&
          cm->colors == NBTTagCompound_getByteArray(ct, NBTString_literalASCII(clone, "colors")));
    CHECK(ci->outer == cm && ((Recorder *)cm->base.nativeContext)->map == cm &&
          ci->nativeContext == cm->base.nativeContext);
    CHECK(NativeHashMap_get(cm->playersHashMap, NULL) == (MCObject *)ci &&
          NativeReferenceList_get(cm->playersArrayList, 0) == (MCObject *)ci);
    cm->colors->values[0] = 99;
    CHECK(m->colors->values[0] == 37);
    CHECK(MapData_updateMapData(cm, -9, 200) == WORLD_SAVED_DATA_OK);
    CHECK(MCObjectHeap_adopt(h, clone));
    MCObjectHeap_free(clone);
    m = (MapData *)MCObjectRoot_get(&mapRoot);
    i = (MapInfo *)MCObjectRoot_get(&infoRoot);
    CHECK(m->colors->values[0] == 99 && i->outer == m && i->minX == -9 && i->maxY == 200);
    CHECK(MCObjectHeap_collect(h));
    MCObjectRoot_drop(&mapRoot);
    MCObjectRoot_drop(&tagRoot);
    MCObjectRoot_drop(&infoRoot);
    MCObjectHeap_free(h);
    h = heap_new();
    m = map_new(h);
    MCObject fake = {h, m->base.object.klass};
    CHECK(!MapData_isInstance(&fake));
    CHECK(MapData_calculateMapCenter((MapData *)&fake, 0, 0, 0) == WORLD_SAVED_DATA_FAILURE &&
          MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h = heap_new();
    m = map_new(h);
    MCObjectHeap *foreign = heap_new();
    NativeByteArray *a = array(foreign, 1, 1);
    m->colors = a;
    t = NBTTagCompound_new(h);
    CHECK(t);
    CHECK(MapData_writeToNBT(m, t) == WORLD_SAVED_DATA_FAILURE && MCObjectHeap_failed(h));
    CHECK(t->count == 6);
    MCObjectHeap_free(h);
    MCObjectHeap_free(foreign);
    h = MCObjectHeap_new(100);
    CHECK(h);
    CHECK(!MapData_new(h, NULL, NULL, NULL) && MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h = heap_new();
    CHECK(NativeByteArray_new(h, 128));
    m = MapData_nativeAllocate(h, NULL, NULL);
    CHECK(m);
    NBTString *name = NBTString_literalASCII(h, "name");
    CHECK(name);
    size_t prefix = MCObjectHeap_liveBytes(h);
    MCObjectHeap_free(h);
    h = MCObjectHeap_new(prefix);
    CHECK(h);
    CHECK(NativeByteArray_new(h, 128));
    m = MapData_nativeAllocate(h, NULL, NULL);
    CHECK(m);
    name = NBTString_literalASCII(h, "name");
    CHECK(name);
    m->xCenter = 91;
    CHECK(!MapData_construct(m, name) && MCObjectHeap_failed(h));
    CHECK(m->base.mapName == name && m->xCenter == 91 && !m->colors && !m->playersArrayList &&
          !m->playersHashMap && !m->mapDecorations);
    MCObjectHeap_free(h);
    h = MCObjectHeap_new(prefix + sizeof(NativeByteArray) + 16384);
    CHECK(h);
    CHECK(NativeByteArray_new(h, 128));
    m = MapData_nativeAllocate(h, NULL, NULL);
    CHECK(m);
    name = NBTString_literalASCII(h, "name");
    CHECK(name);
    CHECK(!MapData_construct(m, name) && MCObjectHeap_failed(h));
    CHECK(m->base.mapName == name && m->colors && m->colors->length == 16384 &&
          !m->playersArrayList && !m->playersHashMap);
    MCObjectHeap_free(h);
    h = heap_new();
    m = map_new(h);
    MCObject context = {h, m->base.object.klass};
    CHECK(!MapData_nativeAllocate(h, NULL, &context) && MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h = heap_new();
    m = map_new(h);
    i = MapInfo_new(h, m, NULL, NULL, NULL);
    CHECK(i);
    MCObject *tiny = MCObjectHeap_alloc(h, sizeof(MCObject), i->object.klass);
    CHECK(tiny && !MapInfo_isInstance(tiny));
    CHECK(NativeHashMap_put(m->playersHashMap, NULL, tiny));
    MapInfo *out = i;
    CHECK(MapData_getMapInfo(m, NULL, &out) == WORLD_SAVED_DATA_FAILURE && out == i &&
          MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    CHECK(MapData_readFromNBT(NULL, NULL) == WORLD_SAVED_DATA_EXCEPTION &&
          MapInfo_update(NULL, 0, 0) == WORLD_SAVED_DATA_EXCEPTION);
}
int main(void) {
    defaults_and_aliases();
    exact_read_write();
    malformed_reads();
    failure_prefixes();
    players_dirty_and_packets();
    iterator_captures();
    centers_and_decorations();
    lifetime_and_boundaries();
    printf("source MapData: %u checks passed\n", checks);
    return 0;
}
