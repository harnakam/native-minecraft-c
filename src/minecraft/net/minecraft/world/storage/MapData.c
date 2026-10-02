#include "world/storage/MapData.h"
#include "entity/Entity.h"
#include "nbt/NBTInternal.h"
#include "util/MCGameplayPlayer.h"
#include "util/MathHelper.h"
#include "util/Vec4b.h"
#include "world/World.h"
#include <math.h>

static const MCObjectClass mapClass, infoClass;
static WorldSavedDataResult failure(MCObjectHeap *h) {
    MCObjectHeap_fail(h);
    return WORLD_SAVED_DATA_FAILURE;
}
static bool identity(const MCObject *o, void *context) { return o == context; }
static bool tracked(MCObjectHeap *h, const MCObject *o) {
    return !o || (o->heap == h && MCObjectHeap_findObject(h, o->klass, identity, (void *)o) == o);
}
bool MapData_isInstance(const MCObject *o) {
    return o && o->klass == &mapClass && tracked(o->heap, o) &&
           MCObjectHeap_objectSize(o) >= sizeof(MapData);
}
bool MapInfo_isInstance(const MCObject *o) {
    return o && o->klass == &infoClass && tracked(o->heap, o) &&
           MCObjectHeap_objectSize(o) >= sizeof(MapInfo);
}
static const NativeJavaClassDescriptor *const mapParents[] = {&WorldSavedData_Class};
const NativeJavaClassDescriptor MapData_Class = {"net.minecraft.world.storage.MapData", mapParents,
                                                 1, MapData_isInstance};
NativeJavaClass *MapData_nativeClass(MCObjectHeap *h) {
    return NativeJavaClass_literal(h, &MapData_Class);
}
static void map_trace(MCObject *o, MCObjectVisitor v, void *c) {
    if (MCObjectHeap_objectSize(o) < sizeof(MapData)) {
        failure(o->heap);
        return;
    }
    MapData *m = (MapData *)o;
    WorldSavedData_traceFields(&m->base, v, c);
    m->colors = (NativeByteArray *)v((MCObject *)m->colors, c);
    m->playersArrayList = (NativeReferenceList *)v((MCObject *)m->playersArrayList, c);
    m->playersHashMap = (NativeHashMap *)v((MCObject *)m->playersHashMap, c);
    m->mapDecorations = (NativeLinkedHashMap *)v((MCObject *)m->mapDecorations, c);
}
static void info_trace(MCObject *o, MCObjectVisitor v, void *c) {
    if (MCObjectHeap_objectSize(o) < sizeof(MapInfo)) {
        failure(o->heap);
        return;
    }
    MapInfo *i = (MapInfo *)o;
    i->outer = (MapData *)v((MCObject *)i->outer, c);
    i->entityplayerObj = (MCGameplayPlayer *)v((MCObject *)i->entityplayerObj, c);
    i->nativeContext = v(i->nativeContext, c);
}
static const MCObjectClass mapClass = {"MapData", MCObjectHeap_plainClone, map_trace, NULL};
static const MCObjectClass infoClass = {"MapData.MapInfo", MCObjectHeap_plainClone, info_trace,
                                        NULL};
