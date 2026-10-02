#include "nbt/NBTInternal.h"
#include "util/NativeLinkedHashMap.h"
#include <stdio.h>
#include <stdlib.h>

static int checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "check %d line %d: %s\n", checks, __LINE__, #x);                       \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

/* Bucket-order traversal, replacement relocation, or snapshot views must fail
   this observable order contract. Literal expectations come from the separate
   Java8 LinkedHashMap witness, not the implementation under test. */
static void insertion_order(void) {
    MCObjectHeap *heap = MCObjectHeap_new(1u << 20);
    CHECK(heap);
    NativeLinkedHashMap *map = NativeLinkedHashMap_new(heap);
    CHECK(map);
    NativeLinkedHashMapView *values = NativeLinkedHashMap_values(map);
    CHECK(values);
    NBTString *keys[3], *items[3];
    const char *names[] = {"z", "a", "m"};
    for (int i = 0; i < 3; i++) {
        keys[i] = NBTString_fromASCII(heap, names[i]);
        items[i] = NBTString_fromASCII(heap, names[i]);
        CHECK(keys[i] && items[i]);
        CHECK(NativeLinkedHashMap_put(map, (MCObject *)keys[i], (MCObject *)items[i]));
    }
    CHECK(NativeLinkedHashMapView_size(values) == 3);
    CHECK(NativeLinkedHashMap_values(map) == values);
    NativeLinkedHashMapIterator *it = NativeLinkedHashMapIterator_fromView(values);
    CHECK(it);
    for (int i = 0; i < 3; i++) {
        MCObject *out = NULL;
        CHECK(NativeLinkedHashMapIterator_hasNext(it));
        CHECK(NativeLinkedHashMapIterator_next(it, &out));
        CHECK(out == (MCObject *)items[i]);
    }
    CHECK(!NativeLinkedHashMapIterator_hasNext(it));
    CHECK(!MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}
static MCObject *next(NativeLinkedHashMapIterator *it) {
    MCObject *out = NULL;
    CHECK(NativeLinkedHashMapIterator_hasNext(it));
    CHECK(NativeLinkedHashMapIterator_next(it, &out));
    return out;
}
static void replacement_reinsert_nullable_utf16(void) {
    MCObjectHeap *h = MCObjectHeap_new(1u << 20);
    CHECK(h);
    NativeLinkedHashMap *m = NativeLinkedHashMap_new(h);
    CHECK(m && NativeLinkedHashMap_isInstance((MCObject *)m));
    NBTString *z = NBTString_fromASCII(h, "z"), *a = NBTString_fromASCII(h, "a"),
              *equal = NBTString_fromASCII(h, "z");
    NBTString *old = NBTString_fromASCII(h, "old"), *fresh = NBTString_fromASCII(h, "fresh");
    CHECK(z && a && equal && old && fresh);
    CHECK(NativeLinkedHashMap_put(m, (MCObject *)z, (MCObject *)old));
    CHECK(NativeLinkedHashMap_put(m, (MCObject *)a, (MCObject *)a));
    NativeLinkedHashMapView *keys = NativeLinkedHashMap_keys(m),
                            *values = NativeLinkedHashMap_values(m);
    CHECK(keys && values && NativeLinkedHashMapView_isInstance((MCObject *)values));
    CHECK(NativeLinkedHashMap_keys(m) == keys);
    NativeLinkedHashMapIterator *it = NativeLinkedHashMapIterator_fromView(values);
    CHECK(it);
    MCObject *previous = (MCObject *)equal;
    CHECK(NativeLinkedHashMap_putWithPrevious(m, (MCObject *)equal, (MCObject *)fresh, &previous));
    CHECK(previous == (MCObject *)old && NativeLinkedHashMap_size(m) == 2);
    CHECK(next(it) == (MCObject *)fresh);
    CHECK(next(it) == (MCObject *)a);
    CHECK(!NativeLinkedHashMapIterator_hasNext(it));
    it = NativeLinkedHashMapIterator_fromView(keys);
    CHECK(it);
    CHECK(next(it) == (MCObject *)z);
    CHECK(next(it) == (MCObject *)a);
    CHECK(NativeLinkedHashMap_remove(m, (MCObject *)z) == (MCObject *)fresh);
    CHECK(!NativeLinkedHashMap_containsKey(m, (MCObject *)equal));
    CHECK(NativeLinkedHashMap_put(m, (MCObject *)equal, NULL));
    it = NativeLinkedHashMapIterator_fromView(keys);
    CHECK(it);
    CHECK(next(it) == (MCObject *)a);
    CHECK(next(it) == (MCObject *)equal);
    CHECK(NativeLinkedHashMap_get(m, (MCObject *)z) == NULL &&
          NativeLinkedHashMap_containsKey(m, (MCObject *)z));
    CHECK(NativeLinkedHashMap_put(m, NULL, NULL));
    NBTString *empty = NBTString_fromASCII(h, "");
    CHECK(empty);
    CHECK(NativeLinkedHashMap_put(m, (MCObject *)empty, (MCObject *)old));
    const uint16_t units[] = {0, 0xd800, 0x65e5};
    NBTString *unicode = NBTString_fromUTF16(h, units, 3),
              *unicodeCopy = NBTString_fromUTF16(h, units, 3);
    CHECK(unicode && unicodeCopy);
    CHECK(NativeLinkedHashMap_put(m, (MCObject *)unicode, (MCObject *)fresh));
    CHECK(NativeLinkedHashMap_get(m, (MCObject *)unicodeCopy) == (MCObject *)fresh);
    CHECK(NativeLinkedHashMap_containsKey(m, NULL));
    CHECK(NativeLinkedHashMap_get(m, NULL) == NULL);
    CHECK(NativeLinkedHashMap_get(m, (MCObject *)empty) == (MCObject *)old);
    CHECK(NativeLinkedHashMapView_size(values) == 5);
    it = NativeLinkedHashMapIterator_fromView(values);
    CHECK(it);
    CHECK(next(it) == (MCObject *)a);
    CHECK(next(it) == NULL);
    CHECK(next(it) == NULL);
    CHECK(next(it) == (MCObject *)old);
    CHECK(next(it) == (MCObject *)fresh);
    CHECK(NativeLinkedHashMap_remove(m, NULL) == NULL && !NativeLinkedHashMap_containsKey(m, NULL));
    CHECK(NativeLinkedHashMap_remove(m, NULL) == NULL && !MCObjectHeap_failed(h));
    CHECK(NativeLinkedHashMapView_clear(values) && NativeLinkedHashMap_size(m) == 0);
    CHECK(NativeLinkedHashMapView_size(keys) == 0);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void growth_preserves_encounter_order(void) {
    MCObjectHeap *h = MCObjectHeap_new(1u << 20);
    CHECK(h);
    NativeLinkedHashMap *m = NativeLinkedHashMap_new(h);
    CHECK(m);
    MCObject *items[200];
    for (int i = 0; i < 200; i++) {
        char name[30];
        snprintf(name, sizeof(name), "entry-%d", i);
        NBTString *k = NBTString_fromASCII(h, name);
        CHECK(k);
        items[i] = (MCObject *)k;
        CHECK(NativeLinkedHashMap_put(m, items[i], items[i]));
    }
    CHECK(NativeLinkedHashMap_size(m) == 200);
    NativeLinkedHashMapIterator *it =
        NativeLinkedHashMapIterator_fromView(NativeLinkedHashMap_values(m));
    CHECK(it);
    for (int i = 0; i < 200; i++) {
        CHECK(next(it) == items[i]);
        CHECK(NativeLinkedHashMap_get(m, items[i]) == items[i]);
    }
    CHECK(!NativeLinkedHashMapIterator_hasNext(it));
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void structural_changes_reject_next(void) {
    for (int mode = 0; mode < 5; mode++) {
        MCObjectHeap *h = MCObjectHeap_new(1u << 20);
        CHECK(h);
        NativeLinkedHashMap *m = NativeLinkedHashMap_new(h);
        CHECK(m);
        NBTString *k = NBTString_fromASCII(h, "one");
        CHECK(k);
        if (mode < 3)
            CHECK(NativeLinkedHashMap_put(m, (MCObject *)k, NULL));
        NativeLinkedHashMapIterator *it =
            NativeLinkedHashMapIterator_fromView(NativeLinkedHashMap_values(m));
        CHECK(it);
        if (mode == 0)
            CHECK(NativeLinkedHashMap_put(m, NULL, NULL));
        if (mode == 1)
            CHECK(NativeLinkedHashMap_remove(m, (MCObject *)k) == NULL);
        if (mode == 2 || mode == 3)
            CHECK(NativeLinkedHashMap_clear(m));
        CHECK(NativeLinkedHashMapIterator_hasNext(it) == (mode < 3));
        MCObject *out = (MCObject *)k;
        CHECK(!NativeLinkedHashMapIterator_next(it, &out));
        CHECK(out == (MCObject *)k && MCObjectHeap_failed(h));
        MCObjectHeap_free(h);
    }
}
static void healthy_source_exceptions_preserve_prefix(void) {
    for (int mode = 0; mode < 3; mode++) {
        MCObjectHeap *h = MCObjectHeap_new(1u << 20);
        CHECK(h);
        NativeLinkedHashMap *m = NativeLinkedHashMap_new(h);
        CHECK(m);
        NBTString *k = NBTString_fromASCII(h, "first");
        CHECK(k);
        if (mode == 0)
            CHECK(NativeLinkedHashMap_put(m, (MCObject *)k, (MCObject *)k));
        NativeLinkedHashMapView *v = NativeLinkedHashMap_values(m);
        CHECK(v);
        NativeLinkedHashMapIterator *it = NativeLinkedHashMapIterator_fromView(v);
        CHECK(it);
        if (mode < 2)
            CHECK(NativeLinkedHashMap_clear(m));
        MCObject *out = (MCObject *)k;
        CHECK(NativeLinkedHashMapIterator_nextSource(it, &out) == NATIVE_LINKED_ITERATOR_EXCEPTION);
        CHECK(out == (MCObject *)k && !MCObjectHeap_failed(h));
        CHECK(NativeLinkedHashMapIterator_hasNext(it) == (mode == 0));
        CHECK(NativeLinkedHashMapIterator_nextSource(it, &out) == NATIVE_LINKED_ITERATOR_EXCEPTION);
        CHECK(out == (MCObject *)k && !MCObjectHeap_failed(h));
        CHECK(NativeLinkedHashMap_put(m, NULL, NULL));
        NativeLinkedHashMapIterator *fresh = NativeLinkedHashMapIterator_fromView(v);
        CHECK(fresh);
        CHECK(NativeLinkedHashMapIterator_nextSource(fresh, &out) == NATIVE_LINKED_ITERATOR_OK &&
              out == NULL);
        CHECK(!NativeLinkedHashMapIterator_hasNext(fresh));
        out = (MCObject *)k;
        CHECK(NativeLinkedHashMapIterator_nextSource(fresh, &out) ==
              NATIVE_LINKED_ITERATOR_EXCEPTION);
        CHECK(out == (MCObject *)k && !MCObjectHeap_failed(h));
        CHECK(!NativeLinkedHashMapIterator_next(fresh, &out) && MCObjectHeap_failed(h) &&
              out == (MCObject *)k);
        MCObjectHeap_free(h);
    }
    MCObjectHeap *h = MCObjectHeap_new(1u << 20);
    CHECK(h);
    NativeLinkedHashMap *m = NativeLinkedHashMap_new(h);
    CHECK(m);
    NativeLinkedHashMapIterator *it =
        NativeLinkedHashMapIterator_fromView(NativeLinkedHashMap_values(m));
    CHECK(it);
    CHECK(NativeLinkedHashMapIterator_nextSource(it, NULL) == NATIVE_LINKED_ITERATOR_FAILURE &&
          MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void retained_iterator_keeps_removed_graph_alive(void) {
    MCObjectHeap *h = MCObjectHeap_new(1u << 20);
    CHECK(h);
    NativeLinkedHashMap *m = NativeLinkedHashMap_new(h);
    CHECK(m);
    NBTString *key = NBTString_fromASCII(h, "retained"), *value = NBTString_fromASCII(h, "value");
    CHECK(key && value);
    CHECK(NativeLinkedHashMap_put(m, (MCObject *)key, (MCObject *)value));
    NativeLinkedHashMapIterator *it =
        NativeLinkedHashMapIterator_fromView(NativeLinkedHashMap_values(m));
    CHECK(it);
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, h, (MCObject *)it));
    CHECK(NativeLinkedHashMap_clear(m));
    CHECK(MCObjectHeap_collect(h));
    CHECK(NativeLinkedHashMapIterator_hasNext(it));
    MCObject *out = (MCObject *)value;
    CHECK(NativeLinkedHashMapIterator_nextSource(it, &out) == NATIVE_LINKED_ITERATOR_EXCEPTION);
    CHECK(out == (MCObject *)value && NBTString_equalsASCII((NBTString *)out, "value") &&
          !MCObjectHeap_failed(h));
    MCObjectRoot_drop(&root);
    CHECK(MCObjectHeap_collect(h) && MCObjectHeap_liveObjects(h) == 0);
    MCObjectHeap_free(h);
}
static void aliases_survive_clone_adopt_and_gc(void) {
    MCObjectHeap *h = MCObjectHeap_new(1u << 20);
    CHECK(h);
    NativeLinkedHashMap *m = NativeLinkedHashMap_new(h);
    CHECK(m);
    NBTString *z = NBTString_fromASCII(h, "z"), *a = NBTString_fromASCII(h, "a");
    CHECK(z && a);
    CHECK(NativeLinkedHashMap_put(m, (MCObject *)z, (MCObject *)a));
    CHECK(NativeLinkedHashMap_put(m, (MCObject *)a, (MCObject *)a));
    CHECK(NativeLinkedHashMap_put(m, NULL, (MCObject *)m));
    NativeLinkedHashMapView *v = NativeLinkedHashMap_values(m), *keys = NativeLinkedHashMap_keys(m);
    CHECK(v && keys);
    NativeLinkedHashMapIterator *it = NativeLinkedHashMapIterator_fromView(v);
    CHECK(it);
    CHECK(next(it) == (MCObject *)a);
    MCObjectRoot rm = {0}, rv = {0}, ri = {0}, ra = {0};
    CHECK(MCObjectRoot_init(&rm, h, (MCObject *)m));
    CHECK(MCObjectRoot_init(&rv, h, (MCObject *)v));
    CHECK(MCObjectRoot_init(&ri, h, (MCObject *)it));
    CHECK(MCObjectRoot_init(&ra, h, (MCObject *)a));
    CHECK(MCObjectHeap_collect(h));
    CHECK(NativeLinkedHashMap_values(m) == v);
    MCObjectHeap *working = MCObjectHeap_clone(h);
    CHECK(working);
    MCObjectRoot wm = {0}, wv = {0}, wi = {0}, wa = {0};
    CHECK(MCObjectRoot_rebind(&wm, working, &rm));
    CHECK(MCObjectRoot_rebind(&wv, working, &rv));
    CHECK(MCObjectRoot_rebind(&wi, working, &ri));
    CHECK(MCObjectRoot_rebind(&wa, working, &ra));
    NativeLinkedHashMap *copy = (NativeLinkedHashMap *)MCObjectRoot_get(&wm);
    NativeLinkedHashMapView *cv = (NativeLinkedHashMapView *)MCObjectRoot_get(&wv);
    NativeLinkedHashMapIterator *ci = (NativeLinkedHashMapIterator *)MCObjectRoot_get(&wi);
    MCObject *ca = MCObjectRoot_get(&wa);
    CHECK(copy != m && cv != v && ci != it && ca != (MCObject *)a);
    CHECK(NativeLinkedHashMap_values(copy) == cv);
    CHECK(next(ci) == ca);
    CHECK(next(ci) == (MCObject *)copy);
    CHECK(NativeLinkedHashMapIterator_hasNext(it));
    CHECK(!NativeLinkedHashMapIterator_hasNext(ci));
    CHECK(NativeLinkedHashMap_put(copy, NULL, ca));
    CHECK(NativeLinkedHashMap_size(m) == 3 && NativeLinkedHashMapView_size(v) == 3);
    CHECK(MCObjectHeap_canAdopt(h, working));
    CHECK(MCObjectHeap_adopt(h, working));
    /* Rebound working handles alias the same root IDs; do not drop those IDs
       while the original root handles still own them. */
    MCObjectHeap_free(working);
    m = (NativeLinkedHashMap *)MCObjectRoot_get(&rm);
    v = (NativeLinkedHashMapView *)MCObjectRoot_get(&rv);
    a = (NBTString *)MCObjectRoot_get(&ra);
    CHECK(NativeLinkedHashMap_values(m) == v && NativeLinkedHashMap_get(m, NULL) == (MCObject *)a);
    CHECK(MCObjectHeap_collect(h));
    CHECK(NativeLinkedHashMapView_size(v) == 3);
    MCObjectRoot_drop(&rm);
    MCObjectRoot_drop(&rv);
    MCObjectRoot_drop(&ri);
    MCObjectRoot_drop(&ra);
    CHECK(MCObjectHeap_collect(h) && MCObjectHeap_liveObjects(h) == 0);
    MCObjectHeap_free(h);
}
static const MCObjectClass wrongClass = {"fixture.Wrong", MCObjectHeap_plainClone, NULL, NULL};
static void rejected_boundaries(void) {
    for (int mode = 0; mode < 8; mode++) {
        MCObjectHeap *h = MCObjectHeap_new(1u << 20), *foreign = MCObjectHeap_new(1u << 20);
        CHECK(h && foreign);
        NativeLinkedHashMap *m = NativeLinkedHashMap_new(h);
        CHECK(m);
        NBTString *k = NBTString_fromASCII(h, "key");
        CHECK(k);
        MCObject *out = (MCObject *)k;
        if (mode == 0) {
            MCObject *wrong = MCObjectHeap_alloc(h, sizeof(MCObject), &wrongClass);
            CHECK(wrong);
            CHECK(!NativeLinkedHashMap_putWithPrevious(m, wrong, NULL, &out));
        } else if (mode == 1) {
            NBTString *fk = NBTString_fromASCII(foreign, "key");
            CHECK(fk);
            CHECK(!NativeLinkedHashMap_put(m, (MCObject *)fk, NULL));
        } else if (mode == 2) {
            MCObject *fv = MCObjectHeap_alloc(foreign, sizeof(MCObject), &wrongClass);
            CHECK(fv);
            CHECK(!NativeLinkedHashMap_put(m, NULL, fv));
        } else if (mode == 3) {
            CHECK(!NativeLinkedHashMap_putWithPrevious(m, NULL, NULL, NULL));
        } else if (mode == 4) {
            MCObject *tiny = MCObjectHeap_alloc(h, sizeof(MCObject), ((MCObject *)m)->klass);
            CHECK(tiny);
            CHECK(!NativeLinkedHashMap_isInstance(tiny));
            CHECK(NativeLinkedHashMap_size((NativeLinkedHashMap *)tiny) == -1);
        } else if (mode == 5) {
            NativeLinkedHashMapView *v = NativeLinkedHashMap_values(m);
            CHECK(v);
            MCObject *tiny = MCObjectHeap_alloc(h, sizeof(MCObject), ((MCObject *)v)->klass);
            CHECK(tiny);
            CHECK(!NativeLinkedHashMapView_isInstance(tiny));
            CHECK(NativeLinkedHashMapView_size((NativeLinkedHashMapView *)tiny) == -1);
        } else if (mode == 6) {
            NativeLinkedHashMapIterator *it =
                NativeLinkedHashMapIterator_fromView(NativeLinkedHashMap_values(m));
            CHECK(it);
            MCObject *tiny = MCObjectHeap_alloc(h, sizeof(MCObject), ((MCObject *)it)->klass);
            CHECK(tiny);
            CHECK(!NativeLinkedHashMapIterator_isInstance(tiny));
            CHECK(!NativeLinkedHashMapIterator_next((NativeLinkedHashMapIterator *)tiny, &out));
        } else {
            MCObject *tiny = MCObjectHeap_alloc(h, sizeof(MCObject), ((MCObject *)k)->klass);
            CHECK(tiny);
            CHECK(!NativeLinkedHashMap_putWithPrevious(m, tiny, NULL, &out));
        }
        CHECK(out == (MCObject *)k && MCObjectHeap_failed(h) && !MCObjectHeap_failed(foreign));
        MCObjectHeap_free(h);
        MCObjectHeap_free(foreign);
    }
    MCObjectHeap *h = MCObjectHeap_new(1);
    CHECK(h);
    CHECK(!NativeLinkedHashMap_new(h) && MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h = MCObjectHeap_new(4096);
    CHECK(h);
    NativeLinkedHashMap *m = NativeLinkedHashMap_new(h);
    CHECK(m);
    NBTString *k = NBTString_fromASCII(h, "oom");
    CHECK(k);
    size_t remaining = 4096 - MCObjectHeap_liveBytes(h);
    CHECK(remaining >= sizeof(MCObject));
    CHECK(MCObjectHeap_alloc(h, remaining, &wrongClass));
    MCObject *previous = (MCObject *)k;
    CHECK(!NativeLinkedHashMap_putWithPrevious(m, (MCObject *)k, NULL, &previous));
    CHECK(previous == (MCObject *)k && MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void collision_tree_boundary(void) {
    MCObjectHeap *h = MCObjectHeap_new(1u << 20);
    CHECK(h);
    NativeLinkedHashMap *m = NativeLinkedHashMap_new(h);
    CHECK(m);
    NBTString *keys[11];
    /* Concatenations of Aa/BB have the same Java String hash. */
    for (int i = 0; i < 11; i++) {
        char key[9];
        for (int j = 0; j < 4; j++) {
            key[j * 2] = (i & (1 << j)) ? 'B' : 'A';
            key[j * 2 + 1] = (i & (1 << j)) ? 'B' : 'a';
        }
        key[8] = 0;
        keys[i] = NBTString_fromASCII(h, key);
        CHECK(keys[i]);
        if (i < 10)
            CHECK(NativeLinkedHashMap_put(m, (MCObject *)keys[i], (MCObject *)keys[i]));
    }
    CHECK(NativeLinkedHashMap_size(m) == 10);
    NativeLinkedHashMapIterator *it =
        NativeLinkedHashMapIterator_fromView(NativeLinkedHashMap_keys(m));
    CHECK(it);
    for (int i = 0; i < 10; i++)
        CHECK(next(it) == (MCObject *)keys[i]);
    MCObject *out = (MCObject *)keys[0];
    CHECK(!NativeLinkedHashMap_putWithPrevious(m, (MCObject *)keys[10], NULL, &out));
    CHECK(out == (MCObject *)keys[0] && MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
int main(void) {
    insertion_order();
    replacement_reinsert_nullable_utf16();
    growth_preserves_encounter_order();
    structural_changes_reject_next();
    healthy_source_exceptions_preserve_prefix();
    retained_iterator_keeps_removed_graph_alive();
    aliases_survive_clone_adopt_and_gc();
    rejected_boundaries();
    collision_tree_boundary();
    printf("native linked hash map: %d checks\n", checks);
    return 0;
}
