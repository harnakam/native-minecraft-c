#include "util/NativeHashMap.h"
#include <limits.h>
#include <string.h>
typedef struct Node {
  MCObject object;
  uint32_t hash;
  MCObject *key, *value;
  struct Node *next;
} Node;
typedef struct Table {
  MCObject object;
  int32_t capacity;
  Node *buckets[];
} Table;
struct NativeHashMap {
  MCObject object;
  NativeHashKeyKind kind;
  const NativeHashKeyMethods *methods;
  MCObject *context;
  Table *table;
  int32_t size, threshold;
  uint32_t modCount;
  NativeHashMapView *keys, *values;
};
struct NativeHashMapView {
  MCObject object;
  NativeHashMap *map;
  bool values;
};
struct NativeIterator {
  MCObject object;
  NativeHashMap *map;
  NativeReferenceList *list;
  Node *next;
  int32_t cursor;
  uint32_t expectedModCount;
  bool values;
};
static const MCObjectClass mapClass, nodeClass, tableClass, viewClass,
    iteratorClass;
static bool fail(MCObjectHeap *h) {
  MCObjectHeap_fail(h);
  return false;
}
static bool table_valid(const Table *t) {
  return t && t->object.klass == &tableClass &&
         MCObjectHeap_objectSize((const MCObject *)t) >= sizeof(*t) &&
         t->capacity > 0 && (t->capacity & (t->capacity - 1)) == 0 &&
         (size_t)t->capacity <=
             (MCObjectHeap_objectSize((const MCObject *)t) - sizeof(*t)) /
                 sizeof(Node *);
}
static void node_trace(MCObject *o, MCObjectVisitor v, void *c) {
  if (MCObjectHeap_objectSize(o) < sizeof(Node)) {
    fail(o->heap);
    return;
  }
  Node *n = (Node *)o;
  n->key = v(n->key, c);
  n->value = v(n->value, c);
  n->next = (Node *)v((MCObject *)n->next, c);
}
static void table_trace(MCObject *o, MCObjectVisitor v, void *c) {
  Table *t = (Table *)o;
  if (!table_valid(t)) {
    fail(o->heap);
    return;
  }
  for (int32_t i = 0; i < t->capacity; i++)
    t->buckets[i] = (Node *)v((MCObject *)t->buckets[i], c);
}
static void map_trace(MCObject *o, MCObjectVisitor v, void *c) {
  if (MCObjectHeap_objectSize(o) < sizeof(NativeHashMap)) {
    fail(o->heap);
    return;
  }
  NativeHashMap *m = (NativeHashMap *)o;
  m->context = v(m->context, c);
  m->table = (Table *)v((MCObject *)m->table, c);
  m->keys = (NativeHashMapView *)v((MCObject *)m->keys, c);
  m->values = (NativeHashMapView *)v((MCObject *)m->values, c);
}
static void view_trace(MCObject *o, MCObjectVisitor v, void *c) {
  if (MCObjectHeap_objectSize(o) < sizeof(NativeHashMapView)) {
    fail(o->heap);
    return;
  }
  NativeHashMapView *x = (NativeHashMapView *)o;
  x->map = (NativeHashMap *)v((MCObject *)x->map, c);
}
static void iterator_trace(MCObject *o, MCObjectVisitor v, void *c) {
  if (MCObjectHeap_objectSize(o) < sizeof(NativeIterator)) {
    fail(o->heap);
    return;
  }
  NativeIterator *i = (NativeIterator *)o;
  i->map = (NativeHashMap *)v((MCObject *)i->map, c);
  i->list = (NativeReferenceList *)v((MCObject *)i->list, c);
  i->next = (Node *)v((MCObject *)i->next, c);
}
static const MCObjectClass mapClass = {
    "native.HashMap", MCObjectHeap_plainClone, map_trace, NULL};
static const MCObjectClass nodeClass = {
    "native.HashMap.Node", MCObjectHeap_plainClone, node_trace, NULL};
static const MCObjectClass tableClass = {
    "native.HashMap.Table", MCObjectHeap_plainClone, table_trace, NULL};
static const MCObjectClass viewClass = {
    "native.HashMap.View", MCObjectHeap_plainClone, view_trace, NULL};