static bool begin(MCObject *o, MCObject *context, bool valid, MCObjectRootScope *s) {
    MCObjectHeap *h = o ? o->heap : NULL;
    if (!valid || MCObjectHeap_failed(h) || !tracked(h, context)) {
        failure(h);
        return false;
    }
    if (!MCObjectRootScope_begin(s, h) || !MCObjectRootScope_pin(s, o) ||
        !MCObjectRootScope_pin(s, context)) {
        MCObjectRootScope_end(s);
        failure(h);
        return false;
    }
    return true;
}
static bool map_begin(MapData *m, MCObjectRootScope *s) {
    bool valid = MapData_isInstance((MCObject *)m);
    return begin((MCObject *)m, valid ? m->base.nativeContext : NULL, valid, s);
}
static bool info_begin(MapInfo *i, MCObjectRootScope *s) {
    bool valid = MapInfo_isInstance((MCObject *)i);
    return begin((MCObject *)i, valid ? i->nativeContext : NULL, valid, s);
}
static WorldSavedDataResult finish(MCObjectHeap *h, MCObjectRootScope *s, MCObject *c,
                                   WorldSavedDataResult r) {
    if (!tracked(h, c) || (r != WORLD_SAVED_DATA_OK && r != WORLD_SAVED_DATA_EXCEPTION))
        failure(h);
    if (MCObjectHeap_failed(h))
        r = WORLD_SAVED_DATA_FAILURE;
    MCObjectRootScope_end(s);
    return r;
}
static bool entity_hash(MCObject *c, MCObject *key, int32_t *out) {
    (void)c;
    if (!key || !tracked(key->heap, key) || !Entity_isInstance(key) || !out) {
        failure(key ? key->heap : NULL);
        return false;
    }
    *out = Entity_hashCode((Entity *)key);
    return !MCObjectHeap_failed(key->heap);
}
static bool entity_equal(MCObject *c, MCObject *q, MCObject *stored, bool *out) {
    (void)c;
    if (!q || !tracked(q->heap, q) || !tracked(q->heap, stored) || !Entity_isInstance(q) || !out) {
        failure(q ? q->heap : NULL);
        return false;
    }
    *out = Entity_equals((Entity *)q, stored);
    return !MCObjectHeap_failed(q->heap);
}
static const NativeHashKeyMethods playerKeys = {entity_hash, entity_equal};
static WorldSavedDataResult dispatch_read(MCObject *c, WorldSavedData *b, NBTTagCompound *t) {
    (void)c;
    return MapData_readFromNBT((MapData *)b, t);
}
static WorldSavedDataResult dispatch_write(MCObject *c, WorldSavedData *b, NBTTagCompound *t) {
    (void)c;
    return MapData_writeToNBT((MapData *)b, t);
}
static WorldSavedDataResult dispatch_dirty(MCObject *c, WorldSavedData *b, bool dirty) {
    MapData *m = (MapData *)b;
    return m->nativeDependencies && m->nativeDependencies->setDirty
               ? m->nativeDependencies->setDirty(c, m, dirty)
               : WorldSavedData_setDirty_base(b, dirty);
}
static const WorldSavedDataVirtualMethods savedMethods = {
    .readFromNBT = dispatch_read, .writeToNBT = dispatch_write, .setDirty = dispatch_dirty};
