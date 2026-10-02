#include "util/BlockPosMutableBlockPos.h"
#include "util/MathHelper.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(value)                                                                                \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "Source BlockPos check %u line %d: %s\n", checks, __LINE__, #value);     \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static void coordinates(Vec3i *self, int32_t x, int32_t y, int32_t z) {
    int32_t value = 99;
    CHECK(Vec3i_getX(self, &value) == NATIVE_ARRAY_OK && value == x);
    CHECK(Vec3i_getY(self, &value) == NATIVE_ARRAY_OK && value == y);
    CHECK(Vec3i_getZ(self, &value) == NATIVE_ARRAY_OK && value == z);
}
static int32_t signed32(uint32_t bits) {
    return bits <= INT32_MAX ? (int32_t)bits : -1 - (int32_t)(UINT32_MAX - bits);
}
static int32_t truncated_coordinate(int32_t value, int32_t modulus) {
    /* Independent signed remainder description of the packed coordinate. */
    int64_t result = (int64_t)value % modulus;
    if (result < -modulus / 2)
        result += modulus;
    if (result >= modulus / 2)
        result -= modulus;
    return (int32_t)result;
}
static void constructors(void) {
    MCObjectHeap *heap = MCObjectHeap_new(1u << 20);
    CHECK(heap);
    static const int32_t integers[] = {0, 1, -1, INT32_MIN, INT32_MAX, -33554432,
                                       33554431, -2048, 2047};
    for (size_t i = 0; i < sizeof integers / sizeof *integers; ++i) {
        for (size_t j = 0; j < sizeof integers / sizeof *integers; ++j) {
            int32_t x = integers[i], y = integers[j], z = integers[(i + j) % 9];
            size_t before = MCObjectHeap_liveObjects(heap);
            Vec3i *base = Vec3i_newInt(heap, x, y, z);
            BlockPos *pos = BlockPos_newInt(heap, x, y, z);
            BlockPosMutableBlockPos *mutable = BlockPosMutableBlockPos_newInt(heap, x, y, z);
            CHECK(base && pos && mutable);
            CHECK(MCObjectHeap_liveObjects(heap) == before + 3);
            CHECK(base->x == x && base->y == y && base->z == z);
            CHECK(pos->vec3i.x == x && pos->vec3i.y == y && pos->vec3i.z == z);
            CHECK(mutable->blockPos.vec3i.x == 0 && mutable->blockPos.vec3i.y == 0 &&
                  mutable->blockPos.vec3i.z == 0);
            CHECK(mutable->x == x && mutable->y == y && mutable->z == z);
            coordinates(base, x, y, z);
            coordinates(&pos->vec3i, x, y, z);
            coordinates(&mutable->blockPos.vec3i, x, y, z);
            int32_t direct = 99;
            CHECK(BlockPosMutableBlockPos_getX(mutable, &direct) == NATIVE_ARRAY_OK && direct == x);
            CHECK(BlockPosMutableBlockPos_getY(mutable, &direct) == NATIVE_ARRAY_OK && direct == y);
            CHECK(BlockPosMutableBlockPos_getZ(mutable, &direct) == NATIVE_ARRAY_OK && direct == z);
            CHECK((void *)mutable == (void *)&mutable->blockPos &&
                  (void *)mutable == (void *)&mutable->blockPos.vec3i &&
                  (void *)mutable == (void *)&mutable->blockPos.vec3i.object);
            CHECK(Vec3i_isInstance((MCObject *)base) && !BlockPos_isInstance((MCObject *)base));
            CHECK(Vec3i_isInstance((MCObject *)pos) && BlockPos_isInstance((MCObject *)pos));
            CHECK(Vec3i_isInstance((MCObject *)mutable) && BlockPos_isInstance((MCObject *)mutable) &&
                  BlockPosMutableBlockPos_isInstance((MCObject *)mutable));
            CHECK(Vec3i_isRuntimeClass((MCObject *)base) && !Vec3i_isRuntimeClass((MCObject *)pos) &&
                  !Vec3i_isRuntimeClass((MCObject *)mutable));
            CHECK(BlockPos_isRuntimeClass((MCObject *)pos) && !BlockPos_isRuntimeClass((MCObject *)mutable));
            CHECK(BlockPosMutableBlockPos_isRuntimeClass((MCObject *)mutable) &&
                  !BlockPosMutableBlockPos_isRuntimeClass((MCObject *)pos));
            CHECK(Vec3i_Class.matchesRuntimeClass((MCObject *)base) &&
                  !Vec3i_Class.matchesRuntimeClass((MCObject *)mutable));
            CHECK(BlockPos_Class.matchesRuntimeClass((MCObject *)pos) &&
                  !BlockPos_Class.matchesRuntimeClass((MCObject *)mutable));
            CHECK(BlockPosMutableBlockPos_Class.matchesRuntimeClass((MCObject *)mutable));
        }
    }
    CHECK(Vec3i_Class.supertypeCount == 1 && Vec3i_Class.supertypes[0] == &NativeJavaClass_ObjectClass);
    CHECK(BlockPos_Class.supertypeCount == 1 && BlockPos_Class.supertypes[0] == &Vec3i_Class);
    CHECK(BlockPosMutableBlockPos_Class.supertypeCount == 1 &&
          BlockPosMutableBlockPos_Class.supertypes[0] == &BlockPos_Class);
    CHECK(Vec3i_nativeClass() != BlockPos_nativeClass() &&
          BlockPos_nativeClass() != BlockPosMutableBlockPos_nativeClass());
    BlockPosMutableBlockPos *empty = BlockPosMutableBlockPos_newEmpty(heap);
    CHECK(empty && empty->x == 0 && empty->y == 0 && empty->z == 0);
    Vec3i *raw = Vec3i_nativeAllocate(heap);
    BlockPos *rawPos = NativeBlockPos_allocate(heap);
    BlockPosMutableBlockPos *rawMutable = BlockPosMutableBlockPos_nativeAllocate(heap);
    CHECK(raw && rawPos && rawMutable);
    CHECK(Vec3i_constructInt(raw, 11, 12, 13) == NATIVE_ARRAY_OK);
    CHECK(NativeBlockPos_constructCoordinates(rawPos, 14, 15, 16));
    CHECK(BlockPosMutableBlockPos_constructEmpty(rawMutable) == NATIVE_ARRAY_OK);
    coordinates(raw, 11, 12, 13);
    coordinates(&rawPos->vec3i, 14, 15, 16);
    coordinates(&rawMutable->blockPos.vec3i, 0, 0, 0);
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}
static void double_constructors(void) {
    static const struct { double value; int32_t expected; } cases[] = {
        {0.0, 0}, {-0.0, 0}, {0.25, 0}, {-0.25, -1}, {1.0, 1}, {-1.0, -1},
        {-1.25, -2}, {2147483647.0, INT32_MAX}, {2147483648.0, INT32_MAX},
        {-2147483648.0, INT32_MIN}, {-2147483649.0, INT32_MAX},
        {INFINITY, INT32_MAX}, {-INFINITY, INT32_MAX}, {NAN, 0}
    };
    MCObjectHeap *heap = MCObjectHeap_new(65536);
    CHECK(heap);
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        size_t j = (i + 3) % 14, k = (i + 8) % 14;
        Vec3i *base = Vec3i_newDouble(heap, cases[i].value, cases[j].value, cases[k].value);
        BlockPos *pos = BlockPos_newDouble(heap, cases[k].value, cases[j].value, cases[i].value);
        CHECK(base && pos);
        coordinates(base, cases[i].expected, cases[j].expected, cases[k].expected);
        coordinates(&pos->vec3i, cases[k].expected, cases[j].expected, cases[i].expected);
        CHECK(Vec3i_constructDouble(base, cases[k].value, cases[i].value, cases[j].value) == NATIVE_ARRAY_OK);
        CHECK(BlockPos_constructDouble(pos, cases[j].value, cases[k].value, cases[i].value) == NATIVE_ARRAY_OK);
        coordinates(base, cases[k].expected, cases[i].expected, cases[j].expected);
        coordinates(&pos->vec3i, cases[j].expected, cases[k].expected, cases[i].expected);
    }
    MCObjectHeap_free(heap);
}
static void mutable_aliases(void) {
    MCObjectHeap *heap = MCObjectHeap_new(65536);
    CHECK(heap);
    BlockPosMutableBlockPos *self = BlockPosMutableBlockPos_newInt(heap, 1, 2, 3);
    CHECK(self);
    CHECK(self->blockPos.vec3i.x == 0 && self->blockPos.vec3i.y == 0 && self->blockPos.vec3i.z == 0);
    CHECK(self->x == 1 && self->y == 2 && self->z == 3);
    BlockPos *alias = &self->blockPos;
    Vec3i *baseAlias = &alias->vec3i;
    size_t before = MCObjectHeap_liveObjects(heap);
    BlockPosMutableBlockPos *out = NULL;
    CHECK(BlockPosMutableBlockPos_set(self, INT32_MAX, INT32_MIN, -4, &out) == NATIVE_ARRAY_OK && out == self);
    CHECK(MCObjectHeap_liveObjects(heap) == before);
    coordinates(baseAlias, INT32_MAX, INT32_MIN, -4);
    CHECK(BlockPos_add(alias, 0, 0, 0) == alias && BlockPos_downN(alias, 0) == alias);
    CHECK(MCObjectHeap_liveObjects(heap) == before);
    BlockPos *sum = BlockPos_add(alias, 1, -1, 5);
    CHECK(sum && sum != alias && BlockPos_isRuntimeClass((MCObject *)sum));
    coordinates(&sum->vec3i, INT32_MIN, INT32_MAX, 1);
    BlockPos *down = BlockPos_down(alias);
    CHECK(down && BlockPos_isRuntimeClass((MCObject *)down));
    coordinates(&down->vec3i, INT32_MAX, INT32_MAX, -4);
    down = BlockPos_downN(alias, INT32_MIN);
    CHECK(down);
    coordinates(&down->vec3i, INT32_MAX, 0, -4);
    CHECK(BlockPosMutableBlockPos_set(self, 4, 5, 6, &out) == NATIVE_ARRAY_OK && out == self);
    coordinates(baseAlias, 4, 5, 6);
    coordinates(&sum->vec3i, INT32_MIN, INT32_MAX, 1);
    CHECK(self->blockPos.vec3i.x == 0 && self->blockPos.vec3i.y == 0 && self->blockPos.vec3i.z == 0);
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}
static void arithmetic(void) {
    static const int32_t values[] = {INT32_MIN, INT32_MAX, -1, 0, 1, -33554432, 33554431};
    MCObjectHeap *heap = MCObjectHeap_new(1u << 20);
    CHECK(heap);
    BlockPosMutableBlockPos *mutable = BlockPosMutableBlockPos_newEmpty(heap), *out = NULL;
    CHECK(mutable);
    for (size_t i = 0; i < sizeof values / sizeof *values; ++i) {
        for (size_t j = 0; j < sizeof values / sizeof *values; ++j) {
            int32_t x = values[i], y = values[j], z = values[(i + j) % 7];
            CHECK(BlockPosMutableBlockPos_set(mutable, x, y, z, &out) == NATIVE_ARRAY_OK && out == mutable);
            BlockPos *same = BlockPos_newInt(heap, x, y, z);
            Vec3i *plain = Vec3i_newInt(heap, x, y, z);
            CHECK(same && plain);
            bool equal = false;
            CHECK(Vec3i_equals(&mutable->blockPos.vec3i, (MCObject *)same, &equal) == NATIVE_ARRAY_OK && equal);
            CHECK(Vec3i_equals(&same->vec3i, (MCObject *)mutable, &equal) == NATIVE_ARRAY_OK && equal);
            CHECK(Vec3i_equals(plain, (MCObject *)mutable, &equal) == NATIVE_ARRAY_OK && equal);
            CHECK(Vec3i_equals(&mutable->blockPos.vec3i, (MCObject *)mutable, &equal) == NATIVE_ARRAY_OK && equal);
            int32_t hash = 0, otherHash = 0;
            CHECK(Vec3i_hashCode(&mutable->blockPos.vec3i, &hash) == NATIVE_ARRAY_OK);
            CHECK(Vec3i_hashCode(&same->vec3i, &otherHash) == NATIVE_ARRAY_OK && otherHash == hash);
            uint32_t expected = (uint32_t)x + (uint32_t)y * 31u + (uint32_t)z * 961u;
            CHECK(hash == signed32(expected));
            for (unsigned axis = 0; axis < 3; ++axis) {
                int32_t cx = axis == 0 ? signed32((uint32_t)x + 1u) : x;
                int32_t cy = axis == 1 ? signed32((uint32_t)y + 1u) : y;
                int32_t cz = axis == 2 ? signed32((uint32_t)z + 1u) : z;
                BlockPos *different = BlockPos_newInt(heap, cx, cy, cz);
                CHECK(different);
                equal = true;
                CHECK(Vec3i_equals(&mutable->blockPos.vec3i, (MCObject *)different, &equal) == NATIVE_ARRAY_OK && !equal);
            }
            BlockPos *added = BlockPos_add(&mutable->blockPos, values[j], values[i], values[(i + 2 * j) % 7]);
            CHECK(added);
            coordinates(&added->vec3i, signed32((uint32_t)x + (uint32_t)values[j]),
                        signed32((uint32_t)y + (uint32_t)values[i]),
                        signed32((uint32_t)z + (uint32_t)values[(i + 2 * j) % 7]));
        }
    }
    MCObjectHeap_free(heap);
}
static void packed(void) {
    /* Numeric vectors derive from supplied Source's 26/12/26 expression, not
       a claim of newly executed original-JVM observations. */
    static const struct { int32_t x, y, z; uint64_t bits; } cases[] = {
        {0, 0, 0, UINT64_C(0)}, {1, 2, 3, UINT64_C(0x4008000003)},
        {-1, -1, -1, UINT64_MAX}, {-1, 0, 0, UINT64_C(0xffffffc000000000)},
        {0, -1, 0, UINT64_C(0x3ffc000000)}, {0, 0, -1, UINT64_C(0x3ffffff)},
        {33554431, 2047, 33554431, UINT64_C(0x7fffffdffdffffff)},
        {-33554432, -2048, -33554432, UINT64_C(0x8000002002000000)},
        {INT32_MIN, INT32_MIN, INT32_MIN, UINT64_C(0)},
        {INT32_MAX, INT32_MAX, INT32_MAX, UINT64_MAX}
    };
    MCObjectHeap *heap = MCObjectHeap_new(1u << 20);
    CHECK(heap);
    BlockPosMutableBlockPos *mutable = BlockPosMutableBlockPos_newEmpty(heap), *out = NULL;
    CHECK(mutable);
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, heap, (MCObject *)mutable));
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        CHECK(BlockPosMutableBlockPos_set(mutable, cases[i].x, cases[i].y, cases[i].z, &out) == NATIVE_ARRAY_OK);
        int64_t value = 123;
        CHECK(BlockPos_toLong(&mutable->blockPos, &value) == NATIVE_ARRAY_OK && (uint64_t)value == cases[i].bits);
        BlockPos *decoded = BlockPos_fromLong(heap, value);
        CHECK(decoded && BlockPos_isRuntimeClass((MCObject *)decoded));
        coordinates(&decoded->vec3i, truncated_coordinate(cases[i].x, 67108864),
                    truncated_coordinate(cases[i].y, 4096), truncated_coordinate(cases[i].z, 67108864));
    }
    uint64_t random = UINT64_C(0x83756a07119237af);
    for (unsigned i = 0; i < 8192; ++i) {
        random = random * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
        int64_t input;
        memcpy(&input, &random, sizeof(input));
        BlockPos *decoded = BlockPos_fromLong(heap, input);
        CHECK(decoded);
        int64_t output = 0;
        CHECK(BlockPos_toLong(decoded, &output) == NATIVE_ARRAY_OK && output == input);
        if (i % 256 == 255) {
            CHECK(MCObjectHeap_collect(heap) && MCObjectRoot_get(&root) == (MCObject *)mutable);
            CHECK(MCObjectHeap_liveObjects(heap) == 1);
        }
    }
    MCObjectRoot_drop(&root);
    CHECK(MCObjectHeap_collect(heap) && MCObjectHeap_liveObjects(heap) == 0);
    MCObjectHeap_free(heap);
}
static const MCObjectClass ordinaryClass = {"fixture.BlockPos.Object", MCObjectHeap_plainClone, NULL, NULL};
static void exceptions_and_shapes(void) {
    MCObjectHeap *heap = MCObjectHeap_new(65536);
    CHECK(heap);
    int32_t value = 47;
    int64_t longValue = 48;
    bool equal = true;
    BlockPosMutableBlockPos *out = NULL;
    CHECK(Vec3i_constructInt(NULL, 1, 2, 3) == NATIVE_ARRAY_EXCEPTION);
    CHECK(Vec3i_constructDouble(NULL, NAN, INFINITY, -INFINITY) == NATIVE_ARRAY_EXCEPTION);
    CHECK(BlockPos_constructInt(NULL, 1, 2, 3) == NATIVE_ARRAY_EXCEPTION);
    CHECK(BlockPos_constructDouble(NULL, 1, 2, 3) == NATIVE_ARRAY_EXCEPTION);
    CHECK(BlockPosMutableBlockPos_constructInt(NULL, 1, 2, 3) == NATIVE_ARRAY_EXCEPTION);
    CHECK(BlockPosMutableBlockPos_constructEmpty(NULL) == NATIVE_ARRAY_EXCEPTION);
    CHECK(Vec3i_getX(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value == 47);
    CHECK(Vec3i_getY(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value == 47);
    CHECK(Vec3i_getZ(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value == 47);
    CHECK(BlockPosMutableBlockPos_getX(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value == 47);
    CHECK(BlockPosMutableBlockPos_getY(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value == 47);
    CHECK(BlockPosMutableBlockPos_getZ(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value == 47);
    CHECK(Vec3i_equals(NULL, NULL, &equal) == NATIVE_ARRAY_EXCEPTION && equal);
    CHECK(Vec3i_hashCode(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value == 47);
    CHECK(BlockPos_toLong(NULL, &longValue) == NATIVE_ARRAY_EXCEPTION && longValue == 48);
    CHECK(BlockPosMutableBlockPos_set(NULL, 1, 2, 3, &out) == NATIVE_ARRAY_EXCEPTION && out == NULL);
    Vec3i *base = Vec3i_newInt(heap, 1, 2, 3);
    MCObject *ordinary = MCObjectHeap_alloc(heap, sizeof(MCObject), &ordinaryClass);
    CHECK(base && ordinary);
    CHECK(Vec3i_equals(base, NULL, &equal) == NATIVE_ARRAY_OK && !equal);
    CHECK(Vec3i_equals(base, ordinary, &equal) == NATIVE_ARRAY_OK && !equal);
    CHECK(!Vec3i_isInstance(ordinary) && !BlockPos_isInstance(ordinary));
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
    for (unsigned method = 0; method < 6; ++method) {
        heap = MCObjectHeap_new(65536);
        CHECK(heap);
        MCObject *shortMutable = MCObjectHeap_alloc(heap, sizeof(MCObject), BlockPosMutableBlockPos_nativeClass());
        CHECK(shortMutable);
        value = 47;
        BlockPosMutableBlockPos *sameOut = (BlockPosMutableBlockPos *)shortMutable;
        if (method == 0) CHECK(BlockPosMutableBlockPos_getX((BlockPosMutableBlockPos *)shortMutable, &value) == NATIVE_ARRAY_FAILURE && value == 47);
        else if (method == 1) CHECK(BlockPosMutableBlockPos_getY((BlockPosMutableBlockPos *)shortMutable, &value) == NATIVE_ARRAY_FAILURE && value == 47);
        else if (method == 2) CHECK(BlockPosMutableBlockPos_getZ((BlockPosMutableBlockPos *)shortMutable, &value) == NATIVE_ARRAY_FAILURE && value == 47);
        else if (method == 3) CHECK(BlockPosMutableBlockPos_set((BlockPosMutableBlockPos *)shortMutable, 1, 2, 3, &sameOut) == NATIVE_ARRAY_FAILURE && sameOut == (BlockPosMutableBlockPos *)shortMutable);
        else if (method == 4) CHECK(BlockPosMutableBlockPos_constructInt((BlockPosMutableBlockPos *)shortMutable, 1, 2, 3) == NATIVE_ARRAY_FAILURE);
        else CHECK(BlockPosMutableBlockPos_constructEmpty((BlockPosMutableBlockPos *)shortMutable) == NATIVE_ARRAY_FAILURE);
        CHECK(MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
    }
    for (unsigned kind = 0; kind < 3; ++kind) {
        const MCObjectClass *klass = kind == 0 ? Vec3i_nativeClass() : kind == 1 ? BlockPos_nativeClass() :
                                                                              BlockPosMutableBlockPos_nativeClass();
        size_t fullSize = kind == 0 ? sizeof(Vec3i) : kind == 1 ? sizeof(BlockPos) : sizeof(BlockPosMutableBlockPos);
        for (unsigned mode = 0; mode < 4; ++mode) {
            heap = MCObjectHeap_new(65536);
            CHECK(heap);
            Vec3i *valid = Vec3i_newInt(heap, 7, 8, 9);
            MCObject untracked = {heap, klass};
            MCObject *bad = mode == 0 ? &untracked : MCObjectHeap_alloc(heap,
                mode == 1 ? sizeof(MCObject) : mode == 2 ? fullSize - 1 : fullSize, mode == 3 ? &ordinaryClass : klass);
            CHECK(valid && bad);
            CHECK(!Vec3i_isInstance(bad) && !BlockPos_isInstance(bad) && !BlockPosMutableBlockPos_isInstance(bad));
            value = 47;
            CHECK(Vec3i_getX((Vec3i *)bad, &value) == NATIVE_ARRAY_FAILURE && value == 47);
            CHECK(MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
            CHECK(Vec3i_constructInt(valid, 4, 5, 6) == NATIVE_ARRAY_FAILURE);
            CHECK(valid->x == 7 && valid->y == 8 && valid->z == 9);
            MCObjectHeap_free(heap);
        }
    }
    for (unsigned mode = 0; mode < 10; ++mode) {
        heap = MCObjectHeap_new(65536);
        MCObjectHeap *other = MCObjectHeap_new(65536);
        CHECK(heap && other);
        BlockPosMutableBlockPos *mutable = BlockPosMutableBlockPos_newInt(heap, 1, 2, 3);
        Vec3i *baseOther = Vec3i_newInt(other, 1, 2, 3);
        CHECK(mutable && baseOther);
        value = 91; longValue = 92; equal = true; out = mutable;
        if (mode == 0)
            CHECK(Vec3i_equals(&mutable->blockPos.vec3i, (MCObject *)baseOther, &equal) == NATIVE_ARRAY_FAILURE && equal);
        else if (mode == 1) CHECK(Vec3i_getX(&mutable->blockPos.vec3i, NULL) == NATIVE_ARRAY_FAILURE);
        else if (mode == 2) CHECK(Vec3i_getY(&mutable->blockPos.vec3i, NULL) == NATIVE_ARRAY_FAILURE);
        else if (mode == 3) CHECK(Vec3i_getZ(&mutable->blockPos.vec3i, NULL) == NATIVE_ARRAY_FAILURE);
        else if (mode == 4) CHECK(Vec3i_equals(&mutable->blockPos.vec3i, NULL, NULL) == NATIVE_ARRAY_FAILURE);
        else if (mode == 5) CHECK(Vec3i_hashCode(&mutable->blockPos.vec3i, NULL) == NATIVE_ARRAY_FAILURE);
        else if (mode == 6) CHECK(BlockPos_toLong(&mutable->blockPos, NULL) == NATIVE_ARRAY_FAILURE);
        else if (mode == 7) CHECK(BlockPosMutableBlockPos_set(mutable, 4, 5, 6, NULL) == NATIVE_ARRAY_FAILURE);
        else if (mode == 8) { MCObjectHeap_fail(heap); CHECK(BlockPos_toLong(&mutable->blockPos, &longValue) == NATIVE_ARRAY_FAILURE && longValue == 92); }
        else { MCObjectHeap_fail(heap); CHECK(BlockPosMutableBlockPos_set(mutable, 4, 5, 6, &out) == NATIVE_ARRAY_FAILURE && out == mutable); }
        CHECK(mutable->x == 1 && mutable->y == 2 && mutable->z == 3);
        CHECK(MCObjectHeap_failed(heap) && !MCObjectHeap_failed(other) && !MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(other); MCObjectHeap_free(heap);
    }
    /* A real descriptor with a short payload must fail before a field read. */
    heap = MCObjectHeap_new(65536);
    CHECK(heap);
    Vec3i *valid = Vec3i_newInt(heap, 1, 2, 3);
    MCObject *shortKnown = MCObjectHeap_alloc(heap, sizeof(MCObject), BlockPosMutableBlockPos_nativeClass());
    CHECK(valid && shortKnown);
    equal = true;
    CHECK(Vec3i_equals(valid, shortKnown, &equal) == NATIVE_ARRAY_FAILURE && equal);
    CHECK(MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}
typedef struct {
    MCObject object;
    Vec3i *base;
    BlockPos *pos;
    BlockPosMutableBlockPos *mutable;
    BlockPos *immutable;
    Vec3i *nullVector;
    BlockPos *origin;
} Aliases;
static void trace_aliases(MCObject *object, MCObjectVisitor visit, void *context) {
    Aliases *a = (Aliases *)object;
    a->base = (Vec3i *)visit((MCObject *)a->base, context);
    a->pos = (BlockPos *)visit((MCObject *)a->pos, context);
    a->mutable = (BlockPosMutableBlockPos *)visit((MCObject *)a->mutable, context);
    a->immutable = (BlockPos *)visit((MCObject *)a->immutable, context);
    a->nullVector = (Vec3i *)visit((MCObject *)a->nullVector, context);
    a->origin = (BlockPos *)visit((MCObject *)a->origin, context);
}
static const MCObjectClass aliasClass = {"fixture.BlockPos.aliases", MCObjectHeap_plainClone, trace_aliases, NULL};
static void lifetime(void) {
    MCObjectHeap *heap = MCObjectHeap_new(1u << 20);
    CHECK(heap);
    Vec3i *zero = NativeVec3i_nullVector(heap);
    BlockPos *origin = NativeBlockPos_origin(heap);
    CHECK(zero && origin && (MCObject *)zero != (MCObject *)origin);
    CHECK(Vec3i_isRuntimeClass((MCObject *)zero) && BlockPos_isRuntimeClass((MCObject *)origin));
    size_t staticCount = MCObjectHeap_liveObjects(heap);
    CHECK(staticCount == 4);
    for (unsigned i = 0; i < 1000; ++i)
        CHECK(NativeVec3i_nullVector(heap) == zero && NativeBlockPos_origin(heap) == origin);
    CHECK(MCObjectHeap_liveObjects(heap) == staticCount);
    Aliases *aliases = (Aliases *)MCObjectHeap_alloc(heap, sizeof(*aliases), &aliasClass);
    CHECK(aliases);
    aliases->mutable = BlockPosMutableBlockPos_newInt(heap, 11, 12, 13);
    CHECK(aliases->mutable);
    aliases->base = &aliases->mutable->blockPos.vec3i;
    aliases->pos = &aliases->mutable->blockPos;
    aliases->immutable = BlockPos_add(aliases->pos, 1, 2, 3);
    aliases->nullVector = zero; aliases->origin = origin;
    CHECK(aliases->immutable);
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, heap, (MCObject *)aliases));
    CHECK(MCObjectHeap_collect(heap));
    CHECK(MCObjectHeap_liveObjects(heap) == staticCount + 3);
    MCObjectHeap *working = MCObjectHeap_clone(heap);
    CHECK(working);
    MCObjectRoot workingRoot = {0};
    CHECK(MCObjectRoot_rebind(&workingRoot, working, &root));
    Aliases *copy = (Aliases *)MCObjectRoot_get(&workingRoot);
    CHECK(copy && copy != aliases && copy->mutable != aliases->mutable);
    CHECK((void *)copy->base == (void *)copy->pos && (void *)copy->pos == (void *)copy->mutable);
    CHECK(copy->nullVector == NativeVec3i_nullVector(working) && copy->origin == NativeBlockPos_origin(working));
    CHECK((MCObject *)copy->nullVector != (MCObject *)copy->origin);
    BlockPosMutableBlockPos *out = NULL;
    CHECK(BlockPosMutableBlockPos_set(copy->mutable, 40, 50, 60, &out) == NATIVE_ARRAY_OK && out == copy->mutable);
    coordinates(copy->base, 40, 50, 60);
    /* Read the retained original fields without a mutable borrow scope; ending
       such a scope deliberately invalidates an outstanding native snapshot. */
    CHECK(aliases->mutable->x == 11 && aliases->mutable->y == 12 && aliases->mutable->z == 13);
    coordinates(&copy->immutable->vec3i, 12, 14, 16);
    CHECK(MCObjectHeap_collect(working));
    CHECK(MCObjectHeap_adopt(heap, working));
    MCObjectHeap_free(working);
    aliases = (Aliases *)MCObjectRoot_get(&root);
    CHECK(aliases && (void *)aliases->base == (void *)aliases->mutable && (void *)aliases->pos == (void *)aliases->mutable);
    CHECK(aliases->nullVector == NativeVec3i_nullVector(heap) && aliases->origin == NativeBlockPos_origin(heap));
    coordinates(aliases->base, 40, 50, 60);
    CHECK(aliases->mutable->blockPos.vec3i.x == 0 && aliases->mutable->blockPos.vec3i.y == 0 &&
          aliases->mutable->blockPos.vec3i.z == 0);
    MCObjectRoot_drop(&root);
    CHECK(MCObjectHeap_collect(heap) && MCObjectHeap_liveObjects(heap) == staticCount);
    CHECK(NativeVec3i_nullVector(heap) && NativeBlockPos_origin(heap));
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}
static void mutation_invalidates_snapshot(void) {
    for (unsigned kind = 0; kind < 4; ++kind) {
        MCObjectHeap *heap = MCObjectHeap_new(65536);
        CHECK(heap);
        BlockPosMutableBlockPos *mutable = BlockPosMutableBlockPos_newInt(heap, 1, 2, 3);
        CHECK(mutable);
        MCObjectRoot root = {0};
        CHECK(MCObjectRoot_init(&root, heap, (MCObject *)mutable));
        MCObjectHeap *working = MCObjectHeap_clone(heap);
        CHECK(working && MCObjectHeap_canAdopt(heap, working));
        if (kind == 0) {
            BlockPosMutableBlockPos *out = NULL;
            CHECK(BlockPosMutableBlockPos_set(mutable, 4, 5, 6, &out) == NATIVE_ARRAY_OK && out == mutable);
            CHECK(mutable->x == 4 && mutable->y == 5 && mutable->z == 6);
        } else if (kind == 1) {
            CHECK(BlockPosMutableBlockPos_constructInt(mutable, 7, 8, 9) == NATIVE_ARRAY_OK);
            CHECK(mutable->x == 7 && mutable->y == 8 && mutable->z == 9 && mutable->blockPos.vec3i.x == 0);
        } else if (kind == 2) {
            CHECK(BlockPos_constructInt(&mutable->blockPos, 10, 11, 12) == NATIVE_ARRAY_OK);
            CHECK(mutable->blockPos.vec3i.x == 10 && mutable->blockPos.vec3i.y == 11 && mutable->blockPos.vec3i.z == 12);
        } else {
            CHECK(Vec3i_constructInt(&mutable->blockPos.vec3i, 13, 14, 15) == NATIVE_ARRAY_OK);
            CHECK(mutable->blockPos.vec3i.x == 13 && mutable->blockPos.vec3i.y == 14 && mutable->blockPos.vec3i.z == 15);
        }
        CHECK(!MCObjectHeap_canAdopt(heap, working));
        CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_failed(working));
        MCObjectHeap_free(working);
        MCObjectRoot_drop(&root);
        MCObjectHeap_free(heap);
    }
}
static void budgets(void) {
    bool failure = false, success = false;
    for (size_t budget = 0; budget <= 512; ++budget) {
        MCObjectHeap *heap = MCObjectHeap_new(budget);
        CHECK(heap);
        BlockPosMutableBlockPos *self = BlockPosMutableBlockPos_newInt(heap, 5, 6, 7);
        if (self) {
            success = true;
            size_t before = MCObjectHeap_liveObjects(heap);
            BlockPosMutableBlockPos *out = NULL;
            CHECK(BlockPosMutableBlockPos_set(self, 8, 9, 10, &out) == NATIVE_ARRAY_OK && out == self);
            CHECK(MCObjectHeap_liveObjects(heap) == before);
            BlockPos *sum = BlockPos_add(&self->blockPos, 1, 1, 1);
            if (sum) coordinates(&sum->vec3i, 9, 10, 11);
            else CHECK(MCObjectHeap_failed(heap) && self->x == 8 && self->y == 9 && self->z == 10);
        } else { failure = true; CHECK(MCObjectHeap_failed(heap)); }
        CHECK(!MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
    }
    CHECK(failure && success);
    for (unsigned kind = 0; kind < 4; ++kind) {
        failure = false; success = false;
        for (size_t budget = 0; budget <= 512; ++budget) {
            MCObjectHeap *heap = MCObjectHeap_new(budget);
            CHECK(heap);
            MCObject *value = kind == 0 ? (MCObject *)Vec3i_newDouble(heap, -1.25, NAN, INFINITY) :
                kind == 1 ? (MCObject *)BlockPos_newDouble(heap, -1.25, NAN, INFINITY) :
                kind == 2 ? (MCObject *)NativeVec3i_nullVector(heap) : (MCObject *)NativeBlockPos_origin(heap);
            if (value) { success = true; CHECK(!MCObjectHeap_failed(heap)); }
            else { failure = true; CHECK(MCObjectHeap_failed(heap)); }
            CHECK(!MCObjectHeap_hasBorrowers(heap));
            MCObjectHeap_free(heap);
        }
        CHECK(failure && success);
    }
}
static void malformed_statics(void) {
    /* Root handles expose actual rooted native holder identity. Derive the next
       root ID from a preceding explicit root in this isolated fixture, then
       use byte copies to corrupt the private reference payload without aliasing
       an unrelated C struct. This tests native lifetime guards, not Java state. */
    for (unsigned kind = 0; kind < 2; ++kind) {
        for (unsigned mode = 0; mode < 7; ++mode) {
            MCObjectHeap *heap = MCObjectHeap_new(65536), *foreignHeap = MCObjectHeap_new(65536);
            CHECK(heap && foreignHeap);
            MCObject *probe = MCObjectHeap_alloc(heap, sizeof(MCObject), &ordinaryClass);
            MCObjectRoot before = {0};
            CHECK(probe && MCObjectRoot_init(&before, heap, probe));
            MCObject *value = kind == 0 ? (MCObject *)NativeVec3i_nullVector(heap) : (MCObject *)NativeBlockPos_origin(heap);
            CHECK(value);
            MCObjectRoot holderRoot = {heap, before.id + 1};
            MCObject *holder = MCObjectRoot_get(&holderRoot);
            CHECK(holder && MCObjectHeap_objectSize(holder) >= sizeof(MCObject) + sizeof(MCObject *));
            CHECK(strcmp(holder->klass->name, kind == 0 ? "native.Vec3i.statics" : "native.BlockPos.statics") == 0);
            if (mode <= 1) {
                MCObject *shortHolder = MCObjectHeap_alloc(heap, sizeof(MCObject), holder->klass);
                CHECK(shortHolder);
                if (mode == 1) {
                    MCObjectRoot malformedRoot = {0};
                    CHECK(MCObjectRoot_init(&malformedRoot, heap, shortHolder));
                    CHECK(!MCObjectHeap_collect(heap) && MCObjectHeap_failed(heap));
                } else {
                    CHECK((kind == 0 ? (MCObject *)NativeVec3i_nullVector(heap) :
                                      (MCObject *)NativeBlockPos_origin(heap)) == NULL);
                    CHECK(MCObjectHeap_failed(heap));
                }
            } else {
                MCObject untracked = {heap, kind == 0 ? Vec3i_nativeClass() : BlockPos_nativeClass()};
                MCObject *replacement = mode == 2 ? (kind == 0 ? (MCObject *)Vec3i_newInt(foreignHeap, 0, 0, 0) :
                                                                 (MCObject *)BlockPos_newInt(foreignHeap, 0, 0, 0)) :
                    mode == 3 ? &untracked : mode == 4 ? NULL : mode == 5 ? probe :
                    (kind == 0 ? (MCObject *)Vec3i_newInt(heap, 1, 0, 0) : (MCObject *)BlockPos_newInt(heap, 1, 0, 0));
                CHECK(mode == 4 || replacement);
                memcpy((unsigned char *)holder + sizeof(MCObject), &replacement, sizeof(replacement));
                MCObjectHeap_touch(heap);
                CHECK((kind == 0 ? (MCObject *)NativeVec3i_nullVector(heap) :
                                  (MCObject *)NativeBlockPos_origin(heap)) == NULL);
                CHECK(MCObjectHeap_failed(heap) && !MCObjectHeap_failed(foreignHeap));
            }
            CHECK(!MCObjectHeap_hasBorrowers(heap));
            MCObjectHeap_free(foreignHeap); MCObjectHeap_free(heap);
        }
    }
}
int main(void) {
    constructors(); double_constructors(); mutable_aliases(); arithmetic(); packed();
    exceptions_and_shapes(); lifetime(); mutation_invalidates_snapshot(); budgets(); malformed_statics();
    printf("Source BlockPos: %u checks passed\n", checks);
    return 0;
}
