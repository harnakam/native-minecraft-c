#include "entity/ai/attributes/AttributeCollections.h"
#include "entity/ai/attributes/IAttribute.h"
#include <limits.h>
typedef struct {
  MCObject *key, *value;
  uint32_t hash;
  bool occupied;
} Entry;
typedef struct {
  MCObject object;
  int32_t capacity;
  Entry entries[];
} Entries;
struct AttributeNativeMap {
  MCObject object;
  AttributeKeyKind kind;
  bool linked;
  int32_t size, used, tableCapacity;
  Entries *entries;
  AttributeCollection *valuesView;
};
struct AttributeCollection {
  MCObject object;
  AttributeNativeMap *map;
  bool values;
};
typedef struct {
  NBTString *key;
  AttributeModifier *value;
} MultiEntry;
typedef struct {
  MCObject object;
  int32_t capacity;
  MultiEntry entries[];
} MultiEntries;
struct AttributeModifierMultimap {
  MCObject object;
  int32_t size;
  MultiEntries *entries;
};
#define ENTRY_LIMIT 4096
static void entries_trace(MCObject *o, MCObjectVisitor v, void *c) {
  Entries *e = (Entries *)o;
  for (int32_t i = 0; i < e->capacity; i++) {
    e->entries[i].key = v(e->entries[i].key, c);
    e->entries[i].value = v(e->entries[i].value, c);
  }
}
static void map_trace(MCObject *o, MCObjectVisitor v, void *c) {
  AttributeNativeMap *m = (AttributeNativeMap *)o;
  m->entries = (Entries *)v((MCObject *)m->entries, c);
  m->valuesView = (AttributeCollection *)v((MCObject *)m->valuesView, c);
}
static void collection_trace(MCObject *o, MCObjectVisitor v, void *c) {
  AttributeCollection *s = (AttributeCollection *)o;
  s->map = (AttributeNativeMap *)v((MCObject *)s->map, c);
}
static void multi_entries_trace(MCObject *o, MCObjectVisitor v, void *c) {
  MultiEntries *e = (MultiEntries *)o;
  for (int32_t i = 0; i < e->capacity; i++) {
    e->entries[i].key = (NBTString *)v((MCObject *)e->entries[i].key, c);
    e->entries[i].value =
        (AttributeModifier *)v((MCObject *)e->entries[i].value, c);
  }
}
static void multi_trace(MCObject *o, MCObjectVisitor v, void *c) {
  AttributeModifierMultimap *m = (AttributeModifierMultimap *)o;
  m->entries = (MultiEntries *)v((MCObject *)m->entries, c);
}
static const MCObjectClass entriesClass = {
    "native.AttributeMapEntries", MCObjectHeap_plainClone, entries_trace, NULL};
static const MCObjectClass mapClass = {
    "native.AttributeMap", MCObjectHeap_plainClone, map_trace, NULL};
static const MCObjectClass collectionClass = {"native.AttributeCollection",
                                              MCObjectHeap_plainClone,
                                              collection_trace, NULL};
static const MCObjectClass multiEntriesClass = {
    "native.AttributeMultimapEntries", MCObjectHeap_plainClone,
    multi_entries_trace, NULL};
static const MCObjectClass multiClass = {"native.AttributeModifierMultimap",
                                         MCObjectHeap_plainClone, multi_trace,
                                         NULL};
