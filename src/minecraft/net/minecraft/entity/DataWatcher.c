#include "entity/DataWatcher.h"
#include <limits.h>
#include <math.h>
#include <string.h>

typedef struct {
    MCObject object;
    union {
        int32_t integer;
        float real;
    } value;
} Box;
static const MCObjectClass boxClass[4] = {
    {"native.java.lang.Byte", MCObjectHeap_plainClone, NULL, NULL},
    {"native.java.lang.Short", MCObjectHeap_plainClone, NULL, NULL},
    {"native.java.lang.Integer", MCObjectHeap_plainClone, NULL, NULL},
    {"native.java.lang.Float", MCObjectHeap_plainClone, NULL, NULL}};
static const MCObjectClass rotationClass = {"native.Rotations.value", MCObjectHeap_plainClone, NULL,
                                            NULL};
static int32_t signed_bits(uint32_t n) {
    int32_t out;
    memcpy(&out, &n, sizeof(out));
    return out;
}
static int32_t value_type(const MCObject *v) {
    if (!v)
        return -1;
    for (int i = 0; i < 4; i++)
        if (v->klass == &boxClass[i])
            return i;
    if (NBTString_isInstance(v))
        return 4;
    if (ItemStack_isInstance(v))
        return 5;
    if (BlockPos_isRuntimeClass(v))
        return 6;
    if (v->klass == &rotationClass)
        return 7;
    return -1;
}
static MCObject *box(MCObjectHeap *h, int type, int32_t n) {
    Box *v = (Box *)MCObjectHeap_alloc(h, sizeof(*v), &boxClass[type]);
    if (v)
        v->value.integer = n;
    return (MCObject *)v;
}
MCObject *DataWatcher_boxByte(MCObjectHeap *h, int32_t n) {
    uint8_t bits = (uint8_t)n;
    int8_t value;
    memcpy(&value, &bits, 1);
    return box(h, 0, value);
}
MCObject *DataWatcher_boxShort(MCObjectHeap *h, int32_t n) {
    uint16_t bits = (uint16_t)n;
    int16_t value;
    memcpy(&value, &bits, 2);
    return box(h, 1, value);
}
MCObject *DataWatcher_boxInt(MCObjectHeap *h, int32_t n) { return box(h, 2, n); }
MCObject *DataWatcher_boxFloat(MCObjectHeap *h, float n) {
    Box *v = (Box *)MCObjectHeap_alloc(h, sizeof(*v), &boxClass[3]);
    if (v)
        v->value.real = n;
    return (MCObject *)v;
}
bool DataWatcher_blockPosIsInstance(const MCObject *o) { return BlockPos_isInstance(o); }
DataWatcherBlockPos *DataWatcher_blockPos(MCObjectHeap *h, int32_t x, int32_t y, int32_t z) {
    return BlockPos_newInt(h, x, y, z);
}
DataWatcherRotations *DataWatcher_rotations(MCObjectHeap *h, float x, float y, float z) {
    DataWatcherRotations *p =
        (DataWatcherRotations *)MCObjectHeap_alloc(h, sizeof(*p), &rotationClass);
    if (p) {
        p->x = x;
        p->y = y;
        p->z = z;
    }
    return p;
}
struct WatchableObject {
    MCObject object;
    int32_t objectType, dataValueId;
    MCObject *watchedObject;
    bool watched;
};
static void watch_trace(MCObject *o, MCObjectVisitor v, void *c) {
    WatchableObject *w = (WatchableObject *)o;
    w->watchedObject = v(w->watchedObject, c);
}
static const MCObjectClass watchClass = {"net.minecraft.entity.DataWatcher.WatchableObject",
                                         MCObjectHeap_plainClone, watch_trace, NULL};
