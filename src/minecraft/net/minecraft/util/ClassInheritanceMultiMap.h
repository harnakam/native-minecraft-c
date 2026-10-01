#ifndef C919_SOURCE_CLASS_INHERITANCE_MULTI_MAP_H
#define C919_SOURCE_CLASS_INHERITANCE_MULTI_MAP_H
#include "util/NativeJavaClass.h"
#include "util/NativeHashSet.h"
#include "util/NativeIdentityHashSet.h"

typedef struct ClassInheritanceMultiMap ClassInheritanceMultiMap;
typedef struct ClassInheritanceMultiMapIterable ClassInheritanceMultiMapIterable;
typedef struct ClassInheritanceMultiMapIterator ClassInheritanceMultiMapIterator;
typedef struct ClassInheritanceMultiMapDependencies {
    /* Immutable native dispatch table. Context/argument/result references are
       guarded in the receiving heap at each call; NULL context is allowed.
       Callbacks may replace same-heap fields, and subsequent calls reread them.
       A foreign replacement fails before its callback can affect that graph. */
    NativeHashMap *(*newMap)(MCObject *,ClassInheritanceMultiMap *);
    NativeIdentityHashSet *(*newKnownKeys)(MCObject *,ClassInheritanceMultiMap *);
    NativeReferenceList *(*newValues)(MCObject *,ClassInheritanceMultiMap *);
    NativeReferenceList *(*newSingleList)(MCObject *,ClassInheritanceMultiMap *,MCObject *);
    NativeHashSet *(*getAllKnownClasses)(MCObject *,ClassInheritanceMultiMap *);
    bool (*createLookup)(MCObject *,ClassInheritanceMultiMap *,NativeJavaClass *);
    NativeJavaClass *(*initializeClassLookup)(MCObject *,ClassInheritanceMultiMap *,NativeJavaClass *);
    NativeJavaClass *(*objectGetClass)(MCObject *,MCObject *);
    bool (*classIsAssignableFrom)(MCObject *,NativeJavaClass *,NativeJavaClass *,bool *);
    bool (*classIsInstance)(MCObject *,NativeJavaClass *,MCObject *,bool *);
    bool (*objectEquals)(MCObject *,MCObject *,MCObject *,bool *);
    ClassInheritanceMultiMapIterable *(*getByClass)(MCObject *,ClassInheritanceMultiMap *,NativeJavaClass *);
} ClassInheritanceMultiMapDependencies;
struct ClassInheritanceMultiMap {
    MCObject object;
    /* The four original instance fields in declaration order. */
    NativeHashMap *map;
    NativeIdentityHashSet *knownKeys;
    NativeJavaClass *baseClass;
    NativeReferenceList *values;
    /* Required native Class, JDK/Guava and virtual-method boundaries. */
    const ClassInheritanceMultiMapDependencies *dependencies;
    MCObject *dependencyContext;
};
/* Original getByClass anonymous Iterable captures clazz and its outer map.
   Lookup initialization is deferred until iterator(), not getByClass(). */
struct ClassInheritanceMultiMapIterable {
    MCObject object;
    NativeJavaClass *clazz;
    ClassInheritanceMultiMap *owner;
};
ClassInheritanceMultiMap *ClassInheritanceMultiMap_nativeAllocate(MCObjectHeap *,const ClassInheritanceMultiMapDependencies *,MCObject *);
bool ClassInheritanceMultiMap_construct(ClassInheritanceMultiMap *,NativeJavaClass *);
ClassInheritanceMultiMap *ClassInheritanceMultiMap_new(MCObjectHeap *,NativeJavaClass *);
bool ClassInheritanceMultiMap_isInstance(const MCObject *);
bool ClassInheritanceMultiMap_createLookup(ClassInheritanceMultiMap *,NativeJavaClass *);
bool ClassInheritanceMultiMap_createLookup_base(ClassInheritanceMultiMap *,NativeJavaClass *);
NativeJavaClass *ClassInheritanceMultiMap_initializeClassLookup(ClassInheritanceMultiMap *,NativeJavaClass *);
NativeJavaClass *ClassInheritanceMultiMap_initializeClassLookup_base(ClassInheritanceMultiMap *,NativeJavaClass *);
bool ClassInheritanceMultiMap_add(ClassInheritanceMultiMap *,MCObject *);
bool ClassInheritanceMultiMap_remove(ClassInheritanceMultiMap *,MCObject *);
bool ClassInheritanceMultiMap_contains(ClassInheritanceMultiMap *,MCObject *);
int32_t ClassInheritanceMultiMap_size(ClassInheritanceMultiMap *);
ClassInheritanceMultiMapIterable *ClassInheritanceMultiMap_getByClass(ClassInheritanceMultiMap *,NativeJavaClass *);
ClassInheritanceMultiMapIterable *ClassInheritanceMultiMap_getByClass_base(ClassInheritanceMultiMap *,NativeJavaClass *);
bool ClassInheritanceMultiMapIterable_isInstance(const MCObject *);
ClassInheritanceMultiMapIterator *ClassInheritanceMultiMapIterable_iterator(ClassInheritanceMultiMapIterable *);
ClassInheritanceMultiMapIterator *ClassInheritanceMultiMap_iterator(ClassInheritanceMultiMap *);
bool ClassInheritanceMultiMapIterator_isInstance(const MCObject *);
bool ClassInheritanceMultiMapIterator_hasNext(ClassInheritanceMultiMapIterator *);
bool ClassInheritanceMultiMapIterator_next(ClassInheritanceMultiMapIterator *,MCObject **);
/* A traced per-heap class-static owner. This exposes its real set to native
   bootstrap/tests; it is not an invented original public Java method. */
NativeHashSet *ClassInheritanceMultiMap_nativeKnownClasses(MCObjectHeap *);
#endif
