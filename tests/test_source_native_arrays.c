#include "nbt/NBTTagByteArray.h"
#include "nbt/NBTTagCompound.h"
#include "util/NativePrimitiveArray.h"
#include "world/chunk/NibbleArray.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        checks++;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "array check %u failed line %d: %s\n", checks, __LINE__, #x);          \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static const MCObjectClass leaf = {"test.array.leaf", MCObjectHeap_plainClone, NULL, NULL};
static void one_java_byte_array_type(void) {
    CHECK(_Generic((NBTByteArrayStorage *)0, NativeByteArray *: true, default: false));
    MCObjectHeap *heap = MCObjectHeap_new(4096);
    CHECK(heap);
    const int8_t input[] = {INT8_MIN, -1, 0, INT8_MAX};
    size_t count = MCObjectHeap_liveObjects(heap);
    NBTByteArrayStorage *bytes = NBTByteArrayStorage_new(heap, input, 4);
    CHECK(bytes);
    CHECK(MCObjectHeap_liveObjects(heap) == count + 1);
    CHECK(NativeByteArray_isInstance((MCObject *)bytes));
    CHECK(NBTByteArrayStorage_length(bytes) == 4 && NBTByteArrayStorage_data(bytes)[0] == INT8_MIN);
    MCObjectHeap_free(heap);
}
static void shared_nbt_nibble_owners(void) {
    MCObjectHeap *heap = MCObjectHeap_new(1024u * 1024u);
    CHECK(heap);
    NativeByteArray *bytes = NativeByteArray_new(heap, 2048);
    CHECK(bytes);
    NBTByteArrayStorage *storage = bytes; /* Exactly the same C type; no cast/view. */
    NBTTagByteArray *tag = NBTTagByteArray_new(heap, storage);
    NibbleArray *nibbles = NibbleArray_newWithArray(heap, storage);
    NBTTagCompound *compound = NBTTagCompound_new(heap);
    NBTString *key = NBTString_fromASCII(heap, "colors");
    CHECK(tag && nibbles && compound && key);
    CHECK(NBTTagCompound_setByteArray(compound, key, bytes));
    CHECK(NBTTagCompound_getByteArray(compound, key) == bytes &&
          NBTTagByteArray_getByteArray(tag) == bytes && NibbleArray_getData(nibbles) == bytes);
    CHECK(NativeByteArray_set(bytes, 0, -126));
    CHECK(NibbleArray_getFromIndex(nibbles, 0) == 2 && NibbleArray_getFromIndex(nibbles, 1) == 8);
    CHECK(NibbleArray_setIndex(nibbles, 1, 15));
    CHECK(NBTByteArrayStorage_data(storage)[0] == -14);
    CHECK(NBTByteArrayStorage_set(storage, 1, 127));
    int8_t out = 0;
    CHECK(NativeByteArray_get(bytes, 1, &out) && out == 127);
    NativeByteArray *reverse = NBTByteArrayStorage_new(heap, NULL, 2048);
    CHECK(reverse);
    NibbleArray *reverseNibbles = NibbleArray_newWithArray(heap, reverse);
    CHECK(reverseNibbles);
    CHECK(reverse->object.klass == bytes->object.klass &&
          NibbleArray_getData(reverseNibbles) == reverse);
    CHECK(NibbleArray_setIndex(reverseNibbles, 0, 9) && NBTByteArrayStorage_data(reverse)[0] == 9);

    /* Source NBT copy duplicates every edge; graph snapshots preserve aliases. */
    NBTTagList *list = NBTTagList_new(heap);
    CHECK(list);
    CHECK(NBTTagList_appendTag(list, (NBTBase *)tag) && NBTTagList_appendTag(list, (NBTBase *)tag));
    NBTTagList *sourceCopy = (NBTTagList *)NBTBase_copy(heap, (NBTBase *)list);
    CHECK(sourceCopy);
    NativeByteArray *copyA =
        NBTTagByteArray_getByteArray((NBTTagByteArray *)NBTTagList_get(sourceCopy, 0));
    NativeByteArray *copyB =
        NBTTagByteArray_getByteArray((NBTTagByteArray *)NBTTagList_get(sourceCopy, 1));
    CHECK(copyA && copyB && copyA != copyB && copyA != bytes && copyB != bytes);
    CHECK(NBTBase_equals((NBTBase *)sourceCopy, (NBTBase *)list) &&
          NBTBase_hashCode((NBTBase *)sourceCopy) == NBTBase_hashCode((NBTBase *)list));
    CHECK(NativeByteArray_set(copyA, 0, 1) && bytes->values[0] == -14 && copyB->values[0] == -14);
    mc_buf wire;
    mc_buf_init(&wire);
    CHECK(NBTBase_write((NBTBase *)tag, &wire));
    CHECK(wire.len == 2052 && wire.data[0] == 0 && wire.data[1] == 0 && wire.data[2] == 8 &&
          wire.data[3] == 0 && wire.data[4] == 242 && wire.data[5] == 127);
    NBTTagByteArray *decoded = NBTTagByteArray_new(heap, NULL);
    CHECK(decoded);
    NBTSizeTracker tracker;
    NBTSizeTracker_initInfinite(&tracker);
    CHECK(NBTBase_read((NBTBase *)decoded, &wire, 0, &tracker));
    NativeByteArray *decodedBytes = NBTTagByteArray_getByteArray(decoded);
    CHECK(decodedBytes && decodedBytes != bytes &&
          NativeByteArray_isInstance((MCObject *)decodedBytes));
    CHECK(NBTBase_equals((NBTBase *)tag, (NBTBase *)decoded) &&
          NBTBase_hashCode((NBTBase *)tag) == NBTBase_hashCode((NBTBase *)decoded));
    mc_buf_free(&wire);

    NativeObjectArray *owners = NativeObjectArray_new(heap, 5);
    CHECK(owners);
    CHECK(NativeObjectArray_set(owners, 0, (MCObject *)tag) &&
          NativeObjectArray_set(owners, 1, (MCObject *)compound) &&
          NativeObjectArray_set(owners, 2, (MCObject *)nibbles) &&
          NativeObjectArray_set(owners, 3, (MCObject *)bytes) &&
          NativeObjectArray_set(owners, 4, (MCObject *)owners));
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, heap, (MCObject *)owners));
    CHECK(MCObjectHeap_collect(heap));
    MCObjectHeap *branch = MCObjectHeap_clone(heap);
    CHECK(branch);
    MCObjectRoot branchRoot = {0};
    CHECK(MCObjectRoot_rebind(&branchRoot, branch, &root));
    NativeObjectArray *cloned = (NativeObjectArray *)MCObjectRoot_get(&branchRoot);
    CHECK(cloned);
    NativeByteArray *clonedBytes = (NativeByteArray *)cloned->values[3];
    CHECK(clonedBytes != bytes && cloned->values[4] == (MCObject *)cloned &&
          NBTTagByteArray_getByteArray((NBTTagByteArray *)cloned->values[0]) == clonedBytes &&
          NibbleArray_getData((NibbleArray *)cloned->values[2]) == clonedBytes);
    NBTTagByteArray *compoundTag = (NBTTagByteArray *)NBTTagCompound_getTag_ascii(
        (NBTTagCompound *)cloned->values[1], "colors");
    CHECK(compoundTag && NBTTagByteArray_getByteArray(compoundTag) == clonedBytes);
    CHECK(NBTByteArrayStorage_set(clonedBytes, 1, -128) && bytes->values[1] == 127);
    CHECK(MCObjectHeap_adopt(heap, branch));
    owners = (NativeObjectArray *)MCObjectRoot_get(&root);
    bytes = (NativeByteArray *)owners->values[3];
    CHECK(bytes->values[1] == -128 &&
          NBTTagByteArray_getByteArray((NBTTagByteArray *)owners->values[0]) == bytes &&
          NibbleArray_getData((NibbleArray *)owners->values[2]) == bytes);
    CHECK(MCObjectHeap_collect(heap) && !MCObjectHeap_failed(heap));
    MCObjectRoot_drop(&root);
    CHECK(MCObjectHeap_collect(heap) && MCObjectHeap_liveObjects(heap) == 0);
    MCObjectHeap_free(branch);
    MCObjectHeap_free(heap);
}
static void byte_array_nullable_and_failure_contracts(void) {
    MCObjectHeap *heap = MCObjectHeap_new(65536);
    CHECK(heap);
    CHECK(NBTByteArrayStorage_length(NULL) == 0 && NBTByteArrayStorage_data(NULL) == NULL &&
          !NBTByteArrayStorage_set(NULL, 0, 1) && !MCObjectHeap_failed(heap));
    NBTTagByteArray *nullTag = NBTTagByteArray_new(heap, NULL),
                    *anotherNull = NBTTagByteArray_new(heap, NULL);
    NativeByteArray *empty = NativeByteArray_new(heap, 0);
    NBTTagByteArray *emptyTag = NBTTagByteArray_new(heap, empty);
    CHECK(nullTag && anotherNull && emptyTag);
    CHECK(NBTTagByteArray_getByteArray(nullTag) == NULL &&
          NBTBase_equals((NBTBase *)nullTag, (NBTBase *)anotherNull));
    CHECK(!NBTBase_equals((NBTBase *)nullTag, (NBTBase *)emptyTag) &&
          NBTBase_hashCode((NBTBase *)nullTag) == 7 && NBTBase_hashCode((NBTBase *)emptyTag) == 6);
    NBTTagCompound *compound = NBTTagCompound_new(heap);
    NBTString *key = NBTString_fromASCII(heap, "missing");
    CHECK(compound && key);
    NativeByteArray *a = NBTTagCompound_getByteArray(compound, key),
                    *b = NBTTagCompound_getByteArray(compound, key);
    CHECK(a && b && a != b && a->length == 0 && b->length == 0 &&
          a->object.klass == b->object.klass);
    CHECK(NBTTagCompound_setByteArray(compound, key, NULL) &&
          NBTTagCompound_getByteArray(compound, key) == NULL);
    mc_buf wire;
    mc_buf_init(&wire);
    CHECK(!NBTBase_write((NBTBase *)nullTag, &wire) && wire.failed);
    mc_buf_free(&wire);
    CHECK(!MCObjectHeap_failed(heap));
    CHECK(NBTBase_copy(heap, (NBTBase *)nullTag) == NULL && MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
    heap = MCObjectHeap_new(4096);
    CHECK(heap);
    a = NBTByteArrayStorage_new(heap, NULL, 1);
    CHECK(a);
    MCObjectHeap_fail(heap);
    CHECK(!NativeByteArray_set(a, 0, 3) && a->values[0] == 0);
    /* Keep the legacy native NBT convenience wrapper's failed-heap policy. */
    CHECK(NBTByteArrayStorage_set(a, 0, -1) && a->values[0] == -1 && MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
    heap = MCObjectHeap_new(4096);
    CHECK(heap);
    CHECK(!NBTByteArrayStorage_new(heap, NULL, -1) && MCObjectHeap_failed(heap) &&
          MCObjectHeap_liveObjects(heap) == 0);
    MCObjectHeap_free(heap);
    heap = MCObjectHeap_new(sizeof(NativeByteArray));
    CHECK(heap);
    CHECK(!NBTByteArrayStorage_new(heap, NULL, 1) && MCObjectHeap_failed(heap) &&
          MCObjectHeap_liveObjects(heap) == 0);
    MCObjectHeap_free(heap);
}
static void values_and_aliases(void) {
    MCObjectHeap *heap = MCObjectHeap_new(65536);
    CHECK(heap);
    NativeByteArray *bytes = NativeByteArray_new(heap, 2);
    CHECK(bytes);
    NativeCharArray *chars = NativeCharArray_new(heap, 2);
    CHECK(chars);
    NativeShortArray *shorts = NativeShortArray_new(heap, 2);
    CHECK(shorts);
    NativeBooleanArray *flags = NativeBooleanArray_new(heap, 2);
    CHECK(flags);
    NativeObjectArray *refs = NativeObjectArray_new(heap, 4);
    CHECK(refs);
    int8_t b = 12;
    uint16_t c = 12;
    int16_t s = 12;
    bool f = true;
    CHECK(NativeByteArray_get(bytes, 0, &b) && b == 0);
    CHECK(NativeCharArray_get(chars, 0, &c) && c == 0);
    CHECK(NativeShortArray_get(shorts, 0, &s) && s == 0);
    CHECK(NativeBooleanArray_get(flags, 0, &f) && !f);
    CHECK(NativeByteArray_set(bytes, 1, INT8_MIN));
    CHECK(NativeCharArray_set(chars, 1, UINT16_MAX));
    CHECK(NativeShortArray_set(shorts, 1, INT16_MIN));
    CHECK(NativeBooleanArray_set(flags, 1, true));
    CHECK(NativeByteArray_get(bytes, 1, &b) && b == -128);
    CHECK(NativeCharArray_get(chars, 1, &c) && c == 65535);
    CHECK(NativeShortArray_get(shorts, 1, &s) && s == -32768);
    CHECK(NativeBooleanArray_get(flags, 1, &f) && f);
    CHECK(NativeObjectArray_set(refs, 0, (MCObject *)bytes));
    CHECK(NativeObjectArray_set(refs, 1, (MCObject *)bytes));
    CHECK(NativeObjectArray_set(refs, 2, (MCObject *)refs));
    CHECK(NativeObjectArray_set(refs, 3, (MCObject *)chars));
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, heap, (MCObject *)refs));
    CHECK(MCObjectHeap_collect(heap));
    CHECK(MCObjectHeap_liveObjects(heap) == 3);
    MCObjectHeap *copy = MCObjectHeap_clone(heap);
    CHECK(copy);
    MCObjectRoot branch = {0};
    CHECK(MCObjectRoot_rebind(&branch, copy, &root));
    NativeObjectArray *cloned = (NativeObjectArray *)MCObjectRoot_get(&branch);
    CHECK(cloned && cloned != refs);
    CHECK(cloned->values[0] == cloned->values[1] && cloned->values[0] != (MCObject *)bytes);
    CHECK(cloned->values[2] == (MCObject *)cloned);
    CHECK(((NativeByteArray *)cloned->values[0])->values[1] == -128);
    CHECK(NativeByteArray_set((NativeByteArray *)cloned->values[0], 1, 127));
    CHECK(bytes->values[1] == -128);
    CHECK(MCObjectHeap_canAdopt(heap, copy));
    CHECK(MCObjectHeap_adopt(heap, copy));
    refs = (NativeObjectArray *)MCObjectRoot_get(&root);
    CHECK(refs->values[0] == refs->values[1] && refs->values[2] == (MCObject *)refs);
    CHECK(((NativeByteArray *)refs->values[0])->values[1] == 127);
    CHECK(MCObjectHeap_collect(heap) && MCObjectHeap_liveObjects(heap) == 3);
    MCObjectRoot_drop(&root);
    CHECK(MCObjectHeap_collect(heap) && MCObjectHeap_liveObjects(heap) == 0);
    MCObjectHeap_free(copy);
    MCObjectHeap_free(heap);
}
static void errors(void) {
    MCObjectHeap *heap = MCObjectHeap_new(4096);
    CHECK(heap);
    NativeByteArray *bytes = NativeByteArray_new(heap, 1);
    CHECK(bytes);
    int8_t out = 42;
    CHECK(!NativeByteArray_get(bytes, -1, &out));
    CHECK(out == 42 && MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
    heap = MCObjectHeap_new(4096);
    CHECK(heap);
    bytes = NativeByteArray_new(heap, 1);
    CHECK(bytes);
    bytes->length = 2;
    CHECK(!NativeByteArray_isInstance((MCObject *)bytes));
    out = 42;
    CHECK(!NativeByteArray_get(bytes, 1, &out));
    CHECK(out == 42 && MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
    heap = MCObjectHeap_new(4096);
    MCObjectHeap *foreign = MCObjectHeap_new(4096);
    CHECK(heap && foreign);
    NativeObjectArray *refs = NativeObjectArray_new(heap, 1);
    MCObject *obj = MCObjectHeap_alloc(foreign, sizeof(MCObject), &leaf);
    CHECK(refs && obj);
    CHECK(!NativeObjectArray_set(refs, 0, obj));
    CHECK(refs->values[0] == NULL && MCObjectHeap_failed(heap) && !MCObjectHeap_failed(foreign));
    MCObjectHeap_free(heap);
    MCObjectHeap_free(foreign);
    heap = MCObjectHeap_new(4096);
    CHECK(!NativeShortArray_new(heap, -1) && MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
    heap = MCObjectHeap_new(sizeof(NativeCharArray));
    CHECK(!NativeCharArray_new(heap, 1) && MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}
int main(void) {
    one_java_byte_array_type();
    shared_nbt_nibble_owners();
    byte_array_nullable_and_failure_contracts();
    values_and_aliases();
    errors();
    printf("source native arrays: %u checks passed\n", checks);
    return 0;
}