WatchableObject *WatchableObject_new(MCObjectHeap *h, int32_t type, int32_t id, MCObject *value) {
    if (value && value->heap != h) {
        MCObjectHeap_fail(h);
        return NULL;
    }
    WatchableObject *w = (WatchableObject *)MCObjectHeap_alloc(h, sizeof(*w), &watchClass);
    if (w) {
        w->objectType = type;
        w->dataValueId = id;
        w->watchedObject = value;
        w->watched = true;
    }
    return w;
}
int32_t WatchableObject_getDataValueId(const WatchableObject *w) { return w->dataValueId; }
bool WatchableObject_setObject(WatchableObject *w, MCObject *v) {
    if (!w)
        return false;
    if (v && v->heap != w->object.heap) {
        MCObjectHeap_fail(w->object.heap);
        return false;
    }
    if (MCObjectHeap_failed(w->object.heap))
        return false;
    w->watchedObject = v;
    MCObjectHeap_touch(w->object.heap);
    return true;
}
MCObject *WatchableObject_getObject(const WatchableObject *w) { return w->watchedObject; }
int32_t WatchableObject_getObjectType(const WatchableObject *w) { return w->objectType; }
bool WatchableObject_isWatched(const WatchableObject *w) { return w->watched; }
void WatchableObject_setWatched(WatchableObject *w, bool flag) {
    w->watched = flag;
    MCObjectHeap_touch(w->object.heap);
}
typedef struct {
    MCObject object;
    int32_t capacity;
    WatchableObject *items[];
} ListStorage;
struct WatchableObjectList {
    MCObject object;
    ListStorage *storage;
    int32_t size;
    uint32_t modCount;
};
static void storage_trace(MCObject *o, MCObjectVisitor v, void *c) {
    ListStorage *s = (ListStorage *)o;
    for (int32_t i = 0; i < s->capacity; i++)
        s->items[i] = (WatchableObject *)v((MCObject *)s->items[i], c);
}
static void list_trace(MCObject *o, MCObjectVisitor v, void *c) {
    WatchableObjectList *l = (WatchableObjectList *)o;
    l->storage = (ListStorage *)v((MCObject *)l->storage, c);
}
static const MCObjectClass storageClass = {"native.WatchableObject[].storage",
                                           MCObjectHeap_plainClone, storage_trace, NULL};
static const MCObjectClass listClass = {"native.ArrayList<WatchableObject>",
                                        MCObjectHeap_plainClone, list_trace, NULL};
WatchableObjectList *WatchableObjectList_new(MCObjectHeap *h) {
    return (WatchableObjectList *)MCObjectHeap_alloc(h, sizeof(WatchableObjectList), &listClass);
}
int32_t WatchableObjectList_size(const WatchableObjectList *l) { return l ? l->size : 0; }
WatchableObject *WatchableObjectList_get(WatchableObjectList *l, int32_t i) {
    if (!l || i < 0 || i >= l->size) {
        MCObjectHeap_fail(l ? l->object.heap : NULL);
        return NULL;
    }
    return l->storage->items[i];
}
static bool list_room(WatchableObjectList *l) {
    if (l->storage && l->size < l->storage->capacity)
        return true;
    int64_t cap = l->storage ? (int64_t)l->storage->capacity * 3 / 2 : 10;
    if (cap <= l->size)
        cap = (int64_t)l->size + 1;
    if (cap > INT32_MAX ||
        (uint64_t)cap > (SIZE_MAX - sizeof(ListStorage)) / sizeof(WatchableObject *)) {
        MCObjectHeap_fail(l->object.heap);
        return false;
    }
    ListStorage *s = (ListStorage *)MCObjectHeap_alloc(
        l->object.heap, sizeof(*s) + (size_t)cap * sizeof(*s->items), &storageClass);
    if (!s)
        return false;
    s->capacity = (int32_t)cap;
    if (l->size)
        memcpy(s->items, l->storage->items, (size_t)l->size * sizeof(*s->items));
    l->storage = s;
    return true;
}
bool WatchableObjectList_add(WatchableObjectList *l, WatchableObject *w) {
    if (!l)
        return false;
    if ((w && w->object.heap != l->object.heap) || l->size == INT32_MAX) {
        MCObjectHeap_fail(l->object.heap);
        return false;
    }
    if (MCObjectHeap_failed(l->object.heap) || !list_room(l))
        return false;
    l->storage->items[l->size++] = w;
    l->modCount++;
    MCObjectHeap_touch(l->object.heap);
    return true;
}
bool WatchableObjectList_set(WatchableObjectList *l, int32_t i, WatchableObject *w) {
    if (!l)
        return false;
    if (i < 0 || i >= l->size || (w && w->object.heap != l->object.heap)) {
        MCObjectHeap_fail(l->object.heap);
        return false;
    }
    if (MCObjectHeap_failed(l->object.heap))
        return false;
    l->storage->items[i] = w;
    MCObjectHeap_touch(l->object.heap);
    return true;
}
WatchableObject *WatchableObjectList_remove(WatchableObjectList *l, int32_t i) {
    if (!l)
        return NULL;
    if (i < 0 || i >= l->size) {
        MCObjectHeap_fail(l->object.heap);
        return NULL;
    }
    if (MCObjectHeap_failed(l->object.heap))
        return NULL;
    WatchableObject *old = l->storage->items[i];
    l->size--;
    memmove(l->storage->items + i, l->storage->items + i + 1, (size_t)(l->size - i) * sizeof(old));
    l->storage->items[l->size] = NULL;
    l->modCount++;
    MCObjectHeap_touch(l->object.heap);
    return old;
}
void WatchableObjectList_clear(WatchableObjectList *l) {
    if (!l || MCObjectHeap_failed(l->object.heap))
        return;
    if (l->size)
        memset(l->storage->items, 0, (size_t)l->size * sizeof(*l->storage->items));
    l->size = 0;
    l->modCount++;
    MCObjectHeap_touch(l->object.heap);
}

