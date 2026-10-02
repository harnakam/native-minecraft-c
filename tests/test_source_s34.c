#include "item/ItemStack.h"
#include "network/play/server/S34PacketMaps.h"
#include "util/NativeReferenceList.h"
#include "util/Vec4b.h"
#include "world/storage/MapData.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "S34 check %u line %d: %s\n", checks, __LINE__, #x);                   \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static NativeReferenceList *icons(MCObjectHeap *heap, Vec4b *icon) {
    NativeReferenceList *list = NativeReferenceList_new(heap);
    CHECK(list && NativeReferenceList_add(list, (MCObject *)icon));
    CHECK(NativeReferenceList_add(list, (MCObject *)icon));
    return list;
}
static void constructor_ownership(void) {
    MCObjectHeap *heap = MCObjectHeap_new(1 << 22);
    Vec4b *icon = Vec4b_new(heap, 18, -128, 127, -1);
    NativeReferenceList *list = icons(heap, icon);
    NativeByteArray *colors = NativeByteArray_new(heap, 16384);
    CHECK(heap && icon && colors);
    colors->values[3 + 4 * 128] = 12;
    colors->values[4 + 4 * 128] = 13;
    colors->values[3 + 5 * 128] = 14;
    colors->values[4 + 5 * 128] = 15;
    S34PacketMaps *packet = S34PacketMaps_new_empty(heap);
    CHECK(packet && S34PacketMaps_construct(packet, 300, -2, (MCObject *)list, colors, 3, 4, 2,
                                            2) == NATIVE_ARRAY_OK);
    CHECK(packet->mapVisiblePlayersVec4b->length == 2);
    CHECK(packet->mapVisiblePlayersVec4b->values[0] == (MCObject *)icon &&
          packet->mapVisiblePlayersVec4b->values[1] == (MCObject *)icon);
    CHECK(packet->mapDataBytes != colors && packet->mapDataBytes->length == 4);
    CHECK(memcmp(packet->mapDataBytes->values, (int8_t[]){12, 13, 14, 15}, 4) == 0);
    colors->values[3 + 4 * 128] = 99;
    CHECK(packet->mapDataBytes->values[0] == 12);
    CHECK(Vec4b_construct(icon, 7, 8, 9, 10));
    CHECK(((Vec4b *)packet->mapVisiblePlayersVec4b->values[0])->field_176117_a == 7);
    NativeReferenceList *empty = NativeReferenceList_new(heap);
    S34PacketMaps *wide = S34PacketMaps_new_empty(heap);
    CHECK(wide && empty &&
          S34PacketMaps_construct(wide, -1, -1, (MCObject *)empty, NULL, -7, INT32_MIN, 129, 0) ==
              NATIVE_ARRAY_OK);
    CHECK(wide->mapMaxX == 129 && wide->mapDataBytes->length == 0);
    mc_buf wire = {0};
    PacketBuffer view;
    CHECK(PacketBuffer_init(&view, heap, &wire));
    CHECK(S34PacketMaps_writePacketData(wide, &view) == NATIVE_ARRAY_OK);
    CHECK(wire.len == 12 && wire.data[5] == 255 && wire.data[7] == 129);
    mc_buf_free(&wire);
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}
static void source_exception_prefixes(void) {
    MCObjectHeap *heap = MCObjectHeap_new(1 << 22);
    NativeReferenceList *empty = NativeReferenceList_new(heap);
    S34PacketMaps *packet = S34PacketMaps_new_empty(heap);
    CHECK(heap && empty && packet);
    CHECK(S34PacketMaps_construct(packet, 71, -8, NULL, NULL, 2, 3, 1, 1) ==
          NATIVE_ARRAY_EXCEPTION);
    CHECK(packet->mapId == 71 && packet->mapScale == -8 && !packet->mapVisiblePlayersVec4b &&
          packet->mapMinX == 0 && !MCObjectHeap_failed(heap));
    CHECK(S34PacketMaps_construct(packet, 72, 2, (MCObject *)empty, NULL, 2, 3, -1, 1) ==
          NATIVE_ARRAY_EXCEPTION);
    CHECK(packet->mapMinX == 2 && packet->mapMinY == 3 && packet->mapMaxX == -1 &&
          packet->mapMaxY == 1 && !packet->mapDataBytes && !MCObjectHeap_failed(heap));
    NativeByteArray *shortColors = NativeByteArray_new(heap, 129);
    CHECK(shortColors);
    shortColors->values[0] = 11;
    shortColors->values[1] = 13;
    shortColors->values[128] = 12;
    CHECK(S34PacketMaps_construct(packet, 73, 3, (MCObject *)empty, shortColors, 0, 0, 2, 2) ==
          NATIVE_ARRAY_EXCEPTION);
    /* X is the outer loop: source0,128,1 succeed before source129 fails. */
    CHECK(packet->mapDataBytes->length == 4 && packet->mapDataBytes->values[0] == 11 &&
          packet->mapDataBytes->values[2] == 12 && packet->mapDataBytes->values[1] == 13 &&
          packet->mapDataBytes->values[3] == 0);
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}
static void map_info_cadence_and_failure(void) {
    MCObjectHeap *heap = MCObjectHeap_new(1 << 23);
    MapData *map = MapData_new(heap, NULL, NULL, NULL);
    MapInfo *info = MapInfo_new(heap, map, NULL, NULL, NULL);
    ItemStack *stack = ItemStack_new(heap, ItemStack_registryItem(358), 1, 41);
    S34PacketMaps *packet = NULL;
    CHECK(heap && map && info && stack);
    CHECK(MapInfo_getPacket(info, stack, &packet) == WORLD_SAVED_DATA_OK && packet &&
          packet->mapId == 41 && packet->mapMaxX == 128 && !info->field_176105_d &&
          info->field_176109_i == 0);
    CHECK(MapInfo_getPacket(info, stack, &packet) == WORLD_SAVED_DATA_OK && packet &&
          packet->mapMaxX == 0 && info->field_176109_i == 1);
    for (int32_t i = 1; i < 5; ++i) {
        CHECK(MapInfo_getPacket(info, NULL, &packet) == WORLD_SAVED_DATA_OK && !packet &&
              info->field_176109_i == i + 1);
    }
    CHECK(MapInfo_getPacket(info, NULL, &packet) == WORLD_SAVED_DATA_EXCEPTION && !packet &&
          info->field_176109_i == 6 && !MCObjectHeap_failed(heap));
    info->field_176105_d = true;
    packet = (S34PacketMaps *)stack;
    CHECK(MapInfo_getPacket(info, NULL, &packet) == WORLD_SAVED_DATA_EXCEPTION &&
          packet == (S34PacketMaps *)stack && !info->field_176105_d && info->field_176109_i == 6);
    info->field_176109_i = INT32_MAX;
    info->outer = NULL;
    CHECK(MapInfo_getPacket(info, NULL, &packet) == WORLD_SAVED_DATA_OK && !packet &&
          info->field_176109_i == INT32_MIN);
    info->field_176109_i = -5;
    CHECK(MapInfo_getPacket(info, stack, &packet) == WORLD_SAVED_DATA_EXCEPTION && !packet &&
          info->field_176109_i == -4 && !MCObjectHeap_failed(heap));
    info->outer = map;
    info->field_176105_d = true;
    info->minX = 0;
    info->minY = 0;
    info->maxX = 255;
    info->maxY = 127;
    CHECK(MapInfo_getPacket(info, stack, &packet) == WORLD_SAVED_DATA_EXCEPTION && !packet &&
          !info->field_176105_d && !MCObjectHeap_failed(heap));
    CHECK(!MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}
static void io_prefixes_and_reuse(void) {
    const uint8_t wire[] = {0xac, 0x02, 0xfe, 0x01, 0x2f, 0x80, 0x7f, 0x02,
                            0x02, 0x03, 0x04, 0x04, 12,   13,   14,   15};
    for (size_t length = 0; length <= sizeof(wire); ++length) {
        MCObjectHeap *heap = MCObjectHeap_new(1 << 22);
        S34PacketMaps *packet = S34PacketMaps_new_empty(heap);
        NativeByteArray *old = NativeByteArray_new(heap, 1);
        NativeTypedObjectArray *oldIcons = NULL;
        CHECK(heap && packet && old &&
              NativeTypedObjectArray_new(heap, Vec4b_nativeClass(heap), 0, &oldIcons) ==
                  NATIVE_ARRAY_OK);
        packet->mapId = 9;
        packet->mapScale = 8;
        packet->mapVisiblePlayersVec4b = oldIcons;
        packet->mapMaxX = 7;
        packet->mapMaxY = 6;
        packet->mapMinX = 5;
        packet->mapMinY = 4;
        packet->mapDataBytes = old;
        mc_buf input = {(uint8_t *)wire, length, length, 0, false};
        PacketBuffer view;
        CHECK(PacketBuffer_init(&view, heap, &input));
        size_t before = MCObjectHeap_liveObjects(heap);
        CHECK(S34PacketMaps_readPacketData(packet, &view) ==
              (length == sizeof(wire) ? NATIVE_ARRAY_OK : NATIVE_ARRAY_EXCEPTION));
        CHECK(packet->mapId == (length >= 2 ? 300 : 9));
        CHECK(packet->mapScale == (length >= 3 ? -2 : 8));
        if (length < 4)
            CHECK(packet->mapVisiblePlayersVec4b == oldIcons);
        else {
            CHECK(packet->mapVisiblePlayersVec4b != oldIcons &&
                  packet->mapVisiblePlayersVec4b->length == 1);
            CHECK((packet->mapVisiblePlayersVec4b->values[0] != NULL) == (length >= 7));
            /* The packed byte has been read, then Vec4b allocated before b/c. */
            if (length == 5 || length == 6)
                CHECK(MCObjectHeap_liveObjects(heap) == before + 2);
        }
        CHECK(packet->mapMaxX == (length >= 8 ? 2 : 7));
        CHECK(packet->mapMaxY == (length >= 9 ? 2 : 6));
        CHECK(packet->mapMinX == (length >= 10 ? 3 : 5));
        CHECK(packet->mapMinY == 4);
        CHECK((packet->mapDataBytes != old) == (length == sizeof(wire)));
        CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
    }
    MCObjectHeap *heap = MCObjectHeap_new(1 << 22);
    S34PacketMaps *packet = S34PacketMaps_new_empty(heap);
    NativeByteArray *old = NativeByteArray_new(heap, 1);
    packet->mapMaxY = 11;
    packet->mapMinX = 12;
    packet->mapMinY = 13;
    packet->mapDataBytes = old;
    const uint8_t empty[] = {0x03, 0x05, 0x00, 0x00};
    mc_buf input = {(uint8_t *)empty, sizeof(empty), sizeof(empty), 0, false};
    PacketBuffer view;
    CHECK(PacketBuffer_init(&view, heap, &input));
    CHECK(S34PacketMaps_readPacketData(packet, &view) == NATIVE_ARRAY_OK && packet->mapMaxX == 0 &&
          packet->mapMaxY == 11 && packet->mapMinX == 12 && packet->mapMinY == 13 &&
          packet->mapDataBytes == old);
    mc_buf output = {0};
    CHECK(PacketBuffer_init(&view, heap, &output));
    packet->mapVisiblePlayersVec4b = NULL;
    CHECK(S34PacketMaps_writePacketData(packet, &view) == NATIVE_ARRAY_EXCEPTION &&
          output.len == 2 && output.data[0] == 3 && output.data[1] == 5);
    CHECK(!MCObjectHeap_failed(heap));
    mc_buf_free(&output);
    const uint8_t negative[] = {0x04, 0x80, 0xff, 0xff, 0xff, 0xff, 0x0f};
    input = (mc_buf){(uint8_t *)negative, sizeof(negative), sizeof(negative), 0, false};
    CHECK(PacketBuffer_init(&view, heap, &input));
    CHECK(S34PacketMaps_readPacketData(packet, &view) == NATIVE_ARRAY_EXCEPTION &&
          packet->mapId == 4 && packet->mapScale == INT8_MIN && !packet->mapVisiblePlayersVec4b &&
          input.pos == sizeof(negative) && !MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}
static void apply_aliases_and_graph(void) {
    MCObjectHeap *heap = MCObjectHeap_new(1 << 23);
    MapData *map = MapData_new(heap, NBTString_fromASCII(heap, "map_71"), NULL, NULL);
    Vec4b *icon = Vec4b_new(heap, 6, 7, 8, 9);
    NativeReferenceList *list = icons(heap, icon);
    CHECK(NativeReferenceList_add(list, NULL));
    S34PacketMaps *packet = S34PacketMaps_new_empty(heap);
    CHECK(map && packet &&
          S34PacketMaps_construct(packet, 71, 3, (MCObject *)list, map->colors, 0, 0, 0, 0) ==
              NATIVE_ARRAY_OK);
    NativeByteArray *alias = map->colors;
    packet->mapMaxX = packet->mapMaxY = 2;
    packet->mapDataBytes = NativeByteArray_new(heap, 1);
    CHECK(packet->mapDataBytes);
    packet->mapDataBytes->values[0] = 31;
    CHECK(S34PacketMaps_setMapdataTo(packet, map) == NATIVE_ARRAY_EXCEPTION);
    CHECK(map->colors == alias && map->colors->values[0] == 31 && map->colors->values[1] == 0 &&
          map->colors->values[128] == 0 && map->scale == 3 && !map->base.dirty);
    NBTString *key0 = NBTString_fromASCII(heap, "icon-0"),
              *key1 = NBTString_fromASCII(heap, "icon-1"),
              *key2 = NBTString_fromASCII(heap, "icon-2");
    CHECK(NativeLinkedHashMap_get(map->mapDecorations, (MCObject *)key0) == (MCObject *)icon &&
          NativeLinkedHashMap_get(map->mapDecorations, (MCObject *)key1) == (MCObject *)icon &&
          NativeLinkedHashMap_containsKey(map->mapDecorations, (MCObject *)key2) &&
          !NativeLinkedHashMap_get(map->mapDecorations, (MCObject *)key2));
    packet->mapVisiblePlayersVec4b = NULL;
    CHECK(S34PacketMaps_setMapdataTo(packet, map) == NATIVE_ARRAY_EXCEPTION &&
          NativeLinkedHashMap_size(map->mapDecorations) == 0 && map->colors == alias);
    CHECK(S34PacketMaps_construct(packet, 71, 4, (MCObject *)list, NULL, 0, 0, 0, 0) ==
          NATIVE_ARRAY_OK);
    CHECK(S34PacketMaps_setMapdataTo(packet, map) == NATIVE_ARRAY_OK);
    NBTTagCompound *tag = NBTTagCompound_new(heap);
    CHECK(tag && MapData_writeToNBT(map, tag) == WORLD_SAVED_DATA_OK);
    CHECK(NBTTagCompound_getByteArray(tag, NBTString_literalASCII(heap, "colors")) == alias);
    MCObjectRoot mapRoot = {0}, packetRoot = {0}, tagRoot = {0};
    CHECK(MCObjectRoot_init(&mapRoot, heap, (MCObject *)map));
    CHECK(MCObjectRoot_init(&packetRoot, heap, (MCObject *)packet));
    CHECK(MCObjectRoot_init(&tagRoot, heap, (MCObject *)tag));
    CHECK(MCObjectHeap_collect(heap));
    MCObjectHeap *working = MCObjectHeap_clone(heap);
    CHECK(working);
    MCObjectRoot wm = {0}, wp = {0}, wt = {0};
    CHECK(MCObjectRoot_rebind(&wm, working, &mapRoot) &&
          MCObjectRoot_rebind(&wp, working, &packetRoot) &&
          MCObjectRoot_rebind(&wt, working, &tagRoot));
    MapData *copy = (MapData *)MCObjectRoot_get(&wm);
    S34PacketMaps *copyPacket = (S34PacketMaps *)MCObjectRoot_get(&wp);
    NBTTagCompound *copyTag = (NBTTagCompound *)MCObjectRoot_get(&wt);
    CHECK(copy->colors != alias &&
          NBTTagCompound_getByteArray(copyTag, NBTString_literalASCII(working, "colors")) ==
              copy->colors);
    CHECK(copyPacket->mapVisiblePlayersVec4b->values[0] ==
              copyPacket->mapVisiblePlayersVec4b->values[1] &&
          copyPacket->mapVisiblePlayersVec4b->values[0] != (MCObject *)icon);
    CHECK(NativeLinkedHashMap_get(copy->mapDecorations,
                                  (MCObject *)NBTString_fromASCII(working, "icon-0")) ==
          copyPacket->mapVisiblePlayersVec4b->values[0]);
    CHECK(Vec4b_construct((Vec4b *)copyPacket->mapVisiblePlayersVec4b->values[0], 1, 2, 3, 4));
    CHECK(icon->field_176117_a == 6);
    CHECK(MCObjectHeap_adopt(heap, working));
    MCObjectHeap_free(working);
    map = (MapData *)MCObjectRoot_get(&mapRoot);
    packet = (S34PacketMaps *)MCObjectRoot_get(&packetRoot);
    CHECK(NativeLinkedHashMap_get(map->mapDecorations,
                                  (MCObject *)NBTString_fromASCII(heap, "icon-0")) ==
          packet->mapVisiblePlayersVec4b->values[0]);
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}
typedef struct {
    MCObject object;
    S34PacketMaps *received;
    int calls;
    bool refuse;
} Receiver;
static void receiver_trace(MCObject *object, MCObjectVisitor visit, void *context) {
    Receiver *receiver = (Receiver *)object;
    receiver->received = (S34PacketMaps *)visit((MCObject *)receiver->received, context);
}
static const MCObjectClass receiverClass = {"fixture.maps.handler", MCObjectHeap_plainClone,
                                            receiver_trace, NULL};
static bool handled(MCObject *object, S34PacketMaps *packet) {
    Receiver *receiver = (Receiver *)object;
    CHECK(MCObjectHeap_hasBorrowers(object->heap) && !MCObjectHeap_collect(object->heap));
    receiver->received = packet;
    ++receiver->calls;
    return !receiver->refuse;
}
static void handler_and_owner_failure(void) {
    static const INetHandlerPlayClientMethods methods = {.handleMaps = handled};
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        MCObjectHeap *heap = MCObjectHeap_new(1 << 20), *foreign = MCObjectHeap_new(1 << 20);
        S34PacketMaps *packet = S34PacketMaps_new_empty(heap);
        Receiver *receiver = (Receiver *)MCObjectHeap_alloc(scenario == 2 ? foreign : heap,
                                                            sizeof(*receiver), &receiverClass);
        receiver->refuse = scenario == 1;
        INetHandlerPlayClientMethods current = methods;
        if (scenario == 3)
            current.handleMaps = NULL;
        CHECK(S34PacketMaps_processPacket(
                  packet, (INetHandlerPlayClient){(MCObject *)receiver, &current}) ==
              (scenario == 0));
        CHECK(receiver->calls == (scenario < 2 ? 1 : 0) &&
              MCObjectHeap_failed(heap) == (scenario != 0) && !MCObjectHeap_failed(foreign) &&
              !MCObjectHeap_hasBorrowers(heap));
        if (scenario < 2)
            CHECK(receiver->received == packet);
        MCObjectHeap_free(heap);
        MCObjectHeap_free(foreign);
    }
}
static void native_field_and_budget_failures(void) {
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        MCObjectHeap *heap = MCObjectHeap_new(1 << 24), *foreign = MCObjectHeap_new(1 << 20);
        S34PacketMaps *packet = S34PacketMaps_new_empty(heap);
        CHECK(heap && foreign && packet);
        if (scenario == 0)
            packet->mapVisiblePlayersVec4b = NativeObjectArray_new(heap, 0);
        else
            CHECK(NativeTypedObjectArray_new(heap, Vec4b_nativeClass(heap), 0,
                                             &packet->mapVisiblePlayersVec4b) == NATIVE_ARRAY_OK);
        if (scenario == 1)
            packet->mapVisiblePlayersVec4b->componentType = Vec4b_nativeClass(foreign);
        mc_buf output = {0};
        PacketBuffer view;
        if (scenario == 2) {
            packet->mapMaxX = 1;
            packet->mapDataBytes = NativeByteArray_new(heap, MC_MAX_PACKET);
            CHECK(packet->mapDataBytes);
        }
        if (scenario == 3) {
            output.len = output.cap = MC_MAX_PACKET;
            output.data = calloc(1, output.cap);
            CHECK(output.data);
        }
        CHECK(PacketBuffer_init(&view, heap, &output));
        CHECK(S34PacketMaps_writePacketData(packet, &view) == NATIVE_ARRAY_FAILURE &&
              MCObjectHeap_failed(heap) && !MCObjectHeap_failed(foreign) &&
              !MCObjectHeap_hasBorrowers(heap));
        mc_buf_free(&output);
        MCObjectHeap_free(heap);
        MCObjectHeap_free(foreign);
    }
    MCObjectHeap *heap = MCObjectHeap_new(1 << 20);
    S34PacketMaps *packet = S34PacketMaps_new_empty(heap);
    CHECK(packet);
    MCObject *undersized = MCObjectHeap_alloc(heap, sizeof(MCObject), packet->object.klass);
    MCObjectRoot root = {0};
    CHECK(undersized && MCObjectRoot_init(&root, heap, undersized));
    CHECK(!MCObjectHeap_collect(heap) && MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}
typedef struct {
    MCObject object;
    Vec4b *extra;
    MCObject *returned;
    NativeByteArray *originalColors;
    int scenario, sizes, arrays;
} CollectionContext;
static void collection_trace(MCObject *object, MCObjectVisitor visit, void *context) {
    CollectionContext *state = (CollectionContext *)object;
    state->extra = (Vec4b *)visit((MCObject *)state->extra, context);
    state->returned = visit(state->returned, context);
    state->originalColors = (NativeByteArray *)visit((MCObject *)state->originalColors, context);
}
static const MCObjectClass collectionClass = {"fixture.maps.collection", MCObjectHeap_plainClone,
                                              collection_trace, NULL};
static NativeArrayResult collection_size(MCObject *context, MCObject *receiver, int32_t *out) {
    CollectionContext *state = (CollectionContext *)context;
    CHECK(state->arrays == 0);
    ++state->sizes;
    return NativeCollectionTyped_size(context->heap, receiver, NULL, NULL, out);
}
static NativeArrayResult collection_array(MCObject *context, MCObject *receiver,
                                          NativeTypedObjectArray *requested, MCObject **out) {
    CollectionContext *state = (CollectionContext *)context;
    CHECK(state->sizes == 1 && requested->length == 2);
    ++state->arrays;
    if (state->scenario == 0) {
        CHECK(NativeReferenceList_add((NativeReferenceList *)receiver, (MCObject *)state->extra));
        state->originalColors->values[0] = 42;
        return NativeCollectionTyped_toArray(context->heap, receiver, NULL, NULL, requested, out);
    }
    *out = state->returned;
    return NATIVE_ARRAY_OK;
}
static void actual_collection_return_and_cast(void) {
    static const NativeCollectionTypedMethods methods = {collection_size, collection_array};
    for (int scenario = 0; scenario < 4; ++scenario) {
        MCObjectHeap *heap = MCObjectHeap_new(1 << 22);
        Vec4b *icon = Vec4b_new(heap, 1, 2, 3, 4);
        NativeReferenceList *list = icons(heap, icon);
        CollectionContext *context =
            (CollectionContext *)MCObjectHeap_alloc(heap, sizeof(*context), &collectionClass);
        S34PacketMaps *packet = S34PacketMaps_new_empty(heap);
        CHECK(context && packet);
        context->scenario = scenario;
        context->extra = Vec4b_new(heap, 6, 7, 8, 9);
        context->originalColors = NativeByteArray_new(heap, 1);
        CHECK(context->extra && context->originalColors);
        if (scenario == 2) {
            NativeObjectArray *wrong = NativeObjectArray_new(heap, 1);
            CHECK(wrong);
            CHECK(NativeObjectArray_set(wrong, 0, (MCObject *)icon));
            context->returned = (MCObject *)wrong;
        } else if (scenario == 3)
            context->returned = (MCObject *)NativeByteArray_new(heap, 0);
        packet->nativeVisiblePlayersMethods = &methods;
        packet->nativeVisiblePlayersContext = (MCObject *)context;
        packet->mapMinX = 91;
        CHECK(S34PacketMaps_construct(packet, 17, -3, (MCObject *)list, context->originalColors, 0,
                                      0, 1, 1) ==
              (scenario < 2 ? NATIVE_ARRAY_OK : NATIVE_ARRAY_EXCEPTION));
        CHECK(context->sizes == 1 && context->arrays == 1 && packet->mapId == 17 &&
              packet->mapScale == -3);
        if (scenario == 0)
            CHECK(packet->mapVisiblePlayersVec4b->length == 3 &&
                  packet->mapVisiblePlayersVec4b->values[2] == (MCObject *)context->extra &&
                  packet->mapDataBytes->values[0] == 42);
        else if (scenario == 1)
            CHECK(!packet->mapVisiblePlayersVec4b && packet->mapDataBytes && packet->mapMinX == 0);
        else
            CHECK(!packet->mapVisiblePlayersVec4b && !packet->mapDataBytes &&
                  packet->mapMinX == 91);
        CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
    }
    MCObjectHeap *heap = MCObjectHeap_new(sizeof(MapInfo) + sizeof(S34PacketMaps) - 1);
    MapInfo *info = MapInfo_new(heap, NULL, NULL, NULL, NULL);
    S34PacketMaps *unchanged = (S34PacketMaps *)info;
    CHECK(info && info->field_176105_d);
    CHECK(MapInfo_getPacket(info, NULL, &unchanged) == WORLD_SAVED_DATA_FAILURE &&
          unchanged == (S34PacketMaps *)info && !info->field_176105_d && MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}
int main(void) {
    native_field_and_budget_failures();
    constructor_ownership();
    source_exception_prefixes();
    map_info_cadence_and_failure();
    io_prefixes_and_reuse();
    apply_aliases_and_graph();
    handler_and_owner_failure();
    actual_collection_return_and_cast();
    printf("Source S34: %u checks passed\n", checks);
    return 0;
}