static bool map_valid(AttributeNativeMap *m) {
  if (!m || m->object.klass != &mapClass ||
      MCObjectHeap_objectSize((MCObject *)m) < sizeof *m ||
      MCObjectHeap_failed(m->object.heap)) {
    MCObjectHeap_fail(m ? m->object.heap : NULL);
    return false;
  }
  return true;
}
static bool collection_valid(AttributeCollection *s) {
  if (!s || s->object.klass != &collectionClass ||
      MCObjectHeap_objectSize((MCObject *)s) < sizeof *s ||
      !map_valid(s->map)) {
    MCObjectHeap_fail(s ? s->object.heap : NULL);
    return false;
  }
  return true;
}
static bool key_valid(AttributeNativeMap *m, MCObject *k) {
  if (!k)
    return true;
  if (k->heap != m->object.heap)
    goto fail;
  if (m->kind == ATTRIBUTE_KEY_STRING && !NBTString_isInstance(k))
    goto fail;
  if (m->kind == ATTRIBUTE_KEY_ATTRIBUTE && !IAttribute_isInstance(k))
    goto fail;
  if (m->kind == ATTRIBUTE_KEY_UUID && !NativeJavaUUID_isInstance(k))
    goto fail;
  if (m->kind == ATTRIBUTE_KEY_MODIFIER && !AttributeModifier_isInstance(k))
    goto fail;
  return true;
fail:
  MCObjectHeap_fail(m->object.heap);
  return false;
}
static uint32_t hash_key(AttributeNativeMap *m, MCObject *k) {
  if (!k)
    return 0;
  uint32_t h;
  switch (m->kind) {
  case ATTRIBUTE_KEY_STRING:
    h = (uint32_t)NBTString_hashCode((NBTString *)k);
    break;
  case ATTRIBUTE_KEY_ATTRIBUTE:
    h = (uint32_t)IAttribute_hashCode((IAttribute *)k);
    break;
  case ATTRIBUTE_KEY_MODIFIER:
    h = (uint32_t)AttributeModifier_hashCode((AttributeModifier *)k);
    break;
  case ATTRIBUTE_KEY_UUID: {
    NativeJavaUUID *u = (NativeJavaUUID *)k;
    uint64_t x =
        (uint64_t)u->mostSignificantBits ^ (uint64_t)u->leastSignificantBits;
    h = (uint32_t)x ^ (uint32_t)(x >> 32);
    break;
  }
  default:
    h = (uint32_t)MCObjectHeap_identityHashCode(k);
    break;
  }
  return h ^ (h >> 16);
}
static bool key_equal(AttributeNativeMap *m, MCObject *a, MCObject *b) {
  if (a == b)
    return true;
  if (!a || !b)
    return false;
  switch (m->kind) {
  case ATTRIBUTE_KEY_STRING:
    return NBTString_equals((NBTString *)a, (NBTString *)b);
  case ATTRIBUTE_KEY_ATTRIBUTE:
    return IAttribute_equals((IAttribute *)a, (IAttribute *)b);
  case ATTRIBUTE_KEY_MODIFIER:
    return AttributeModifier_equals((AttributeModifier *)a,
                                    (AttributeModifier *)b);
  case ATTRIBUTE_KEY_UUID: {
    NativeJavaUUID *x = (NativeJavaUUID *)a, *y = (NativeJavaUUID *)b;
    return x->mostSignificantBits == y->mostSignificantBits &&
           x->leastSignificantBits == y->leastSignificantBits;
  }
  default:
    return false;
  }
}
static int32_t find(AttributeNativeMap *m, MCObject *key, uint32_t hash) {
  for (int32_t i = 0; i < m->used; i++) {
    Entry *e = &m->entries->entries[i];
    if (e->occupied && e->hash == hash && key_equal(m, key, e->key))
      return i;
  }
  return -1;
}
AttributeNativeMap *AttributeNativeMap_new(MCObjectHeap *h,
                                           AttributeKeyKind kind, bool linked) {
  if (kind < ATTRIBUTE_KEY_IDENTITY || kind > ATTRIBUTE_KEY_MODIFIER) {
    MCObjectHeap_fail(h);
    return NULL;
  }
  AttributeNativeMap *m =
      (AttributeNativeMap *)MCObjectHeap_alloc(h, sizeof *m, &mapClass);
  if (m) {
    m->kind = kind;
    m->linked = linked;
    m->tableCapacity = 16;
  }
  return m;
}
MCObject *AttributeNativeMap_get(AttributeNativeMap *m, MCObject *key) {
  if (!map_valid(m) || !key_valid(m, key))
    return NULL;
  int32_t i = find(m, key, hash_key(m, key));
  return i >= 0 ? m->entries->entries[i].value : NULL;
}
bool AttributeNativeMap_containsKey(AttributeNativeMap *m, MCObject *key) {
  return map_valid(m) && key_valid(m, key) &&
         find(m, key, hash_key(m, key)) >= 0;
}
bool AttributeNativeMap_put(AttributeNativeMap *m, MCObject *key,
                            MCObject *value) {
  if (!map_valid(m) || !key_valid(m, key) ||
      (value && value->heap != m->object.heap)) {
    MCObjectHeap_fail(m ? m->object.heap : NULL);
    return false;
  }
  MCObjectRootScope s = {0};
  MCObjectHeap *h = m->object.heap;
  if (!MCObjectRootScope_begin(&s, h) ||
      !MCObjectRootScope_pin(&s, (MCObject *)m) ||
      !MCObjectRootScope_pin(&s, key) || !MCObjectRootScope_pin(&s, value))
    goto fail;
  uint32_t hash = hash_key(m, key);
  int32_t i = find(m, key, hash);
  if (i >= 0) {
    m->entries->entries[i].value = value;
    MCObjectHeap_touch(h);
    MCObjectRootScope_end(&s);
    return true;
  }
  int32_t collisions = 0;
  for (int32_t j = 0; j < m->used; j++) {
    Entry *e = &m->entries->entries[j];
    if (e->occupied && (e->hash & (uint32_t)(m->tableCapacity - 1)) ==
                           (hash & (uint32_t)(m->tableCapacity - 1)))
      collisions++;
  }
  if (!m->linked && collisions >= 8 && m->tableCapacity >= 64)
    goto fail;
  if (m->used > m->size * 2 && m->used >= 16) {
    int32_t n = 0;
    for (int32_t j = 0; j < m->used; j++)
      if (m->entries->entries[j].occupied)
        m->entries->entries[n++] = m->entries->entries[j];
    for (int32_t j = n; j < m->used; j++)
      m->entries->entries[j] = (Entry){0};
    m->used = n;
  }
  if (m->used >= ENTRY_LIMIT)
    goto fail;
  if (!m->entries || m->used == m->entries->capacity) {
    int32_t cap = m->entries ? m->entries->capacity * 2 : 16;
    Entries *e = (Entries *)MCObjectHeap_alloc(
        h, sizeof *e + (size_t)cap * sizeof(Entry), &entriesClass);
    if (!e)
      goto fail;
    e->capacity = cap;
    if (m->entries)
      for (int32_t j = 0; j < m->used; j++)
        e->entries[j] = m->entries->entries[j];
    m->entries = e;
  }
  m->entries->entries[m->used++] = (Entry){key, value, hash, true};
  m->size++;
  if (!m->linked && collisions >= 8 && m->tableCapacity < 64)
    m->tableCapacity *= 2;
  if (m->size > m->tableCapacity * 3 / 4)
    m->tableCapacity *= 2;
  MCObjectHeap_touch(h);
  MCObjectRootScope_end(&s);
  return true;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&s);
  return false;
}
bool AttributeNativeMap_remove(AttributeNativeMap *m, MCObject *key) {
  if (!map_valid(m) || !key_valid(m, key))
    return false;
  int32_t i = find(m, key, hash_key(m, key));
  if (i >= 0) {
    m->entries->entries[i].occupied = false;
    m->entries->entries[i].key = NULL;
    m->entries->entries[i].value = NULL;
    m->size--;
    MCObjectHeap_touch(m->object.heap);
  }
  return true;
}
bool AttributeNativeMap_clear(AttributeNativeMap *m) {
  if (!map_valid(m))
    return false;
  for (int32_t i = 0; i < m->used; i++)
    m->entries->entries[i] = (Entry){0};
  m->used = 0;
  m->size = 0;
  MCObjectHeap_touch(m->object.heap);
  return true;
}
AttributeCollection *AttributeNativeMap_values(AttributeNativeMap *m) {
  if (!map_valid(m))
    return NULL;
  if (m->valuesView)
    return m->valuesView;
  MCObjectRootScope s = {0};
  if (!MCObjectRootScope_begin(&s, m->object.heap) ||
      !MCObjectRootScope_pin(&s, (MCObject *)m)) {
    MCObjectRootScope_end(&s);
    return NULL;
  }
  AttributeCollection *c = (AttributeCollection *)MCObjectHeap_alloc(
      m->object.heap, sizeof *c, &collectionClass);
  if (c) {
    c->map = m;
    c->values = true;
    m->valuesView = c;
    MCObjectHeap_touch(m->object.heap);
  }
  MCObjectRootScope_end(&s);
  return c;
}
AttributeCollection *AttributeCollection_newSet(MCObjectHeap *h,
                                                AttributeKeyKind kind) {
  MCObjectRootScope s = {0};
  if (!MCObjectRootScope_begin(&s, h))
    return NULL;
  AttributeCollection *c =
      (AttributeCollection *)MCObjectHeap_alloc(h, sizeof *c, &collectionClass);
  if (c)
    c->map = AttributeNativeMap_new(h, kind, false);
  if (!c || !c->map)
    c = NULL;
  MCObjectRootScope_end(&s);
  return c;
}
AttributeCollection *AttributeCollection_copySet(AttributeCollection *source,
                                                 AttributeKeyKind kind) {
  if (!collection_valid(source))
    return NULL;
  MCObjectRootScope scope = {0};
  MCObjectHeap *h = source->object.heap;
  if (!MCObjectRootScope_begin(&scope, h) ||
      !MCObjectRootScope_pin(&scope, (MCObject *)source))
    goto fail;
  AttributeCollection *copy = AttributeCollection_newSet(h, kind);
  if (!copy)
    goto fail;
  int32_t requested = source->map->size * 4 / 3 + 1, capacity = 16;
  while (capacity < requested)
    capacity *= 2;
  copy->map->tableCapacity = capacity;
  if (!AttributeCollection_addAll(copy, source))
    goto fail;
  MCObjectRootScope_end(&scope);
  return copy;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&scope);
  return NULL;
}
int32_t AttributeCollection_size(const AttributeCollection *s) {
  return collection_valid((AttributeCollection *)s) ? s->map->size : 0;
}
static Entry *entry_at(AttributeNativeMap *m, int32_t index) {
  if (index < 0 || index >= m->size) {
    MCObjectHeap_fail(m->object.heap);
    return NULL;
  }
  if (m->linked) {
    for (int32_t i = 0; i < m->used; i++)
      if (m->entries->entries[i].occupied && index-- == 0)
        return &m->entries->entries[i];
  } else
    for (int32_t bucket = 0; bucket < m->tableCapacity; bucket++)
      for (int32_t i = 0; i < m->used; i++) {
        Entry *e = &m->entries->entries[i];
        if (e->occupied &&
            (int32_t)(e->hash & (uint32_t)(m->tableCapacity - 1)) == bucket &&
            index-- == 0)
          return e;
      }
  return NULL;
}
MCObject *AttributeCollection_getAt(AttributeCollection *s, int32_t index) {
  if (!collection_valid(s))
    return NULL;
  Entry *e = entry_at(s->map, index);
  return e ? (s->values ? e->value : e->key) : NULL;
}
bool AttributeCollection_contains(AttributeCollection *s, MCObject *o) {
  if (!collection_valid(s))
    return false;
  if (!s->values)
    return AttributeNativeMap_containsKey(s->map, o);
  for (int32_t i = 0; i < s->map->used; i++)
    if (s->map->entries->entries[i].occupied &&
        s->map->entries->entries[i].value == o)
      return true;
  return false;
}
bool AttributeCollection_add(AttributeCollection *s, MCObject *o) {
  if (!collection_valid(s))
    return false;
  if (s->values) {
    MCObjectHeap_fail(s->object.heap);
    return false;
  }
  if (AttributeNativeMap_containsKey(s->map, o))
    return true;
  return AttributeNativeMap_put(s->map, o, o);
}
bool AttributeCollection_remove(AttributeCollection *s, MCObject *o) {
  if (!collection_valid(s))
    return false;
  if (!s->values)
    return AttributeNativeMap_remove(s->map, o);
  for (int32_t i = 0; i < s->map->size; i++) {
    Entry *e = entry_at(s->map, i);
    if (e && e->value == o)
      return AttributeNativeMap_remove(s->map, e->key);
  }
  return true;
}
bool AttributeCollection_clear(AttributeCollection *s) {
  return collection_valid(s) && AttributeNativeMap_clear(s->map);
}
bool AttributeCollection_addAll(AttributeCollection *s,
                                AttributeCollection *other) {
  if (!collection_valid(s) || !collection_valid(other) ||
      s->object.heap != other->object.heap) {
    MCObjectHeap_fail(s ? s->object.heap : NULL);
    return false;
  }
  for (int32_t i = 0; i < AttributeCollection_size(other); i++)
    if (!AttributeCollection_add(s, AttributeCollection_getAt(other, i)))
      return false;
  return true;
}
AttributeModifierMultimap *AttributeModifierMultimap_new(MCObjectHeap *h) {
  return (AttributeModifierMultimap *)MCObjectHeap_alloc(
      h, sizeof(AttributeModifierMultimap), &multiClass);
}
static bool multi_valid(AttributeModifierMultimap *m) {
  if (!m || m->object.klass != &multiClass ||
      MCObjectHeap_objectSize((MCObject *)m)<sizeof *m ||
      MCObjectHeap_failed(m->object.heap)) {
    MCObjectHeap_fail(m ? m->object.heap : NULL);
    return false;
  }
  return true;
}
bool AttributeModifierMultimap_put(AttributeModifierMultimap *m, NBTString *key,
                                   AttributeModifier *value) {
  if (!multi_valid(m) || !NBTString_isInstance((MCObject *)key) ||
      !AttributeModifier_isInstance((MCObject *)value) ||
      ((MCObject *)key)->heap != m->object.heap ||
      value->object.heap != m->object.heap || m->size >= ENTRY_LIMIT) {
    MCObjectHeap_fail(m ? m->object.heap : NULL);
    return false;
  }
  MCObjectRootScope s = {0};
  MCObjectHeap *h = m->object.heap;
  if (!MCObjectRootScope_begin(&s, h) ||
      !MCObjectRootScope_pin(&s, (MCObject *)m) ||
      !MCObjectRootScope_pin(&s, (MCObject *)key) ||
      !MCObjectRootScope_pin(&s, (MCObject *)value))
    goto fail;
  if (!m->entries || m->size == m->entries->capacity) {
    int32_t cap = m->entries ? m->entries->capacity * 2 : 16;
    MultiEntries *e = (MultiEntries *)MCObjectHeap_alloc(
        h, sizeof *e + (size_t)cap * sizeof(MultiEntry), &multiEntriesClass);
    if (!e)
      goto fail;
    e->capacity = cap;
    if (m->entries)
      for (int32_t i = 0; i < m->size; i++)
        e->entries[i] = m->entries->entries[i];
    m->entries = e;
  }
  m->entries->entries[m->size++] = (MultiEntry){key, value};
  MCObjectHeap_touch(h);
  MCObjectRootScope_end(&s);
  return true;
fail:
  MCObjectHeap_fail(h);
  MCObjectRootScope_end(&s);
  return false;
}
int32_t AttributeModifierMultimap_size(const AttributeModifierMultimap *m) {
  return multi_valid((AttributeModifierMultimap *)m) ? m->size : 0;
}
NBTString *AttributeModifierMultimap_keyAt(AttributeModifierMultimap *m,
                                           int32_t i) {
  if (!multi_valid(m) || i < 0 || i >= m->size) {
    MCObjectHeap_fail(m ? m->object.heap : NULL);
    return NULL;
  }
  return m->entries->entries[i].key;
}
AttributeModifier *
AttributeModifierMultimap_valueAt(AttributeModifierMultimap *m, int32_t i) {
  if (!multi_valid(m) || i < 0 || i >= m->size) {
    MCObjectHeap_fail(m ? m->object.heap : NULL);
    return NULL;
  }
  return m->entries->entries[i].value;
}
