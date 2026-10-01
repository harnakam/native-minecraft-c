#include "util/NativeHashMap.h"
#include <stdio.h>
#include <stdlib.h>
static int checks;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      fprintf(stderr, "check %d line %d: %s\n", checks, __LINE__, #x);         \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
typedef struct {
  MCObject object;
  int32_t value, hash;
  int hashes, equals;
  bool reject;
} Key;
static const MCObjectClass keyClass = {"fixture.HashKey",
                                       MCObjectHeap_plainClone, NULL, NULL};
static bool keyHash(MCObject *ctx, MCObject *key, int32_t *out) {
  (void)ctx;
  Key *k = (Key *)key;
  ++k->hashes;
  *out = k->hash;
  return !k->reject;
}
static bool keyEquals(MCObject *ctx, MCObject *q, MCObject *stored, bool *out) {
  (void)ctx;
  Key *k = (Key *)q;
  ++k->equals;
  *out = stored && k->value == ((Key *)stored)->value;
  return !k->reject;
}
static const NativeHashKeyMethods keyMethods = {keyHash, keyEquals};
int main(void) {
  MCObjectHeap *heap = MCObjectHeap_new(1u << 20);
  CHECK(heap);
  NativeHashMap *map = NativeHashMap_new(heap, NATIVE_HASH_KEY_STRING);
  CHECK(map);
  CHECK(NativeHashMap_size(map) == 0);
  NBTString *key = NBTString_fromASCII(heap, "Aa"),
            *equal = NBTString_fromASCII(heap, "Aa");
  NBTString *value = NBTString_fromASCII(heap, "value");
  CHECK(key && equal && value && key != equal);
  CHECK(NativeHashMap_put(map, (MCObject *)key, (MCObject *)value));
  CHECK(NativeHashMap_get(map, (MCObject *)equal) == (MCObject *)value);
  CHECK(NativeHashMap_put(map, (MCObject *)equal, NULL));
  CHECK(NativeHashMap_size(map) == 1);
  CHECK(NativeHashMap_containsKey(map, (MCObject *)key));
  CHECK(!NativeHashMap_get(map, (MCObject *)key));
  NativeHashMapView *keys = NativeHashMap_keys(map),
                    *values = NativeHashMap_values(map);
  CHECK(keys && values);
  CHECK(NativeHashMap_keys(map) == keys && NativeHashMap_values(map) == values);
  NativeIterator *it = NativeIterator_fromView(keys);
  CHECK(it && NativeIterator_hasNext(it));
  MCObject *out = NULL;
  CHECK(NativeIterator_next(it, &out) && out == (MCObject *)key);
  CHECK(!NativeIterator_hasNext(it));
  CHECK(NativeHashMap_put(map, NULL, (MCObject *)value));
  CHECK(NativeHashMapView_size(keys) == 2);
  CHECK(NativeHashMapView_remove(values, NULL));
  CHECK(NativeHashMap_size(map) == 1);
  CHECK(!NativeHashMap_containsKey(map, (MCObject *)key));
  CHECK(NativeHashMap_remove(map, NULL) == (MCObject *)value);
  CHECK(NativeHashMap_size(map) == 0);
  NativeReferenceList *list = NativeReferenceList_new(heap);
  CHECK(list);
  CHECK(NativeReferenceList_add(list, NULL));
  CHECK(NativeReferenceList_add(list, (MCObject *)key));
  it = NativeIterator_fromList(list);
  CHECK(it);
  out = (MCObject *)value;
  CHECK(NativeIterator_next(it, &out) && out == NULL);
  CHECK(NativeReferenceList_set(list, 1, (MCObject *)equal) == (MCObject *)key);
  CHECK(NativeIterator_next(it, &out) && out == (MCObject *)equal);
  CHECK(!NativeIterator_hasNext(it));
  for (int i = 0; i < 200; i++) {
    char name[24];
    snprintf(name, sizeof(name), "entry-%d", i);
    NBTString *k = NBTString_fromASCII(heap, name);
    CHECK(k && NativeHashMap_put(map, (MCObject *)k, (MCObject *)value));
  }
  CHECK(NativeHashMap_size(map) == 200);
  for (int i = 0; i < 200; i++) {
    char name[24];
    snprintf(name, sizeof(name), "entry-%d", i);
    NBTString *k = NBTString_fromASCII(heap, name);
    CHECK(NativeHashMap_get(map, (MCObject *)k) == (MCObject *)value);
  }
  MCObjectRoot root = {0};
  CHECK(MCObjectRoot_init(&root, heap, (MCObject *)keys));
  CHECK(MCObjectHeap_collect(heap));
  MCObjectHeap *clone = MCObjectHeap_clone(heap);
  CHECK(clone);
  MCObjectRoot r2 = {0};
  CHECK(MCObjectRoot_rebind(&r2, clone, &root));
  NativeHashMapView *copied = (NativeHashMapView *)MCObjectRoot_get(&r2);
  CHECK(copied && copied != keys && NativeHashMapView_size(copied) == 200);
  CHECK(NativeHashMapView_clear(copied));
  CHECK(NativeHashMapView_size(copied) == 0 &&
        NativeHashMapView_size(keys) == 200);
  CHECK(MCObjectHeap_adopt(heap, clone));
  MCObjectHeap_free(clone);
  keys = (NativeHashMapView *)MCObjectRoot_get(&root);
  CHECK(NativeHashMapView_size(keys) == 0);
  MCObjectRoot_drop(&root);
  MCObjectHeap_free(heap);
  heap = MCObjectHeap_new(1u << 20);
  map = NativeHashMap_new(heap, NATIVE_HASH_KEY_STRING);
  key = NBTString_fromASCII(heap, "x");
  CHECK(NativeHashMap_put(map, (MCObject *)key, NULL));
  it = NativeIterator_fromView(NativeHashMap_keys(map));
  CHECK(it);
  CHECK(NativeHashMap_put(map, NULL, NULL));
  CHECK(NativeIterator_hasNext(it));
  out = (MCObject *)key;
  CHECK(!NativeIterator_next(it, &out) && out == (MCObject *)key &&
        MCObjectHeap_failed(heap));
  MCObjectHeap_free(heap);
  heap = MCObjectHeap_new(1u << 20);
  list = NativeReferenceList_new(heap);
  CHECK(NativeReferenceList_add(list, NULL));
  it = NativeIterator_fromList(list);
  CHECK(NativeReferenceList_clear(list));
  CHECK(!NativeIterator_hasNext(it));
  out = NULL;
  CHECK(!NativeIterator_next(it, &out) && MCObjectHeap_failed(heap));
  MCObjectHeap_free(heap);
  heap = MCObjectHeap_new(1u << 20);
  map = NativeHashMap_newWithKeys(heap, &keyMethods, NULL);
  CHECK(map);
  Key *first = (Key *)MCObjectHeap_alloc(heap, sizeof(*first), &keyClass),
      *query = (Key *)MCObjectHeap_alloc(heap, sizeof(*query), &keyClass);
  CHECK(first && query);
  first->value = query->value = 7;
  first->hash = query->hash = -1;
  CHECK(NativeHashMap_put(map, (MCObject *)first, NULL) && first->hashes == 1 &&
        first->equals == 0);
  CHECK(NativeHashMap_containsKey(map, (MCObject *)query) &&
        query->hashes == 1 && query->equals == 1 && first->equals == 0);
  CHECK(NativeHashMap_put(map, (MCObject *)query, (MCObject *)query));
  int hashesBefore = query->hashes, equalsBefore = query->equals;
  out = (MCObject *)first;
  CHECK(NativeHashMap_putWithPrevious(map, (MCObject *)query, (MCObject *)first,
                                      &out) &&
        out == (MCObject *)query && query->hashes == hashesBefore + 1 &&
        query->equals == equalsBefore + 1);
  hashesBefore = first->hashes;
  out = (MCObject *)query;
  CHECK(NativeHashMap_putWithPrevious(map, (MCObject *)first, (MCObject *)query,
                                      &out) &&
        out == (MCObject *)first && first->hashes == hashesBefore + 1 &&
        first->equals == 0);
  Key *newKey = (Key *)MCObjectHeap_alloc(heap, sizeof(*newKey), &keyClass);
  CHECK(newKey);
  newKey->value = 9;
  newKey->hash = 3;
  out = (MCObject *)first;
  CHECK(NativeHashMap_putWithPrevious(map, (MCObject *)newKey, NULL, &out) &&
        out == NULL && newKey->hashes == 1 && newKey->equals == 0);
  it = NativeIterator_fromView(NativeHashMap_keys(map));
  out = NULL;
  CHECK(NativeIterator_next(it, &out) && out == (MCObject *)first);
  first->hash = 0;
  CHECK(!NativeHashMap_containsKey(map, (MCObject *)first));
  first->hash = -1;
  CHECK(NativeHashMap_get(map, (MCObject *)first) == (MCObject *)query);
  CHECK(NativeHashMap_put(map, NULL, NULL));
  CHECK(NativeHashMap_containsKey(map, NULL));
  out = (MCObject *)first;
  CHECK(NativeHashMap_putWithPrevious(map, NULL, (MCObject *)query, &out) &&
        out == NULL);
  CHECK(NativeHashMap_putWithPrevious(map, NULL, NULL, &out) &&
        out == (MCObject *)query);
  CHECK(first->equals == 0);
  MCObjectHeap_free(heap);
  heap = MCObjectHeap_new(1u << 20);
  map = NativeHashMap_newWithKeys(heap, &keyMethods, NULL);
  first = (Key *)MCObjectHeap_alloc(heap, sizeof(*first), &keyClass);
  CHECK(first);
  first->reject = true;
  out = (MCObject *)first;
  CHECK(!NativeHashMap_putWithPrevious(map, (MCObject *)first, NULL, &out) &&
        out == (MCObject *)first && first->hashes == 1 &&
        MCObjectHeap_failed(heap));
  MCObjectHeap_free(heap);
  heap = MCObjectHeap_new(1u << 20);
  map = NativeHashMap_new(heap, NATIVE_HASH_KEY_IDENTITY);
  CHECK(map);
  CHECK(!NativeHashMap_putWithPrevious(map, NULL, NULL, NULL) &&
        MCObjectHeap_failed(heap));
  MCObjectHeap_free(heap);
  heap = MCObjectHeap_new(1u << 20);
  map = NativeHashMap_new(heap, NATIVE_HASH_KEY_STRING);
  MCObjectHeap *foreign = MCObjectHeap_new(1u << 20);
  key = NBTString_fromASCII(foreign, "x");
  CHECK(!NativeHashMap_put(map, (MCObject *)key, NULL) &&
        MCObjectHeap_failed(heap) && !MCObjectHeap_failed(foreign));
  MCObjectHeap_free(heap);
  MCObjectHeap_free(foreign);
  heap = MCObjectHeap_new(1u << 20);
  map = NativeHashMap_new(heap, NATIVE_HASH_KEY_IDENTITY);
  first = (Key *)MCObjectHeap_alloc(heap, sizeof(*first), &keyClass);
  query = (Key *)MCObjectHeap_alloc(heap, sizeof(*query), &keyClass);
  CHECK(NativeHashMap_put(map, (MCObject *)first, NULL) &&
        NativeHashMap_put(map, (MCObject *)query, NULL) &&
        NativeHashMap_size(map) == 2);
  CHECK(!NativeHashMap_containsKey(map, NULL));
  MCObjectHeap_free(heap);
  heap = MCObjectHeap_new(1u << 20);
  map = NativeHashMap_newWithKeys(heap, &keyMethods, NULL);
  for (int i = 0; i < 10; i++) {
    first = (Key *)MCObjectHeap_alloc(heap, sizeof(*first), &keyClass);
    first->value = i;
    CHECK(NativeHashMap_put(map, (MCObject *)first, NULL));
  }
  first = (Key *)MCObjectHeap_alloc(heap, sizeof(*first), &keyClass);
  first->value = 10;
  CHECK(!NativeHashMap_put(map, (MCObject *)first, NULL) &&
        MCObjectHeap_failed(heap));
  MCObjectHeap_free(heap);
  printf("native hash map: %d checks\n", checks);
  return 0;
}