static const MCObjectClass iteratorClass = {"native.Collection.Iterator",
                                            MCObjectHeap_plainClone,
                                            iterator_trace, NULL};
bool NativeHashMap_isInstance(const MCObject *o) {
  return o && o->klass == &mapClass &&
         MCObjectHeap_objectSize(o) >= sizeof(NativeHashMap);
}
bool NativeHashMapView_isInstance(const MCObject *o) {
  return o && o->klass == &viewClass &&
         MCObjectHeap_objectSize(o) >= sizeof(NativeHashMapView);
}
bool NativeIterator_isInstance(const MCObject *o) {
  return o && o->klass == &iteratorClass &&
         MCObjectHeap_objectSize(o) >= sizeof(NativeIterator);
}
static bool valid(const NativeHashMap *m) {
  MCObjectHeap *h = m ? m->object.heap : NULL;
  return NativeHashMap_isInstance((const MCObject *)m) &&
                 !MCObjectHeap_failed(h) && m->size >= 0 &&
                 (!m->table
                      ? m->size == 0
                      : table_valid(m->table) && m->table->object.heap == h) &&
                 (!m->context || m->context->heap == h)
             ? true
             : fail(h);
}
static bool node_valid(NativeHashMap *m, Node *n) {
  return n && n->object.klass == &nodeClass &&
                 n->object.heap == m->object.heap &&
                 MCObjectHeap_objectSize((MCObject *)n) >= sizeof(*n)
             ? true
             : fail(m->object.heap);
}
static bool begin(NativeHashMap *m, MCObjectRootScope *s, MCObject *k,
                  MCObject *v) {
  if (!valid(m) || !MCObjectRootScope_begin(s, m->object.heap))
    return false;
  if (MCObjectRootScope_pin(s, (MCObject *)m) && MCObjectRootScope_pin(s, k) &&
      MCObjectRootScope_pin(s, v))
    return true;
  MCObjectRootScope_end(s);
  return false;
}
NativeHashMap *NativeHashMap_new(MCObjectHeap *h, NativeHashKeyKind kind) {
  if (kind != NATIVE_HASH_KEY_STRING && kind != NATIVE_HASH_KEY_IDENTITY) {
    fail(h);
    return NULL;
  }
  NativeHashMap *m =
      (NativeHashMap *)MCObjectHeap_alloc(h, sizeof(*m), &mapClass);
  if (m)
    m->kind = kind;
  return m;
}
NativeHashMap *NativeHashMap_newWithKeys(MCObjectHeap *h,
                                         const NativeHashKeyMethods *methods,
                                         MCObject *context) {
  if (!methods || !methods->hashCode || !methods->equals ||
      (context && context->heap != h)) {
    fail(h);
    return NULL;
  }
  NativeHashMap *m = NativeHashMap_new(h, NATIVE_HASH_KEY_IDENTITY);
  if (m) {
    m->methods = methods;
    m->context = context;
  }
  return m;
}
int32_t NativeHashMap_size(const NativeHashMap *m) {
  return valid(m) ? m->size : -1;
}
static bool hash(NativeHashMap *m, MCObject *key, uint32_t *out) {
  int32_t h = 0;
  if (key) {
    if (key->heap != m->object.heap)
      return fail(m->object.heap);
    if (m->methods) {
      if (!m->methods->hashCode(m->context, key, &h) ||
          MCObjectHeap_failed(m->object.heap))
        return fail(m->object.heap);
    } else if (m->kind == NATIVE_HASH_KEY_STRING) {
      if (!NBTString_isInstance(key))
        return fail(m->object.heap);
      h = NBTString_hashCode((NBTString *)key);
    } else
      h = MCObjectHeap_identityHashCode(key);
  }
  *out = (uint32_t)h ^ ((uint32_t)h >> 16);
  return true;
}
static bool equal(NativeHashMap *m, MCObject *q, MCObject *stored, bool *out) {
  if (q == stored) {
    *out = true;
    return true;
  }
  if (!q) {
    *out = false;
    return true;
  }
  if (stored && stored->heap != m->object.heap)
    return fail(m->object.heap);
  if (m->methods)
    return m->methods->equals(m->context, q, stored, out) &&
                   !MCObjectHeap_failed(m->object.heap)
               ? true
               : fail(m->object.heap);
  *out = m->kind == NATIVE_HASH_KEY_STRING &&
         NBTString_equals((NBTString *)q, (NBTString *)stored);
  return true;
}
static Node *find(NativeHashMap *m, MCObject *key, uint32_t h) {
  if (!m->table)
    return NULL;
  int32_t budget = m->size;
  for (Node *n = m->table->buckets[h & (uint32_t)(m->table->capacity - 1)]; n;
       n = n->next) {
    if (budget-- <= 0 || !node_valid(m, n)) {
      fail(m->object.heap);
      return NULL;
    }
    bool same = false;
    if (n->hash == h) {
      if (!equal(m, key, n->key, &same))
        return NULL;
      if (same)
        return n;
    }
  }
  return NULL;
}
MCObject *NativeHashMap_get(NativeHashMap *m, MCObject *key) {
  MCObjectRootScope s = {0};
  if (!begin(m, &s, key, NULL))
    return NULL;
  uint32_t h;
  Node *n = hash(m, key, &h) ? find(m, key, h) : NULL;
  MCObject *out = n ? n->value : NULL;
  if (out && out->heap != m->object.heap) {
    fail(m->object.heap);
    out = NULL;
  }
  MCObjectRootScope_end(&s);
  return out;
}
bool NativeHashMap_containsKey(NativeHashMap *m, MCObject *key) {
  MCObjectRootScope s = {0};
  if (!begin(m, &s, key, NULL))
    return false;
  uint32_t h;
  bool found = hash(m, key, &h) && find(m, key, h) != NULL;
  MCObjectRootScope_end(&s);
  return found;
}
static bool resize(NativeHashMap *m) {
  int32_t old = m->table ? m->table->capacity : 0;
  if (old >= 1 << 30) {
    m->threshold = INT32_MAX;
    return true;
  }
  int32_t cap = old ? old * 2 : 16;
  if ((size_t)cap > (SIZE_MAX - sizeof(Table)) / sizeof(Node *))
    return fail(m->object.heap);
  Table *t = (Table *)MCObjectHeap_alloc(
      m->object.heap, sizeof(*t) + (size_t)cap * sizeof(Node *), &tableClass);
  if (!t)
    return false;
  t->capacity = cap;
  if (old)
    for (int32_t i = 0; i < old; i++) {
      Node *lo = NULL, *lt = NULL, *hi = NULL, *ht = NULL;
      int32_t budget = m->size;
      for (Node *n = m->table->buckets[i]; n;) {
        if (budget-- <= 0 || !node_valid(m, n))
          return fail(m->object.heap);
        Node *next = n->next;
        if (n->hash & (uint32_t)old) {
          if (ht)
            ht->next = n;
          else
            hi = n;
          ht = n;
        } else {
          if (lt)
            lt->next = n;
          else
            lo = n;
          lt = n;
        }
        n = next;
      }
      if (lt)
        lt->next = NULL;
      if (ht)
        ht->next = NULL;
      t->buckets[i] = lo;
      t->buckets[i + old] = hi;
    }
  m->table = t;
  m->threshold = cap - (cap >> 2);
  MCObjectHeap_touch(m->object.heap);
  return true;
}
bool NativeHashMap_putWithPrevious(NativeHashMap *m, MCObject *key,
                                   MCObject *value, MCObject **previous) {
  if(!previous)return fail(m?m->object.heap:NULL);
  MCObjectRootScope s = {0};
  if (!begin(m, &s, key, value))
    return false;
  bool ok = false;
  MCObject *oldValue=NULL;
  uint32_t h;
  if (!hash(m, key, &h))
    goto done;
  if (!m->table && !resize(m))
    goto done;
  uint32_t index = h & (uint32_t)(m->table->capacity - 1);
  Node *last = NULL;
  int32_t length = 0;
  for (Node *n = m->table->buckets[index]; n; n = n->next) {
    if (++length > m->size || !node_valid(m, n)) {
      fail(m->object.heap);
      goto done;
    }
    bool same = false;
    if (n->hash == h && !equal(m, key, n->key, &same))
      goto done;
    if (same) {
      oldValue=n->value;
      n->value = value;
      MCObjectHeap_touch(m->object.heap);
      ok = true;
      goto done;
    }
    last = n;
  }
  /* Native list bins refuse the unported JDK tree-bin transition. */
  if (length >= 8 && m->table->capacity >= 64) {
    fail(m->object.heap);
    goto done;
  }
  if (m->size == INT32_MAX) {
    fail(m->object.heap);
    goto done;
  }
  Node *n = (Node *)MCObjectHeap_alloc(m->object.heap, sizeof(*n), &nodeClass);
  if (!n)
    goto done;
  n->hash = h;
  n->key = key;
  n->value = value;
  if (last)
    last->next = n;
  else
    m->table->buckets[index] = n;
  ++m->modCount;
  ++m->size;
  MCObjectHeap_touch(m->object.heap);
  if (length >= 8 && !resize(m))
    goto done;
  if (m->size > m->threshold && !resize(m))
    goto done;
  ok = true;
done:
  MCObjectRootScope_end(&s);
  ok=ok && !MCObjectHeap_failed(m->object.heap);
  if(ok)*previous=oldValue;
  return ok;
}
bool NativeHashMap_put(NativeHashMap *m, MCObject *key, MCObject *value) {
  MCObject *previous=NULL;
  return NativeHashMap_putWithPrevious(m,key,value,&previous);
}
static MCObject *remove_node(NativeHashMap *m, MCObject *key, bool byValue) {
  uint32_t h = 0;
  if (!byValue && !hash(m, key, &h))
    return NULL;
  if (!m->table)
    return NULL;
  int32_t start =
              byValue ? 0 : (int32_t)(h & (uint32_t)(m->table->capacity - 1)),
          end = byValue ? m->table->capacity : start + 1;
  for (int32_t j = start; j < end; j++) {
    Node *previous = NULL;
    int32_t budget = m->size;
    for (Node *n = m->table->buckets[j]; n; n = n->next) {
      if (budget-- <= 0 || !node_valid(m, n)) {
        fail(m->object.heap);
        return NULL;
      }
      bool same =
          n->value == key ||
          (key && NBTString_isInstance(key) && NBTString_isInstance(n->value) &&
           NBTString_equals((NBTString *)key, (NBTString *)n->value));
      if (!byValue) {
        same = false;
        if (n->hash == h && !equal(m, key, n->key, &same))
          return NULL;
      }
      if (same) {
        if (previous)
          previous->next = n->next;
        else
          m->table->buckets[j] = n->next;
        --m->size;
        ++m->modCount;
        MCObjectHeap_touch(m->object.heap);
        return byValue ? n->key : n->value;
      }
      previous = n;
    }
  }
  return NULL;
}
MCObject *NativeHashMap_remove(NativeHashMap *m, MCObject *key) {
  MCObjectRootScope s = {0};
  if (!begin(m, &s, key, NULL))
    return NULL;
  MCObject *out = remove_node(m, key, false);
  MCObjectRootScope_end(&s);
  return out;
}
bool NativeHashMap_clear(NativeHashMap *m) {
  if (!valid(m))
    return false;
  ++m->modCount;
  if (m->table && m->size) {
    memset(m->table->buckets, 0, (size_t)m->table->capacity * sizeof(Node *));
    m->size = 0;
  }
  MCObjectHeap_touch(m->object.heap);
  return true;
}
static NativeHashMapView *view(NativeHashMap *m, bool values) {
  MCObjectRootScope s = {0};
  if (!begin(m, &s, NULL, NULL))
    return NULL;
  NativeHashMapView **slot = values ? &m->values : &m->keys;
  if (!*slot) {
    *slot = (NativeHashMapView *)MCObjectHeap_alloc(m->object.heap,
                                                    sizeof(**slot), &viewClass);
    if (*slot) {
      (*slot)->map = m;
      (*slot)->values = values;
      MCObjectHeap_touch(m->object.heap);
    }
  }
  NativeHashMapView *out = *slot;
  MCObjectRootScope_end(&s);
  return out;
}
NativeHashMapView *NativeHashMap_values(NativeHashMap *m) {
  return view(m, true);
}
NativeHashMapView *NativeHashMap_keys(NativeHashMap *m) {
  return view(m, false);
}
static bool view_valid(NativeHashMapView *v) {
  return NativeHashMapView_isInstance((MCObject *)v) && valid(v->map) &&
                 v->map->object.heap == v->object.heap
             ? true
             : fail(v ? v->object.heap : NULL);
}
int32_t NativeHashMapView_size(NativeHashMapView *v) {
  return view_valid(v) ? v->map->size : -1;
}
bool NativeHashMapView_remove(NativeHashMapView *v, MCObject *obj) {
  if (!view_valid(v))
    return false;
  MCObjectRootScope s = {0};
  if (!begin(v->map, &s, obj, NULL))
    return false;
  int32_t old = v->map->size;
  remove_node(v->map, obj, v->values);
  bool changed = old != v->map->size;
  MCObjectRootScope_end(&s);
  return changed && !MCObjectHeap_failed(v->object.heap);
}
bool NativeHashMapView_clear(NativeHashMapView *v) {
  return view_valid(v) && NativeHashMap_clear(v->map);
}
static void advance_bucket(NativeIterator *i) {
  Table *t = i->map->table;
  if (t)
    while (!i->next && i->cursor < t->capacity)
      i->next = t->buckets[i->cursor++];
}
NativeIterator *NativeIterator_fromView(NativeHashMapView *v) {
  if (!view_valid(v))
    return NULL;
  MCObjectRootScope s = {0};
  if (!begin(v->map, &s, (MCObject *)v, NULL))
    return NULL;
  NativeIterator *i = (NativeIterator *)MCObjectHeap_alloc(
      v->object.heap, sizeof(*i), &iteratorClass);
  if (i) {
    i->map = v->map;
    i->values = v->values;
    i->expectedModCount = v->map->modCount;
    advance_bucket(i);
  }
  MCObjectRootScope_end(&s);
  return i;
}
NativeIterator *NativeIterator_fromList(NativeReferenceList *list) {
  if (NativeReferenceList_size(list) < 0)
    return NULL;
  MCObjectRootScope s = {0};
  if (!MCObjectRootScope_begin(&s, list->object.heap))
    return NULL;
  NativeIterator *i = NULL;
  if (MCObjectRootScope_pin(&s, (MCObject *)list)) {
    i = (NativeIterator *)MCObjectHeap_alloc(list->object.heap, sizeof(*i),
                                             &iteratorClass);
    if (i) {
      i->list = list;
      i->expectedModCount = list->modCount;
    }
  }
  MCObjectRootScope_end(&s);
  return i;
}
static bool iterator_valid(NativeIterator *i) {
  if (!NativeIterator_isInstance((MCObject *)i) ||
      MCObjectHeap_failed(i ? i->object.heap : NULL))
    return fail(i ? i->object.heap : NULL);
  if (i->map)
    return valid(i->map) && i->map->object.heap == i->object.heap
               ? true
               : fail(i->object.heap);
  return i->list && i->list->object.heap == i->object.heap &&
                 NativeReferenceList_size(i->list) >= 0
             ? true
             : fail(i->object.heap);
}
bool NativeIterator_hasNext(NativeIterator *i) {
  if (!iterator_valid(i))
    return false;
  return i->map ? i->next != NULL : i->cursor != i->list->size;
}
bool NativeIterator_next(NativeIterator *i, MCObject **out) {
  if (!iterator_valid(i) || !out)
    return fail(i ? i->object.heap : NULL);
  uint32_t mod = i->map ? i->map->modCount : i->list->modCount;
  if (mod != i->expectedModCount)
    return fail(i->object.heap);
  MCObject *value;
  if (i->map) {
    Node *n = i->next;
    if (!node_valid(i->map, n))
      return false;
    value = i->values ? n->value : n->key;
    i->next = n->next;
    advance_bucket(i);
  } else {
    if (i->cursor >= i->list->size || i->cursor < 0)
      return fail(i->object.heap);
    value = NativeReferenceList_get(i->list, i->cursor);
    if (MCObjectHeap_failed(i->object.heap))
      return false;
    ++i->cursor;
  }
  if (value && value->heap != i->object.heap)
    return fail(i->object.heap);
  *out = value;
  MCObjectHeap_touch(i->object.heap);
  return true;
}