static const WorldSavedDataNativeType savedType = {&mapClass, sizeof(MapData), &savedMethods};
MapData *MapData_nativeAllocate(MCObjectHeap *h, const MapDataDependencies *d, MCObject *c) {
    if (!tracked(h, c)) {
        failure(h);
        return NULL;
    }
    MapData *m = (MapData *)WorldSavedData_nativeAllocate(h, &savedType, c);
    if (m)
        m->nativeDependencies = d;
    return m;
}
bool MapData_construct(MapData *m, NBTString *name) {
    MCObjectRootScope s = {0};
    if (!map_begin(m, &s))
        return false;
    MCObjectHeap *h = m->base.object.heap;
    bool ok = tracked(h, (MCObject *)name);
    if (ok)
        ok = WorldSavedData_construct(&m->base, name);
    else
        failure(h);
    if (ok) {
        NativeByteArray *v = NativeByteArray_new(h, 16384);
        ok = v != NULL;
        if (ok)
            m->colors = v;
    }
    if (ok) {
        NativeReferenceList *v = NativeReferenceList_new(h);
        ok = v != NULL;
        if (ok)
            m->playersArrayList = v;
    }
    if (ok) {
        NativeHashMap *v = NativeHashMap_newWithKeys(h, &playerKeys, NULL);
        ok = v != NULL;
        if (ok)
            m->playersHashMap = v;
    }
    if (ok) {
        NativeLinkedHashMap *v = NativeLinkedHashMap_new(h);
        ok = v != NULL;
        if (ok)
            m->mapDecorations = v;
    }
    if (ok)
        MCObjectHeap_touch(h);
    return finish(h, &s, m->base.nativeContext,
                  ok ? WORLD_SAVED_DATA_OK : WORLD_SAVED_DATA_FAILURE) == WORLD_SAVED_DATA_OK;
}
MapData *MapData_new(MCObjectHeap *h, NBTString *name, const MapDataDependencies *d, MCObject *c) {
    if (!tracked(h, (MCObject *)name) || !tracked(h, c)) {
        failure(h);
        return NULL;
    }
    MCObjectRootScope s = {0};
    if (!MCObjectRootScope_begin(&s, h))
        return NULL;
    if (!MCObjectRootScope_pin(&s, (MCObject *)name) || !MCObjectRootScope_pin(&s, c)) {
        MCObjectRootScope_end(&s);
        return NULL;
    }
    MapData *m = MapData_nativeAllocate(h, d, c);
    if (m && !MapData_construct(m, name))
        m = NULL;
    MCObjectRootScope_end(&s);
    return m;
}
MapInfo *MapInfo_nativeAllocate(MCObjectHeap *h, const MapInfoDependencies *d, MCObject *c) {
    if (!tracked(h, c)) {
        failure(h);
        return NULL;
    }
    MapInfo *i = (MapInfo *)MCObjectHeap_alloc(h, sizeof(*i), &infoClass);
    if (i) {
        i->nativeDependencies = d;
        i->nativeContext = c;
    }
    return i;
}
bool MapInfo_construct(MapInfo *i, MapData *outer, MCGameplayPlayer *p) {
    MCObjectRootScope s = {0};
    if (!info_begin(i, &s))
        return false;
    MCObjectHeap *h = i->object.heap;
    bool ok =
        (!outer || (tracked(h, (MCObject *)outer) && MapData_isInstance((MCObject *)outer))) &&
        (!p || (tracked(h, (MCObject *)p) && MCGameplayPlayer_isInstance((MCObject *)p)));
    if (ok) {
        i->outer = outer;
        i->field_176105_d = true;
        i->minX = 0;
        i->minY = 0;
        i->maxX = 127;
        i->maxY = 127;
        i->entityplayerObj = p;
        MCObjectHeap_touch(h);
    } else
        failure(h);
    return finish(h, &s, i->nativeContext, ok ? WORLD_SAVED_DATA_OK : WORLD_SAVED_DATA_FAILURE) ==
           WORLD_SAVED_DATA_OK;
}
MapInfo *MapInfo_new(MCObjectHeap *h, MapData *outer, MCGameplayPlayer *p,
                     const MapInfoDependencies *d, MCObject *c) {
    if (!tracked(h, (MCObject *)outer) || !tracked(h, (MCObject *)p) || !tracked(h, c)) {
        failure(h);
        return NULL;
    }
    MCObjectRootScope s = {0};
    if (!MCObjectRootScope_begin(&s, h))
        return NULL;
    if (!MCObjectRootScope_pin(&s, (MCObject *)outer) ||
        !MCObjectRootScope_pin(&s, (MCObject *)p) || !MCObjectRootScope_pin(&s, c)) {
        MCObjectRootScope_end(&s);
        return NULL;
    }
    MapInfo *i = MapInfo_nativeAllocate(h, d, c);
    if (i && !MapInfo_construct(i, outer, p))
        i = NULL;
    MCObjectRootScope_end(&s);
    return i;
}
WorldSavedDataResult MapData_calculateMapCenter(MapData *m, double x, double z, int32_t scale) {
    if (!m)
        return WORLD_SAVED_DATA_EXCEPTION;
    MCObjectRootScope s = {0};
    if (!map_begin(m, &s))
        return WORLD_SAVED_DATA_FAILURE;
    int32_t width = nbt_i32(UINT32_C(128) * (UINT32_C(1) << ((uint32_t)scale & 31u)));
    int32_t a = MathHelper_floor_double((x + 64.0) / (double)width),
            b = MathHelper_floor_double((z + 64.0) / (double)width);
    m->xCenter = nbt_i32((uint32_t)a * (uint32_t)width + (uint32_t)(width / 2) - 64u);
    m->zCenter = nbt_i32((uint32_t)b * (uint32_t)width + (uint32_t)(width / 2) - 64u);
    MCObjectHeap_touch(m->base.object.heap);
    return finish(m->base.object.heap, &s, m->base.nativeContext, WORLD_SAVED_DATA_OK);
}
static WorldSavedDataResult check_tag(MapData *m, NBTTagCompound *t) {
    if (!t)
        return WORLD_SAVED_DATA_EXCEPTION;
    if (!tracked(m->base.object.heap, (MCObject *)t) || !NBTTagCompound_isInstance((MCObject *)t))
        return failure(m->base.object.heap);
    return WORLD_SAVED_DATA_OK;
}
static WorldSavedDataResult callback_result(MapData *m, WorldSavedDataResult r) {
    MCObjectHeap *h = m->base.object.heap;
    if (!tracked(h, m->base.nativeContext) ||
        (r != WORLD_SAVED_DATA_OK && r != WORLD_SAVED_DATA_EXCEPTION))
        failure(h);
    return MCObjectHeap_failed(h) ? WORLD_SAVED_DATA_FAILURE : r;
}
static WorldSavedDataResult get_number(MapData *m, NBTTagCompound *t, const char *key, int kind,
                                       int32_t *out) {
    WorldSavedDataResult r = check_tag(m, t);
    if (r != WORLD_SAVED_DATA_OK)
        return r;
    if (!tracked(m->base.object.heap, m->base.nativeContext))
        return failure(m->base.object.heap);
    if (m->nativeDependencies && m->nativeDependencies->getNumber)
        return callback_result(
            m, m->nativeDependencies->getNumber(m->base.nativeContext, t, key, kind, out));
    *out = kind == 1   ? NBTTagCompound_getByte_ascii(t, key)
           : kind == 2 ? NBTTagCompound_getShort_ascii(t, key)
                       : NBTTagCompound_getInteger_ascii(t, key);
    return callback_result(m, WORLD_SAVED_DATA_OK);
}
static WorldSavedDataResult get_array(MapData *m, NBTTagCompound *t, NativeByteArray **out) {
    WorldSavedDataResult r = check_tag(m, t);
    if (r != WORLD_SAVED_DATA_OK)
        return r;
    if (!tracked(m->base.object.heap, m->base.nativeContext))
        return failure(m->base.object.heap);
    NativeByteArray *value = NULL;
    if (m->nativeDependencies && m->nativeDependencies->getByteArray)
        r = m->nativeDependencies->getByteArray(m->base.nativeContext, t, "colors", &value);
    else {
        NBTString *key = NBTString_literalASCII(m->base.object.heap, "colors");
        if (!key)
            return WORLD_SAVED_DATA_FAILURE;
        value = NBTTagCompound_getByteArray(t, key);
    }
    r = callback_result(m, r);
    if (r != WORLD_SAVED_DATA_OK)
        return r;
    if (value && (!tracked(m->base.object.heap, (MCObject *)value) ||
                  !NativeByteArray_isInstance((MCObject *)value)))
        return failure(m->base.object.heap);
    *out = value;
    return WORLD_SAVED_DATA_OK;
}
static WorldSavedDataResult array_read(MCObjectHeap *h, NativeByteArray *a, int32_t index,
                                       int8_t *out) {
    if (!a)
        return WORLD_SAVED_DATA_EXCEPTION;
    if (!tracked(h, (MCObject *)a) || !NativeByteArray_isInstance((MCObject *)a))
        return failure(h);
    if (index < 0 || index >= a->length)
        return WORLD_SAVED_DATA_EXCEPTION;
    *out = a->values[index];
    return WORLD_SAVED_DATA_OK;
}
static WorldSavedDataResult array_write(MCObjectHeap *h, NativeByteArray *a, int32_t index,
                                        int8_t value) {
    if (!a)
        return WORLD_SAVED_DATA_EXCEPTION;
    if (!tracked(h, (MCObject *)a) || !NativeByteArray_isInstance((MCObject *)a))
        return failure(h);
    if (index < 0 || index >= a->length)
        return WORLD_SAVED_DATA_EXCEPTION;
    a->values[index] = value;
    MCObjectHeap_touch(h);
    return WORLD_SAVED_DATA_OK;
}
WorldSavedDataResult MapData_readFromNBT(MapData *m, NBTTagCompound *t) {
    if (!m)
        return WORLD_SAVED_DATA_EXCEPTION;
    MCObjectRootScope s = {0};
    if (!map_begin(m, &s))
        return WORLD_SAVED_DATA_FAILURE;
    MCObjectHeap *h = m->base.object.heap;
    WorldSavedDataResult r = WORLD_SAVED_DATA_FAILURE;
    if (!tracked(h, (MCObject *)t) || !MCObjectRootScope_pin(&s, (MCObject *)t))
        goto done;
    int32_t value, width, height;
    r = get_number(m, t, "dimension", 1, &value);
    if (r != WORLD_SAVED_DATA_OK)
        goto done;
    m->dimension = nbt_i8((uint8_t)value);
    r = get_number(m, t, "xCenter", 3, &value);
    if (r != WORLD_SAVED_DATA_OK)
        goto done;
    m->xCenter = value;
    r = get_number(m, t, "zCenter", 3, &value);
    if (r != WORLD_SAVED_DATA_OK)
        goto done;
    m->zCenter = value;
    r = get_number(m, t, "scale", 1, &value);
    if (r != WORLD_SAVED_DATA_OK)
        goto done;
    m->scale = nbt_i8((uint8_t)value);
    if (m->scale < 0)
        m->scale = 0;
    else if (m->scale > 4)
        m->scale = 4;
    r = get_number(m, t, "width", 2, &width);
    if (r != WORLD_SAVED_DATA_OK)
        goto done;
    r = get_number(m, t, "height", 2, &height);
    if (r != WORLD_SAVED_DATA_OK)
        goto done;
    width = nbt_i16((uint16_t)width);
    height = nbt_i16((uint16_t)height);
    NativeByteArray *input = NULL;
    r = get_array(m, t, &input);
    if (r != WORLD_SAVED_DATA_OK)
        goto done;
    if (!MCObjectRootScope_pin(&s, (MCObject *)input)) {
        r = WORLD_SAVED_DATA_FAILURE;
        goto done;
    }
    if (width == 128 && height == 128) {
        m->colors = input;
        goto done;
    }
    NativeByteArray *colors = NativeByteArray_new(h, 16384);
    if (!colors) {
        r = WORLD_SAVED_DATA_FAILURE;
        goto done;
    }
    m->colors = colors;
    int32_t offsetX = (128 - width) / 2, offsetY = (128 - height) / 2;
    for (int32_t row = 0; row < height; row++) {
        int32_t y = row + offsetY;
        /* The original uses OR here and in the inner condition. */
        if (y >= 0 || y < 128)
            for (int32_t column = 0; column < width; column++) {
                int32_t x = column + offsetX;
                if (x >= 0 || x < 128) {
                    NativeByteArray *destination = m->colors;
                    int32_t destinationIndex = x + y * 128;
                    int8_t byte;
                    /* Source baload precedes the destination's null/index check. */
                    r = array_read(h, input, column + row * width, &byte);
                    if (r != WORLD_SAVED_DATA_OK)
                        goto done;
                    r = array_write(h, destination, destinationIndex, byte);
                    if (r != WORLD_SAVED_DATA_OK)
                        goto done;
                }
            }
    }
    r = WORLD_SAVED_DATA_OK;
done:
    MCObjectHeap_touch(h);
    return finish(h, &s, m->base.nativeContext, r);
}
static WorldSavedDataResult set_number(MapData *m, NBTTagCompound *t, const char *key, int kind,
                                       int32_t value) {
    WorldSavedDataResult r = check_tag(m, t);
    if (r != WORLD_SAVED_DATA_OK)
        return r;
    if (!tracked(m->base.object.heap, m->base.nativeContext))
        return failure(m->base.object.heap);
    if (m->nativeDependencies && m->nativeDependencies->setNumber)
        return callback_result(
            m, m->nativeDependencies->setNumber(m->base.nativeContext, t, key, kind, value));
    bool ok = kind == 1   ? NBTTagCompound_setByte_ascii(t, key, nbt_i8((uint8_t)value))
              : kind == 2 ? NBTTagCompound_setShort_ascii(t, key, nbt_i16((uint16_t)value))
                          : NBTTagCompound_setInteger_ascii(t, key, value);
    return callback_result(m, ok ? WORLD_SAVED_DATA_OK : WORLD_SAVED_DATA_FAILURE);
}
static WorldSavedDataResult set_array(MapData *m, NBTTagCompound *t, NativeByteArray *value) {
    WorldSavedDataResult r = check_tag(m, t);
    if (r != WORLD_SAVED_DATA_OK)
        return r;
    if (!tracked(m->base.object.heap, m->base.nativeContext) ||
        (value && (!tracked(m->base.object.heap, (MCObject *)value) ||
                   !NativeByteArray_isInstance((MCObject *)value))))
        return failure(m->base.object.heap);
    if (m->nativeDependencies && m->nativeDependencies->setByteArray)
        return callback_result(
            m, m->nativeDependencies->setByteArray(m->base.nativeContext, t, "colors", value));
    NBTString *key = NBTString_literalASCII(m->base.object.heap, "colors");
    if (!key)
        return WORLD_SAVED_DATA_FAILURE;
    return callback_result(m, NBTTagCompound_setByteArray(t, key, value)
                                  ? WORLD_SAVED_DATA_OK
                                  : WORLD_SAVED_DATA_FAILURE);
}
WorldSavedDataResult MapData_writeToNBT(MapData *m, NBTTagCompound *t) {
    if (!m)
        return WORLD_SAVED_DATA_EXCEPTION;
    MCObjectRootScope s = {0};
    if (!map_begin(m, &s))
        return WORLD_SAVED_DATA_FAILURE;
    MCObjectHeap *h = m->base.object.heap;
    WorldSavedDataResult r = WORLD_SAVED_DATA_FAILURE;
    if (!tracked(h, (MCObject *)t) || !MCObjectRootScope_pin(&s, (MCObject *)t))
        goto done;
    r = set_number(m, t, "dimension", 1, m->dimension);
    if (r != WORLD_SAVED_DATA_OK)
        goto done;
    r = set_number(m, t, "xCenter", 3, m->xCenter);
    if (r != WORLD_SAVED_DATA_OK)
        goto done;
    r = set_number(m, t, "zCenter", 3, m->zCenter);
    if (r != WORLD_SAVED_DATA_OK)
        goto done;
    r = set_number(m, t, "scale", 1, m->scale);
    if (r != WORLD_SAVED_DATA_OK)
        goto done;
    r = set_number(m, t, "width", 2, 128);
    if (r != WORLD_SAVED_DATA_OK)
        goto done;
    r = set_number(m, t, "height", 2, 128);
    if (r != WORLD_SAVED_DATA_OK)
        goto done;
    r = set_array(m, t, m->colors);
done:
    return finish(h, &s, m->base.nativeContext, r);
}
static WorldSavedDataResult player_argument(MCObjectHeap *h, MCGameplayPlayer *p) {
    if (p && (!tracked(h, (MCObject *)p) || !MCGameplayPlayer_isInstance((MCObject *)p)))
        return failure(h);
    return WORLD_SAVED_DATA_OK;
}
static WorldSavedDataResult lookup(MapData *m, MCGameplayPlayer *p, MapInfo **out) {
    if (!m->playersHashMap)
        return WORLD_SAVED_DATA_EXCEPTION;
    if (!tracked(m->base.object.heap, (MCObject *)m->playersHashMap) ||
        !NativeHashMap_isInstance((MCObject *)m->playersHashMap))
        return failure(m->base.object.heap);
    MCObject *value = NativeHashMap_get(m->playersHashMap, (MCObject *)p);
    if (MCObjectHeap_failed(m->base.object.heap))
        return WORLD_SAVED_DATA_FAILURE;
    if (!tracked(m->base.object.heap, value))
        return failure(m->base.object.heap);
    if (value && !MapInfo_isInstance(value))
        return value->klass == &infoClass ? failure(m->base.object.heap)
                                          : WORLD_SAVED_DATA_EXCEPTION;
    *out = (MapInfo *)value;
    return WORLD_SAVED_DATA_OK;
}
WorldSavedDataResult MapData_getMapInfo(MapData *m, MCGameplayPlayer *p, MapInfo **out) {
    if (!m)
        return WORLD_SAVED_DATA_EXCEPTION;
    MCObjectRootScope s = {0};
    if (!map_begin(m, &s))
        return WORLD_SAVED_DATA_FAILURE;
    MCObjectHeap *h = m->base.object.heap;
    WorldSavedDataResult r = WORLD_SAVED_DATA_FAILURE;
    MapInfo *info = NULL;
    if (!out || player_argument(h, p) != WORLD_SAVED_DATA_OK ||
        !MCObjectRootScope_pin(&s, (MCObject *)p))
        goto done;
    r = lookup(m, p, &info);
    if (r != WORLD_SAVED_DATA_OK)
        goto done;
    if (!info) {
        info = MapInfo_new(h, m, p, NULL, NULL);
        if (!info) {
            r = WORLD_SAVED_DATA_FAILURE;
            goto done;
        }
        if (!NativeHashMap_put(m->playersHashMap, (MCObject *)p, (MCObject *)info)) {
            r = WORLD_SAVED_DATA_FAILURE;
            goto done;
        }
        if (!m->playersArrayList) {
            r = WORLD_SAVED_DATA_EXCEPTION;
            goto done;
        }
        if (!tracked(h, (MCObject *)m->playersArrayList) ||
            !NativeReferenceList_add(m->playersArrayList, (MCObject *)info)) {
            r = WORLD_SAVED_DATA_FAILURE;
            goto done;
        }
    }
done:
    r = finish(h, &s, m->base.nativeContext, r);
    if (r == WORLD_SAVED_DATA_OK)
        *out = info;
    return r;
}
WorldSavedDataResult MapInfo_update_base(MapInfo *i, int32_t x, int32_t y) {
    if (!i)
        return WORLD_SAVED_DATA_EXCEPTION;
    MCObjectRootScope s = {0};
    if (!info_begin(i, &s))
        return WORLD_SAVED_DATA_FAILURE;
    if (i->field_176105_d) {
        if (x < i->minX)
            i->minX = x;
        if (y < i->minY)
            i->minY = y;
        if (x > i->maxX)
            i->maxX = x;
        if (y > i->maxY)
            i->maxY = y;
    } else {
        i->field_176105_d = true;
        i->minX = x;
        i->minY = y;
        i->maxX = x;
        i->maxY = y;
    }
    MCObjectHeap_touch(i->object.heap);
    return finish(i->object.heap, &s, i->nativeContext, WORLD_SAVED_DATA_OK);
}
WorldSavedDataResult MapInfo_update(MapInfo *i, int32_t x, int32_t y) {
    if (!i)
        return WORLD_SAVED_DATA_EXCEPTION;
    MCObjectRootScope s = {0};
    if (!info_begin(i, &s))
        return WORLD_SAVED_DATA_FAILURE;
    WorldSavedDataResult r = i->nativeDependencies && i->nativeDependencies->update
                                 ? i->nativeDependencies->update(i->nativeContext, i, x, y)
                                 : MapInfo_update_base(i, x, y);
    return finish(i->object.heap, &s, i->nativeContext, r);
}
WorldSavedDataResult MapData_updateMapData(MapData *m, int32_t x, int32_t y) {
    if (!m)
        return WORLD_SAVED_DATA_EXCEPTION;
    MCObjectRootScope s = {0};
    if (!map_begin(m, &s))
        return WORLD_SAVED_DATA_FAILURE;
    MCObjectHeap *h = m->base.object.heap;
    WorldSavedDataResult r = WorldSavedData_markDirty_base(&m->base);
    if (r != WORLD_SAVED_DATA_OK)
        goto done;
    NativeReferenceList *list = m->playersArrayList;
    if (!list) {
        r = WORLD_SAVED_DATA_EXCEPTION;
        goto done;
    }
    if (!tracked(h, (MCObject *)list)) {
        r = WORLD_SAVED_DATA_FAILURE;
        goto done;
    }
    NativeIterator *iterator = NativeIterator_fromList(list);
    if (!iterator || !MCObjectRootScope_pin(&s, (MCObject *)iterator)) {
        r = WORLD_SAVED_DATA_FAILURE;
        goto done;
    }
    while (NativeIterator_hasNext(iterator)) {
        MCObject *value = NULL;
        if (!NativeIterator_next(iterator, &value)) {
            r = WORLD_SAVED_DATA_FAILURE;
            goto done;
        }
        if (!tracked(h, value)) {
            r = failure(h);
            goto done;
        }
        if (value && !MapInfo_isInstance(value)) {
            r = value->klass == &infoClass ? failure(h) : WORLD_SAVED_DATA_EXCEPTION;
            goto done;
        }
        r = MapInfo_update((MapInfo *)value, x, y);
        if (r != WORLD_SAVED_DATA_OK)
            goto done;
    }
    if (MCObjectHeap_failed(h))
        r = WORLD_SAVED_DATA_FAILURE;
done:
    return finish(h, &s, m->base.nativeContext, r);
}
WorldSavedDataResult MapData_getMapPacket(MapData *m, ItemStack *stack, World *world,
                                          MCGameplayPlayer *p, S34PacketMaps **out) {
    if (!m)
        return WORLD_SAVED_DATA_EXCEPTION;
    MCObjectRootScope s = {0};
    if (!map_begin(m, &s))
        return WORLD_SAVED_DATA_FAILURE;
    MCObjectHeap *h = m->base.object.heap;
    WorldSavedDataResult r = WORLD_SAVED_DATA_FAILURE;
    MapInfo *info = NULL;
    S34PacketMaps *packet = NULL;
    if (!out || !tracked(h, (MCObject *)stack) || !tracked(h, (MCObject *)world) ||
        player_argument(h, p) != WORLD_SAVED_DATA_OK ||
        !MCObjectRootScope_pin(&s, (MCObject *)stack) ||
        !MCObjectRootScope_pin(&s, (MCObject *)world) || !MCObjectRootScope_pin(&s, (MCObject *)p))
        goto done;
    r = lookup(m, p, &info);
    if (r == WORLD_SAVED_DATA_OK && info)
        r = MapInfo_getPacket(info, stack, &packet);
done:
    r = finish(h, &s, m->base.nativeContext, r);
    if (r == WORLD_SAVED_DATA_OK)
        *out = packet;
    return r;
}
WorldSavedDataResult MapData_updateDecorations(MapData *m, int32_t type, World *world,
                                               NBTString *id, double x, double z, double rotation) {
    if (!m)
        return WORLD_SAVED_DATA_EXCEPTION;
    MCObjectRootScope s = {0};
    if (!map_begin(m, &s))
        return WORLD_SAVED_DATA_FAILURE;
    MCObjectHeap *h = m->base.object.heap;
    WorldSavedDataResult r = WORLD_SAVED_DATA_FAILURE;
    if (!tracked(h, (MCObject *)world) ||
        (id && (!tracked(h, (MCObject *)id) || !NBTString_isInstance((MCObject *)id))) ||
        !MCObjectRootScope_pin(&s, (MCObject *)world) || !MCObjectRootScope_pin(&s, (MCObject *)id))
        goto done;
    int32_t size = nbt_i32(UINT32_C(1) << ((uint32_t)m->scale & 31u));
    float f = (float)(x - (double)m->xCenter) / (float)size,
          f1 = (float)(z - (double)m->zCenter) / (float)size;
    int8_t b0 = nbt_i8((uint8_t)nbt_javaDoubleInt((double)(f * 2.0f) + 0.5));
    int8_t b1 = nbt_i8((uint8_t)nbt_javaDoubleInt((double)(f1 * 2.0f) + 0.5)), b2;
    if (f >= -63.0f && f1 >= -63.0f && f <= 63.0f && f1 <= 63.0f) {
        rotation = rotation + (rotation < 0.0 ? -8.0 : 8.0);
        b2 = nbt_i8((uint8_t)nbt_javaDoubleInt(rotation * 16.0 / 360.0));
        if (m->dimension < 0) {
            if (!world) {
                r = WORLD_SAVED_DATA_EXCEPTION;
                goto done;
            }
            if (!World_isInstance((MCObject *)world)) {
                r = failure(h);
                goto done;
            }
            WorldInfo *info = World_getWorldInfo(world);
            if (MCObjectHeap_failed(h)) {
                r = WORLD_SAVED_DATA_FAILURE;
                goto done;
            }
            if (!info) {
                r = WORLD_SAVED_DATA_EXCEPTION;
                goto done;
            }
            if (!tracked(h, (MCObject *)info)) {
                r = failure(h);
                goto done;
            }
            int64_t time = WorldInfo_getWorldTime(info);
            if (MCObjectHeap_failed(h)) {
                r = WORLD_SAVED_DATA_FAILURE;
                goto done;
            }
            uint32_t k = (uint32_t)(time / 10);
            b2 = nbt_i8((uint8_t)(((k * k * UINT32_C(34187121) + k * UINT32_C(121)) >> 15) & 15u));
        }
    } else {
        /* Target bytecode removes NaN: both absolute comparisons must be <320. */
        if (!(fabsf(f) < 320.0f && fabsf(f1) < 320.0f)) {
            if (!m->mapDecorations) {
                r = WORLD_SAVED_DATA_EXCEPTION;
                goto done;
            }
            if (!tracked(h, (MCObject *)m->mapDecorations)) {
                r = failure(h);
                goto done;
            }
            NativeLinkedHashMap_remove(m->mapDecorations, (MCObject *)id);
            r = MCObjectHeap_failed(h) ? WORLD_SAVED_DATA_FAILURE : WORLD_SAVED_DATA_OK;
            goto done;
        }
        type = 6;
        b2 = 0;
        if (f <= -63.0f)
            b0 = -128;
        if (f1 <= -63.0f)
            b1 = -128;
        if (f >= 63.0f)
            b0 = 127;
        if (f1 >= 63.0f)
            b1 = 127;
    }
    NativeLinkedHashMap *decorations = m->mapDecorations;
    Vec4b *value = Vec4b_new(h, nbt_i8((uint8_t)type), b0, b1, b2);
    if (!value) {
        r = WORLD_SAVED_DATA_FAILURE;
        goto done;
    }
    if (!decorations) {
        r = WORLD_SAVED_DATA_EXCEPTION;
        goto done;
    }
    if (!tracked(h, (MCObject *)decorations)) {
        r = failure(h);
        goto done;
    }
    r = NativeLinkedHashMap_put(decorations, (MCObject *)id, (MCObject *)value)
            ? WORLD_SAVED_DATA_OK
            : WORLD_SAVED_DATA_FAILURE;
done:
    return finish(h, &s, m->base.nativeContext, r);
}
