#include "world/chunk/NibbleArray.h"
static const MCObjectClass klass;

static bool fail(MCObjectHeap *h) {
    MCObjectHeap_fail(h);
    return false;
}
bool NibbleArray_isInstance(const MCObject *o) {
    return o && o->klass == &klass && MCObjectHeap_objectSize(o) >= sizeof(NibbleArray);
}
static bool valid(NibbleArray *n) {
    return (NibbleArray_isInstance((MCObject *)n) && !MCObjectHeap_failed(n->object.heap)) ||
           fail(n ? n->object.heap : NULL);
}
static bool array_valid(NibbleArray *n, NativeByteArray *a) {
    return (NativeByteArray_isInstance((MCObject *)a) && a->object.heap == n->object.heap) ||
           fail(n->object.heap);
}
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
    if (!NibbleArray_isInstance(o)) {
        fail(o->heap);
        return;
    }
    NibbleArray *n = (NibbleArray *)o;
    n->data = (NativeByteArray *)v((MCObject *)n->data, c);
}
static const MCObjectClass klass = {"NibbleArray", MCObjectHeap_plainClone, trace, NULL};

NibbleArray *NibbleArray_nativeAllocate(MCObjectHeap *h) {
    return (NibbleArray *)MCObjectHeap_alloc(h, sizeof(NibbleArray), &klass);
}
bool NibbleArray_construct(NibbleArray *n) {
    if (!valid(n))
        return false;

    MCObjectRootScope scope = {0};

    if (!MCObjectRootScope_begin(&scope, n->object.heap) ||
        !MCObjectRootScope_pin(&scope, (MCObject *)n))
        return false;

    NativeByteArray *a = NativeByteArray_new(n->object.heap, 2048);

    if (a) {
        n->data = a;
        MCObjectHeap_touch(n->object.heap);
    }
    MCObjectRootScope_end(&scope);
    return a != NULL;
}
bool NibbleArray_constructWithArray(NibbleArray *n, NativeByteArray *a) {
    if (!valid(n))
        return false;

    if (a && (!NativeByteArray_isInstance((MCObject *)a) || a->object.heap != n->object.heap))
        return fail(n->object.heap);

    n->data = a;
    MCObjectHeap_touch(n->object.heap);

    return (array_valid(n, a) && a->length == 2048) || fail(n->object.heap);
}
NibbleArray *NibbleArray_new(MCObjectHeap *h) {
    NibbleArray *n = NibbleArray_nativeAllocate(h);
    return n && NibbleArray_construct(n) ? n : NULL;
}
NibbleArray *NibbleArray_newWithArray(MCObjectHeap *h, NativeByteArray *a) {
    NibbleArray *n = NibbleArray_nativeAllocate(h);
    return n && NibbleArray_constructWithArray(n, a) ? n : NULL;
}
static int32_t signed32(uint32_t v) {
    return v <= INT32_MAX ? (int32_t)v : (int32_t)((int64_t)v - INT64_C(4294967296));
}
static int32_t coordinates(int32_t x, int32_t y, int32_t z) {
    return signed32(((uint32_t)y << 8) | ((uint32_t)z << 4) | (uint32_t)x);
}
static int32_t nibble_index(int32_t i) { return i >= 0 ? i / 2 : (int32_t)(((int64_t)i - 1) / 2); }
int32_t NibbleArray_getFromIndex(NibbleArray *n, int32_t index) {
    if (!valid(n) || !array_valid(n, n->data))
        return 0;

    int8_t byte = 0;

    if (!NativeByteArray_get(n->data, nibble_index(index), &byte))
        return 0;

    return (index & 1) == 0 ? ((int32_t)byte & 15) : (((int32_t)(uint8_t)byte >> 4) & 15);
}
bool NibbleArray_setIndex(NibbleArray *n, int32_t index, int32_t value) {
    if (!valid(n) || !array_valid(n, n->data))
        return false;

    int8_t old = 0;
    int32_t i = nibble_index(index);

    if (!NativeByteArray_get(n->data, i, &old))
        return false;

    uint8_t v = (index & 1) == 0 ? (((uint8_t)old & 240) | ((uint32_t)value & 15))
                                 : (((uint8_t)old & 15) | (((uint32_t)value & 15) << 4));

    int8_t narrowed = (int8_t)(v < 128 ? (int32_t)v : (int32_t)v - 256);
    return NativeByteArray_set(n->data, i, narrowed);
}
int32_t NibbleArray_get(NibbleArray *n, int32_t x, int32_t y, int32_t z) {
    return NibbleArray_getFromIndex(n, coordinates(x, y, z));
}
bool NibbleArray_set(NibbleArray *n, int32_t x, int32_t y, int32_t z, int32_t value) {
    return NibbleArray_setIndex(n, coordinates(x, y, z), value);
}
NativeByteArray *NibbleArray_getData(NibbleArray *n) {
    if (!valid(n))
        return NULL;

    if (n->data && !array_valid(n, n->data))
        return NULL;
    return n->data;
}