/* Native Integer-key HashMap dependency. Hash spreading, bucket chains,
   resizing and red/black tree-bin traversal preserve the JDK8 values order.
   Distinct Integer keys have distinct spread hashes, so no identity/comparable
   tie-break dependency is needed. Managed nodes retain direct value refs. */
typedef struct MapNode {
    MCObject object;
    uint32_t hash;
    int32_t id;
    WatchableObject *value;
    struct MapNode *next, *previous, *left, *right, *parent;
    bool tree, red;
} MapNode;
typedef struct {
    MCObject object;
    int32_t capacity;
    MapNode *buckets[];
} Table;
struct DataWatcher {
    MCObject object;
    MCObject *owner, *context;
    const DataWatcherDependencies *dependencies;
    bool isBlank, objectChanged;
    Table *table;
    int32_t size, threshold;
    uint32_t modCount;
};
static void node_trace(MCObject *o, MCObjectVisitor v, void *c) {
    MapNode *n = (MapNode *)o;
    n->value = (WatchableObject *)v((MCObject *)n->value, c);
    n->next = (MapNode *)v((MCObject *)n->next, c);
    n->previous = (MapNode *)v((MCObject *)n->previous, c);
    n->left = (MapNode *)v((MCObject *)n->left, c);
    n->right = (MapNode *)v((MCObject *)n->right, c);
    n->parent = (MapNode *)v((MCObject *)n->parent, c);
}
static void table_trace(MCObject *o, MCObjectVisitor v, void *c) {
    Table *t = (Table *)o;
    for (int32_t i = 0; i < t->capacity; i++)
        t->buckets[i] = (MapNode *)v((MCObject *)t->buckets[i], c);
}
static void watcher_trace(MCObject *o, MCObjectVisitor v, void *c) {
    DataWatcher *w = (DataWatcher *)o;
    w->owner = v(w->owner, c);
    w->context = v(w->context, c);
    w->table = (Table *)v((MCObject *)w->table, c);
}
static const MCObjectClass nodeClass = {"native.HashMap<Integer,WatchableObject>.node",
                                        MCObjectHeap_plainClone, node_trace, NULL};
static const MCObjectClass tableClass = {"native.HashMap<Integer,WatchableObject>.table",
                                         MCObjectHeap_plainClone, table_trace, NULL};
static const MCObjectClass watcherClass = {"net.minecraft.entity.DataWatcher",
                                           MCObjectHeap_plainClone, watcher_trace, NULL};
