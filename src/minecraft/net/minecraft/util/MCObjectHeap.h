#ifndef C919_MC_OBJECT_HEAP_H
#define C919_MC_OBJECT_HEAP_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Native lifetime adapter for translated Java classes. Object references have
   stable identity; class fields and managed array storage may form cycles.
   Original class copy methods are independent of this transaction snapshot. */
typedef struct MCObjectHeap MCObjectHeap;
typedef struct MCObject MCObject;
typedef struct MCObjectClass MCObjectClass;
typedef MCObject *(*MCObjectVisitor)(MCObject *child, void *context);
struct MCObjectClass {
    const char *name;
    MCObject *(*shallow_clone)(MCObjectHeap *destination, const MCObject *source);
    /* Assign visitor results through each field's actual pointer type. During
       collection the visitor returns its input; during cloning it remaps it. */
    void (*trace)(MCObject *object, MCObjectVisitor visitor, void *context);
    /* Release only native buffers owned by this object. Managed child objects
       belong to the heap and must never be followed or freed here. A failed
       snapshot may destroy a shallow copy before all edges were remapped. */
    void (*destroy)(MCObject *object);
};
struct MCObject { MCObjectHeap *heap; const MCObjectClass *klass; };
typedef struct { MCObjectHeap *heap; uint64_t id; } MCObjectRoot;
/* Initialize with {0} before the first begin; end clears the scope for reuse. */
typedef struct { MCObjectHeap *heap; bool active; } MCObjectRootScope;
/* Native lifetime-only guard for an immutable parent graph. Unlike a mutable
   RootScope, ending this guard does not invalidate snapshots by itself. Explicit
   touch/mutable scopes still do. Never modify fields through a read guard. */
typedef struct { MCObjectHeap *heap; bool active; } MCObjectReadScope;

MCObjectHeap *MCObjectHeap_new(size_t byte_budget);
void MCObjectHeap_free(MCObjectHeap *heap);
MCObject *MCObjectHeap_alloc(MCObjectHeap *heap, size_t bytes, const MCObjectClass *klass);
size_t MCObjectHeap_objectSize(const MCObject *object);
/* Native Java identity-hash adapter. Transaction snapshots retain this value;
   original class constructors/copy methods allocate a new object identity. */
int32_t MCObjectHeap_identityHashCode(const MCObject *object);
/* For classes with inline payload or separately managed child storage only.
   Classes owning native buffers implement shallow_clone and duplicate them. */
MCObject *MCObjectHeap_plainClone(MCObjectHeap *destination, const MCObject *source);
bool MCObjectHeap_failed(const MCObjectHeap *heap);
void MCObjectHeap_fail(MCObjectHeap *heap);
size_t MCObjectHeap_liveObjects(const MCObjectHeap *heap);
size_t MCObjectHeap_liveBytes(const MCObjectHeap *heap);
bool MCObjectHeap_hasBorrowers(const MCObjectHeap *heap);
void MCObjectHeap_touch(MCObjectHeap *heap);
/* Borrow a tracked instance of a class, e.g. an interned Java literal. This
   does not make a root; the caller must retain the returned reference. */
MCObject *MCObjectHeap_findObject(MCObjectHeap *heap,const MCObjectClass *klass,
    bool (*predicate)(const MCObject *object,void *context),void *context);

bool MCObjectRoot_init(MCObjectRoot *root, MCObjectHeap *heap, MCObject *object);
MCObject *MCObjectRoot_get(const MCObjectRoot *root);
bool MCObjectRoot_set(MCObjectRoot *root, MCObject *object);
void MCObjectRoot_drop(MCObjectRoot *root);
/* Root IDs are retained by a complete heap snapshot. Rebind only to the
   snapshot of the source heap; no object/value copy is performed here. */
bool MCObjectRoot_rebind(MCObjectRoot *destination, MCObjectHeap *heap, const MCObjectRoot *source);
bool MCObjectRootScope_begin(MCObjectRootScope *scope, MCObjectHeap *heap);
bool MCObjectRootScope_pin(MCObjectRootScope *scope, MCObject *object);
void MCObjectRootScope_end(MCObjectRootScope *scope);
bool MCObjectReadScope_begin(MCObjectReadScope *, MCObjectHeap *);
void MCObjectReadScope_end(MCObjectReadScope *);

/* No collection or adoption while any borrowed-pointer scope is active.
   Trace marking and snapshot remapping are iterative and cycle-aware. */
bool MCObjectHeap_collect(MCObjectHeap *heap);
MCObjectHeap *MCObjectHeap_clone(const MCObjectHeap *source);
/* Single-writer preflight before a native graph adoption. This performs every adoption
   check without changing either graph. No source mutation may follow the check
   until adoption finishes. */
bool MCObjectHeap_canAdopt(const MCObjectHeap *target,const MCObjectHeap *working);
/* Replace the target graph atomically at the caller's validated commit boundary. Existing
   target root handles retain their IDs. All pointers borrowed before this
   operation become invalid. Native working-owner handles are rebound to the
   target by root ID before the empty working heap is freed. */
bool MCObjectHeap_adopt(MCObjectHeap *target, MCObjectHeap *working);
#endif