bool DataWatcher_isInstance(const MCObject *o) { return o && o->klass == &watcherClass; }
static uint32_t hash(int32_t id) {
    uint32_t n = (uint32_t)id;
    return n ^ (n >> 16);
}
static MapNode *find(DataWatcher *w, int32_t id) {
    if (!w->table)
        return NULL;
    uint32_t hv = hash(id);
    for (MapNode *n = w->table->buckets[hv & (uint32_t)(w->table->capacity - 1)]; n; n = n->next)
        if (n->id == id)
            return n;
    return NULL;
}
static MapNode *rotate_left(MapNode *root, MapNode *n) {
    MapNode *r = n->right;
    if (r) {
        n->right = r->left;
        if (r->left)
            r->left->parent = n;
        r->parent = n->parent;
        if (!n->parent) {
            root = r;
            r->red = false;
        } else if (n->parent->left == n)
            n->parent->left = r;
        else
            n->parent->right = r;
        r->left = n;
        n->parent = r;
    }
    return root;
}
static MapNode *rotate_right(MapNode *root, MapNode *n) {
    MapNode *l = n->left;
    if (l) {
        n->left = l->right;
        if (l->right)
            l->right->parent = n;
        l->parent = n->parent;
        if (!n->parent) {
            root = l;
            l->red = false;
        } else if (n->parent->right == n)
            n->parent->right = l;
        else
            n->parent->left = l;
        l->right = n;
        n->parent = l;
    }
    return root;
}
static MapNode *balance(MapNode *root, MapNode *x) {
    x->red = true;
    for (;;) {
        MapNode *p = x->parent;
        if (!p) {
            x->red = false;
            return x;
        }
        if (!p->red || !p->parent)
            return root;
        MapNode *g = p->parent;
        if (p == g->left) {
            MapNode *u = g->right;
            if (u && u->red) {
                u->red = false;
                p->red = false;
                g->red = true;
                x = g;
            } else {
                if (x == p->right) {
                    root = rotate_left(root, p);
                    x = p;
                    p = x->parent;
                    g = p ? p->parent : NULL;
                }
                if (p) {
                    p->red = false;
                    if (g) {
                        g->red = true;
                        root = rotate_right(root, g);
                    }
                }
            }
        } else {
            MapNode *u = g->left;
            if (u && u->red) {
                u->red = false;
                p->red = false;
                g->red = true;
                x = g;
            } else {
                if (x == p->left) {
                    root = rotate_right(root, p);
                    x = p;
                    p = x->parent;
                    g = p ? p->parent : NULL;
                }
                if (p) {
                    p->red = false;
                    if (g) {
                        g->red = true;
                        root = rotate_left(root, g);
                    }
                }
            }
        }
    }
}
static void root_front(Table *t, uint32_t bucket, MapNode *root) {
    MapNode *first = t->buckets[bucket];
    if (root && root != first) {
        if (root->next)
            root->next->previous = root->previous;
        if (root->previous)
            root->previous->next = root->next;
        if (first)
            first->previous = root;
        root->next = first;
        root->previous = NULL;
        t->buckets[bucket] = root;
    }
}
static void treeify(Table *t, uint32_t bucket) {
    MapNode *root = NULL;
    for (MapNode *n = t->buckets[bucket]; n; n = n->next) {
        n->tree = true;
        n->left = n->right = NULL;
        if (!root) {
            n->parent = NULL;
            n->red = false;
            root = n;
        } else {
            MapNode *p = root;
            for (;;) {
                bool left = signed_bits(n->hash) < signed_bits(p->hash);
                MapNode *next = left ? p->left : p->right;
                if (next) {
                    p = next;
                    continue;
                }
                n->parent = p;
                if (left)
                    p->left = n;
                else
                    p->right = n;
                root = balance(root, n);
                break;
            }
        }
    }
    root_front(t, bucket, root);
}
static void untree(MapNode *n) {
    for (; n; n = n->next) {
        n->tree = false;
        n->left = n->right = n->parent = NULL;
        n->red = false;
    }
}
static bool resize(DataWatcher *w) {
    int32_t oldcap = w->table ? w->table->capacity : 0;
    if (oldcap >= (1 << 30)) {
        w->threshold = INT32_MAX;
        return true;
    }
    int32_t cap = oldcap ? oldcap * 2 : 16;
    if ((size_t)cap > (SIZE_MAX - sizeof(Table)) / sizeof(MapNode *)) {
        MCObjectHeap_fail(w->object.heap);
        return false;
    }
    Table *t = (Table *)MCObjectHeap_alloc(
        w->object.heap, sizeof(*t) + (size_t)cap * sizeof(*t->buckets), &tableClass);
    if (!t)
        return false;
    t->capacity = cap;
    if (w->table)
        for (int32_t i = 0; i < oldcap; i++) {
            MapNode *head = w->table->buckets[i];
            if (!head)
                continue;
            MapNode *low = NULL, *lt = NULL, *high = NULL, *ht = NULL;
            unsigned lc = 0, hc = 0;
            bool wasTree = head->tree;
            for (MapNode *n = head; n;) {
                MapNode *next = n->next;
                n->next = NULL;
                bool upper = (n->hash & (uint32_t)oldcap) != 0;
                MapNode **tail = upper ? &ht : &lt, **first = upper ? &high : &low;
                n->previous = *tail;
                if (*tail)
                    (*tail)->next = n;
                else
                    *first = n;
                *tail = n;
                if (upper)
                    hc++;
                else
                    lc++;
                n = next;
            }
            t->buckets[i] = low;
            t->buckets[i + oldcap] = high;
            if (wasTree) {
                if (lc <= 6)
                    untree(low);
                else if (high)
                    treeify(t, (uint32_t)i);
                if (hc <= 6)
                    untree(high);
                else if (low)
                    treeify(t, (uint32_t)(i + oldcap));
            }
        }
    w->table = t;
    w->threshold = cap >= (1 << 30) ? INT32_MAX : (cap / 4) * 3;
    MCObjectHeap_touch(w->object.heap);
    return true;
}
static bool put(DataWatcher *w, int32_t id, WatchableObject *value) {
    MapNode *old = find(w, id);
    if (old) {
        old->value = value;
        MCObjectHeap_touch(w->object.heap);
        return true;
    }
    if (!w->table && !resize(w))
        return false;
    uint32_t hv = hash(id), bucket = hv & (uint32_t)(w->table->capacity - 1);
    MapNode *n = (MapNode *)MCObjectHeap_alloc(w->object.heap, sizeof(*n), &nodeClass);
    if (!n)
        return false;
    n->hash = hv;
    n->id = id;
    n->value = value;
    MapNode *head = w->table->buckets[bucket];
    if (!head)
        w->table->buckets[bucket] = n;
    else if (head->tree) {
        MapNode *root = head;
        while (root->parent)
            root = root->parent;
        MapNode *p = root;
        for (;;) {
            bool left = signed_bits(hv) < signed_bits(p->hash);
            MapNode *next = left ? p->left : p->right;
            if (next) {
                p = next;
                continue;
            }
            n->tree = true;
            n->parent = p;
            if (left)
                p->left = n;
            else
                p->right = n;
            n->next = p->next;
            n->previous = p;
            if (p->next)
                p->next->previous = n;
            p->next = n;
            root = balance(root, n);
            root_front(w->table, bucket, root);
            break;
        }
    } else {
        unsigned count = 1;
        MapNode *tail = head;
        while (tail->next) {
            tail = tail->next;
            count++;
        }
        tail->next = n;
        n->previous = tail;
        if (count >= 8) {
            if (w->table->capacity < 64) {
                if (!resize(w))
                    return false;
            } else
                treeify(w->table, bucket);
        }
    }
    w->size++;
    w->modCount++;
    if (w->size > w->threshold && !resize(w))
        return false;
    MCObjectHeap_touch(w->object.heap);
    return true;
}
DataWatcher *DataWatcher_new(MCObjectHeap *h, MCObject *owner, const DataWatcherDependencies *d,
                             MCObject *context) {
    if (!d || !d->onDataWatcherUpdate || (owner && owner->heap != h) ||
        (context && context->heap != h)) {
        MCObjectHeap_fail(h);
        return NULL;
    }
    DataWatcher *w = (DataWatcher *)MCObjectHeap_alloc(h, sizeof(*w), &watcherClass);
    if (w) {
        w->owner = owner;
        w->context = context;
        w->dependencies = d;
        w->isBlank = true;
    }
    return w;
}
static bool begin(DataWatcher *w, MCObjectRootScope *scope) {
    if (!w || MCObjectHeap_failed(w->object.heap) ||
        !MCObjectRootScope_begin(scope, w->object.heap))
        return false;
    if (MCObjectRootScope_pin(scope, (MCObject *)w))
        return true;
    MCObjectRootScope_end(scope);
    return false;
}
bool DataWatcher_addObject(DataWatcher *w, int32_t id, MCObject *value) {
    MCObjectRootScope scope = {0};
    if (!begin(w, &scope))
        return false;
    bool ok = MCObjectRootScope_pin(&scope, value);
    int type = ok ? value_type(value) : -1;
    ok = ok && type >= 0 && id <= 31 && !find(w, id);
    WatchableObject *object = ok ? WatchableObject_new(w->object.heap, type, id, value) : NULL;
    ok = object && put(w, id, object);
    if (ok) {
        w->isBlank = false;
        MCObjectHeap_touch(w->object.heap);
    } else
        MCObjectHeap_fail(w->object.heap);
    MCObjectRootScope_end(&scope);
    return ok;
}
bool DataWatcher_addObjectByDataType(DataWatcher *w, int32_t id, int32_t type) {
    MCObjectRootScope scope = {0};
    if (!begin(w, &scope))
        return false;
    WatchableObject *v = WatchableObject_new(w->object.heap, type, id, NULL);
    bool ok = v && put(w, id, v);
    if (ok) {
        w->isBlank = false;
        MCObjectHeap_touch(w->object.heap);
    }
    MCObjectRootScope_end(&scope);
    return ok;
}
WatchableObject *DataWatcher_nativeGetWatchedObject(DataWatcher *w, int32_t id) {
    if (!w)
        return NULL;
    MapNode *n = find(w, id);
    return n ? n->value : NULL;
}
static MCObject *get_value(DataWatcher *w, int32_t id, int type, bool nullable) {
    MapNode *n = w ? find(w, id) : NULL;
    MCObject *value = n ? n->value->watchedObject : NULL;
    if (!n || (!value && !nullable) || (value && value_type(value) != type)) {
        MCObjectHeap_fail(w ? w->object.heap : NULL);
        return NULL;
    }
    return value;
}
int8_t DataWatcher_getWatchableObjectByte(DataWatcher *w, int32_t id) {
    Box *b = (Box *)get_value(w, id, 0, false);
    return b ? (int8_t)b->value.integer : 0;
}
int16_t DataWatcher_getWatchableObjectShort(DataWatcher *w, int32_t id) {
    Box *b = (Box *)get_value(w, id, 1, false);
    return b ? (int16_t)b->value.integer : 0;
}
int32_t DataWatcher_getWatchableObjectInt(DataWatcher *w, int32_t id) {
    Box *b = (Box *)get_value(w, id, 2, false);
    return b ? b->value.integer : 0;
}
float DataWatcher_getWatchableObjectFloat(DataWatcher *w, int32_t id) {
    Box *b = (Box *)get_value(w, id, 3, false);
    return b ? b->value.real : 0;
}
NBTString *DataWatcher_getWatchableObjectString(DataWatcher *w, int32_t id) {
    return (NBTString *)get_value(w, id, 4, true);
}
ItemStack *DataWatcher_getWatchableObjectItemStack(DataWatcher *w, int32_t id) {
    return (ItemStack *)get_value(w, id, 5, true);
}
DataWatcherRotations *DataWatcher_getWatchableObjectRotations(DataWatcher *w, int32_t id) {
    return (DataWatcherRotations *)get_value(w, id, 7, true);
}
static uint32_t float_bits(float x) {
    uint32_t out;
    if (isnan(x))
        return 0x7fc00000u;
    memcpy(&out, &x, sizeof(out));
    return out;
}
static bool equal(DataWatcher *w, const MCObject *a, const MCObject *b, bool *result) {
    if (a == b) {
        *result = true;
        return true;
    }
    if (!a || !b) {
        *result = false;
        return true;
    }
    /* ObjectUtils invokes the new value's actual equals method. Vec3i.equals
       accepts its whole hierarchy, independently of watcher dataTypes' exact
       Class keys, and performs ordered virtual X/Y/Z comparisons. */
    if (Vec3i_isInstance(a))
        return Vec3i_equals((Vec3i *)a, (MCObject *)b, result) == NATIVE_ARRAY_OK;
    int type = value_type(a);
    if (type >= 0) {
        if (type != value_type(b)) {
            *result = false;
            return true;
        }
        switch (type) {
        case 0:
        case 1:
        case 2:
            *result = ((const Box *)a)->value.integer == ((const Box *)b)->value.integer;
            break;
        case 3:
            *result = float_bits(((const Box *)a)->value.real) ==
                      float_bits(((const Box *)b)->value.real);
            break;
        case 4:
            *result = NBTString_equals((const NBTString *)a, (const NBTString *)b);
            break;
        case 5:
            *result = false;
            break;
        default: {
            const DataWatcherRotations *x = (const DataWatcherRotations *)a,
                                       *y = (const DataWatcherRotations *)b;
            *result = x->x == y->x && x->y == y->y && x->z == y->z;
            break;
        }
        }
        return true;
    }
    return w->dependencies->objectEquals && w->dependencies->objectEquals(w->context, a, b, result);
}
static bool notify(DataWatcher *w, int32_t id) {
    bool ok = w->owner && w->dependencies->onDataWatcherUpdate(w->context, w->owner, id);
    if (!ok)
        MCObjectHeap_fail(w->object.heap);
    return ok && !MCObjectHeap_failed(w->object.heap);
}
bool DataWatcher_updateObject(DataWatcher *w, int32_t id, MCObject *value) {
    MCObjectRootScope scope = {0};
    if (!begin(w, &scope))
        return false;
    MapNode *n = find(w, id);
    WatchableObject *v = n ? n->value : NULL;
    bool same = false, ok = v && MCObjectRootScope_pin(&scope, value) &&
                            equal(w, value, v->watchedObject, &same) &&
                            !MCObjectHeap_failed(w->object.heap);
    if (ok && !same) {
        ok = WatchableObject_setObject(v, value) && notify(w, id);
        if (ok) {
            WatchableObject_setWatched(v, true);
            w->objectChanged = true;
            MCObjectHeap_touch(w->object.heap);
        }
    }
    if (!ok)
        MCObjectHeap_fail(w->object.heap);
    MCObjectRootScope_end(&scope);
    return ok;
}
bool DataWatcher_setObjectWatched(DataWatcher *w, int32_t id) {
    if (!w || MCObjectHeap_failed(w->object.heap))
        return false;
    MapNode *n = find(w, id);
    if (!n) {
        MCObjectHeap_fail(w->object.heap);
        return false;
    }
    n->value->watched = true;
    w->objectChanged = true;
    MCObjectHeap_touch(w->object.heap);
    return true;
}
bool DataWatcher_hasObjectChanged(const DataWatcher *w) { return w->objectChanged; }
bool DataWatcher_getIsBlank(const DataWatcher *w) { return w->isBlank; }
void DataWatcher_func_111144_e(DataWatcher *w) {
    w->objectChanged = false;
    MCObjectHeap_touch(w->object.heap);
}
static WatchableObjectList *watched_list(DataWatcher *w, bool changed) {
    MCObjectRootScope scope = {0};
    if (!begin(w, &scope))
        return NULL;
    WatchableObjectList *out = NULL;
    bool ok = true;
    if (!changed || w->objectChanged) {
        if (w->table)
            for (int32_t i = 0; i < w->table->capacity && ok; i++)
                for (MapNode *n = w->table->buckets[i]; n && ok; n = n->next) {
                    WatchableObject *v = n->value;
                    if (!changed || v->watched) {
                        if (changed)
                            WatchableObject_setWatched(v, false);
                        if (!out)
                            out = WatchableObjectList_new(w->object.heap);
                        ok = out && WatchableObjectList_add(out, v);
                    }
                }
    }
    if (changed && ok) {
        w->objectChanged = false;
        MCObjectHeap_touch(w->object.heap);
    }
    MCObjectRootScope_end(&scope);
    return ok ? out : NULL;
}
WatchableObjectList *DataWatcher_getChanged(DataWatcher *w) { return watched_list(w, true); }
WatchableObjectList *DataWatcher_getAllWatched(DataWatcher *w) { return watched_list(w, false); }
bool DataWatcher_updateWatchedObjectsFromList(DataWatcher *w, WatchableObjectList *l) {
    MCObjectRootScope scope = {0};
    if (!begin(w, &scope))
        return false;
    bool ok = l && MCObjectRootScope_pin(&scope, (MCObject *)l);
    uint32_t expected = l ? l->modCount : 0;
    for (int32_t i = 0; ok && i != l->size; i++) {
        if (l->modCount != expected) {
            ok = false;
            break;
        }
        WatchableObject *from = WatchableObjectList_get(l, i);
        if (!from) {
            ok = false;
            break;
        }
        MapNode *node = find(w, from->dataValueId);
        if (node)
            ok = WatchableObject_setObject(node->value, from->watchedObject) &&
                 notify(w, from->dataValueId);
    }
    if (ok) {
        w->objectChanged = true;
        MCObjectHeap_touch(w->object.heap);
    } else
        MCObjectHeap_fail(w->object.heap);
    MCObjectRootScope_end(&scope);
    return ok;
}
static bool buffer_begin(PacketBuffer *p, MCObjectRootScope *scope) {
    if (!p || !p->buffer || !p->heap)
        return false;
    if (p->buffer->failed || MCObjectHeap_failed(p->heap) ||
        !MCObjectRootScope_begin(scope, p->heap)) {
        p->buffer->failed = true;
        return false;
    }
    return true;
}
static bool buffer_end(PacketBuffer *p, MCObjectRootScope *scope, bool ok) {
    if (!ok || MCObjectHeap_failed(p->heap))
        p->buffer->failed = true;
    MCObjectRootScope_end(scope);
    return !p->buffer->failed;
}
static bool write_one(PacketBuffer *p, WatchableObject *w, MCObjectRootScope *scope) {
    if (!w || w->object.heap != p->heap) {
        MCObjectHeap_fail(p->heap);
        return false;
    }
    mc_buf *b = p->buffer;
    mc_put_u8(b, (uint8_t)(((uint32_t)w->objectType << 5) | ((uint32_t)w->dataValueId & 31u)));
    if (b->failed)
        return false;
    MCObject *v = w->watchedObject;
    int type = w->objectType;
    if (type >= 0 && type <= 7 && type != 5 && type != 6 &&
        (!v || value_type(v) != type)) {
        MCObjectHeap_fail(p->heap);
        return false;
    }
    switch (type) {
    case 0:
        mc_put_u8(b, (uint8_t)((Box *)v)->value.integer);
        break;
    case 1: {
        uint16_t bits = (uint16_t)((Box *)v)->value.integer;
        int16_t n;
        memcpy(&n, &bits, 2);
        mc_put_i16(b, n);
        break;
    }
    case 2:
        mc_put_i32(b, ((Box *)v)->value.integer);
        break;
    case 3:
        mc_put_f32(b, ((Box *)v)->value.real);
        break;
    case 4:
        return PacketBuffer_writeString(p, (NBTString *)v);
    case 5:
        if (v && !ItemStack_isInstance(v)) {
            MCObjectHeap_fail(p->heap);
            return false;
        }
        return PacketBuffer_writeItemStackToBuffer(p, (ItemStack *)v);
    case 6: {
        if (!v || v->heap != p->heap || !MCObjectRootScope_pin(scope, v) ||
            !BlockPos_isInstance(v)) {
            MCObjectHeap_fail(p->heap);
            return false;
        }
        Vec3i *a = (Vec3i *)v;
        int32_t coordinate;
        if (Vec3i_getX(a, &coordinate) != NATIVE_ARRAY_OK)
            return false;
        mc_put_i32(b, coordinate);
        if (b->failed || Vec3i_getY(a, &coordinate) != NATIVE_ARRAY_OK)
            return false;
        mc_put_i32(b, coordinate);
        if (b->failed || Vec3i_getZ(a, &coordinate) != NATIVE_ARRAY_OK)
            return false;
        mc_put_i32(b, coordinate);
        break;
    }
    case 7: {
        DataWatcherRotations *a = (DataWatcherRotations *)v;
        mc_put_f32(b, a->x);
        mc_put_f32(b, a->y);
        mc_put_f32(b, a->z);
        break;
    }
    default:
        break;
    }
    return !b->failed;
}
bool DataWatcher_writeWatchedListToPacketBuffer(WatchableObjectList *l, PacketBuffer *p) {
    MCObjectRootScope scope = {0};
    if (!buffer_begin(p, &scope))
        return false;
    bool ok = MCObjectRootScope_pin(&scope, (MCObject *)l);
    uint32_t expected = l ? l->modCount : 0;
    if (l)
        for (int32_t i = 0; ok && i != l->size; i++) {
            if (l->modCount != expected) {
                MCObjectHeap_fail(p->heap);
                ok = false;
                break;
            }
            ok = write_one(p, WatchableObjectList_get(l, i), &scope);
        }
    if (ok)
        mc_put_u8(p->buffer, 127);
    return buffer_end(p, &scope, ok);
}
bool DataWatcher_writeTo(DataWatcher *w, PacketBuffer *p) {
    MCObjectRootScope scope = {0};
    if (!buffer_begin(p, &scope))
        return false;
    bool ok = w && MCObjectRootScope_pin(&scope, (MCObject *)w);
    if (ok && w->table)
        for (int32_t i = 0; i < w->table->capacity && ok; i++)
            for (MapNode *n = w->table->buckets[i]; n && ok; n = n->next)
                ok = write_one(p, n->value, &scope);
    if (ok)
        mc_put_u8(p->buffer, 127);
    return buffer_end(p, &scope, ok);
}
bool DataWatcher_readWatchedListFromPacketBuffer(PacketBuffer *p, WatchableObjectList **output) {
    MCObjectRootScope scope = {0};
    if (!buffer_begin(p, &scope))
        return false;
    if (!output) {
        MCObjectHeap_fail(p->heap);
        return buffer_end(p, &scope, false);
    }
    WatchableObjectList *list = NULL;
    bool ok = true;
    for (;;) {
        uint8_t header = mc_get_u8(p->buffer);
        if (p->buffer->failed) {
            ok = false;
            break;
        }
        if (header == 127)
            break;
        if (!list)
            list = WatchableObjectList_new(p->heap);
        if (!list) {
            ok = false;
            break;
        }
        int type = (header & 224) >> 5, id = header & 31;
        MCObject *value = NULL;
        switch (type) {
        case 0: {
            uint8_t bits = mc_get_u8(p->buffer);
            int8_t n;
            memcpy(&n, &bits, 1);
            if (!p->buffer->failed)
                value = DataWatcher_boxByte(p->heap, n);
            break;
        }
        case 1: {
            int16_t n = mc_get_i16(p->buffer);
            if (!p->buffer->failed)
                value = DataWatcher_boxShort(p->heap, n);
            break;
        }
        case 2: {
            int32_t n = mc_get_i32(p->buffer);
            if (!p->buffer->failed)
                value = DataWatcher_boxInt(p->heap, n);
            break;
        }
        case 3: {
            float n = mc_get_f32(p->buffer);
            if (!p->buffer->failed)
                value = DataWatcher_boxFloat(p->heap, n);
            break;
        }
        case 4: {
            NBTString *n = NULL;
            ok = PacketBuffer_readStringFromBuffer(p, 32767, &n);
            value = (MCObject *)n;
            break;
        }
        case 5: {
            ItemStack *n = NULL;
            ok = PacketBuffer_readItemStackFromBuffer(p, &n);
            value = (MCObject *)n;
            break;
        }
        case 6: {
            int32_t x = mc_get_i32(p->buffer);
            if (p->buffer->failed)
                break;
            int32_t y = mc_get_i32(p->buffer);
            if (p->buffer->failed)
                break;
            int32_t z = mc_get_i32(p->buffer);
            if (!p->buffer->failed)
                value = (MCObject *)DataWatcher_blockPos(p->heap, x, y, z);
            break;
        }
        case 7: {
            float x = mc_get_f32(p->buffer);
            if (p->buffer->failed)
                break;
            float y = mc_get_f32(p->buffer);
            if (p->buffer->failed)
                break;
            float z = mc_get_f32(p->buffer);
            if (!p->buffer->failed)
                value = (MCObject *)DataWatcher_rotations(p->heap, x, y, z);
            break;
        }
        }
        if (!ok || p->buffer->failed || MCObjectHeap_failed(p->heap)) {
            ok = false;
            break;
        }
        WatchableObject *w = WatchableObject_new(p->heap, type, id, value);
        if (!w || !WatchableObjectList_add(list, w)) {
            ok = false;
            break;
        }
    }
    ok = buffer_end(p, &scope, ok);
    if (ok)
        *output = list;
    return ok;
}
