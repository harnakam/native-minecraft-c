#include "world/World.h"
#include "util/NativeCollection.h"
#include "util/ClassInheritanceMultiMap.h"
#include "util/NativeHashMap.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"World lifecycle check %u line %d\n",checks,__LINE__);exit(1);}} while(0)
static const MCObjectClass tokenClass={"test.LifecycleToken",MCObjectHeap_plainClone,NULL,NULL};
static void bulk_basic(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1u<<20);CHECK(heap);
    NativeReferenceList *list=NativeReferenceList_new(heap);CHECK(list);
    MCObject *token=MCObjectHeap_alloc(heap,sizeof(*token),&tokenClass);CHECK(token);
    NativeObjectArray *array=NativeObjectArray_new(heap,3);CHECK(array);
    CHECK(NativeObjectArray_set(array,0,token));CHECK(NativeObjectArray_set(array,2,token));
    bool changed=false;CHECK(NativeReferenceList_addAllArray(list,array,&changed));CHECK(changed);
    CHECK(list->size==3&&list->modCount==1);
    CHECK(NativeReferenceList_get(list,0)==token&&NativeReferenceList_get(list,1)==NULL&&NativeReferenceList_get(list,2)==token);
    NativeIterator *iterator=NativeIterator_fromList(list);CHECK(iterator);
    NativeObjectArray *empty=NativeObjectArray_new(heap,0);CHECK(empty);
    changed=true;CHECK(NativeReferenceList_addAllArray(list,empty,&changed));CHECK(!changed);
    CHECK(list->size==3&&list->modCount==2);
    MCObject *value=token;CHECK(!NativeIterator_next(iterator,&value));CHECK(value==token&&MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}
typedef struct Context {
    MCObject object;World *world;
    NativeReferenceList *destination,*replacement;
    NativeObjectArray *snapshot;
    MCObject *foreign;
    unsigned calls,mode;
} Context;
static void trace(MCObject *object,MCObjectVisitor visit,void *opaque) {
    Context *c=(Context *)object;
    c->world=(World *)visit((MCObject *)c->world,opaque);
    c->destination=(NativeReferenceList *)visit((MCObject *)c->destination,opaque);
    c->replacement=(NativeReferenceList *)visit((MCObject *)c->replacement,opaque);
    c->snapshot=(NativeObjectArray *)visit((MCObject *)c->snapshot,opaque);
    /* Foreign fixture edges are intentionally not traced into the owner. */
}
static const MCObjectClass contextClass={"test.LifecycleContext",MCObjectHeap_plainClone,trace,NULL};
static Context *setup(MCObjectHeap *heap,const WorldDependencies *deps) {
    Context *c=(Context *)MCObjectHeap_alloc(heap,sizeof(*c),&contextClass);CHECK(c);
    c->world=World_nativeAllocate(heap,deps,(MCObject *)c);CHECK(c->world);
    c->destination=NativeReferenceList_new(heap);c->replacement=NativeReferenceList_new(heap);
    CHECK(c->destination&&c->replacement);
    c->world->unloadedEntityList=c->destination;
    c->world->tileEntitiesToBeRemoved=c->destination;
    c->snapshot=NativeObjectArray_new(heap,3);CHECK(c->snapshot);
    c->snapshot->values[0]=(MCObject *)c->world;c->snapshot->values[2]=(MCObject *)c->world;
    return c;
}
static NativeObjectArray *snapshot(MCObject *object,MCObject *collection) {
    Context *c=(Context *)object;++c->calls;CHECK(collection==(MCObject *)c->replacement);
    CHECK(c->destination->size==0&&c->destination->modCount==0);
    CHECK(MCObjectHeap_hasBorrowers(object->heap));CHECK(!MCObjectHeap_collect(object->heap));
    if(c->mode==1)c->world->unloadedEntityList=c->replacement;
    if(c->mode==2)return NULL;
    if(c->mode==3){MCObjectHeap_fail(object->heap);return c->snapshot;}
    if(c->mode==4)return (NativeObjectArray *)c->foreign;
    if(c->mode==5)c->snapshot->values[1]=c->foreign;
    if(c->mode==6)return (NativeObjectArray *)MCObjectHeap_alloc(object->heap,sizeof(MCObject),c->snapshot->object.klass);
    if(c->mode==7)c->snapshot->length=4;
    return c->snapshot;
}
static bool bulk_dispatch(MCObject *object,NativeReferenceList *destination,MCObject *collection,bool *out) {
    Context *c=(Context *)object;++c->calls;
    CHECK(destination==c->destination&&collection==(c->mode==7?NULL:(MCObject *)c->replacement));
    c->world->unloadedEntityList=c->replacement;
    if(c->mode==6)return false;
    return NativeReferenceList_addAllArray(destination,c->snapshot,out);
}
static bool virtual_mark(MCObject *object,World *world,MCObject *tile) {
    Context *c=(Context *)object;++c->calls;CHECK(c->world==world);
    return c->mode!=6&&World_markTileEntityForRemoval_base(world,tile);
}
static bool virtual_unload(MCObject *object,World *world,MCObject *collection) {
    Context *c=(Context *)object;++c->calls;CHECK(c->world==world);
    return c->mode!=6&&World_unloadEntities_base(world,collection);
}
static void callback_order(void) {
    const WorldDependencies deps={.collectionToArray=snapshot};
    for(unsigned mode=0;mode<=7;mode++) {
        MCObjectHeap *heap=MCObjectHeap_new(1u<<20),*other=MCObjectHeap_new(1u<<20);CHECK(heap&&other);
        Context *c=setup(heap,&deps);c->mode=mode;
        c->foreign=mode==4?(MCObject *)NativeObjectArray_new(other,0):MCObjectHeap_alloc(other,sizeof(MCObject),&tokenClass);
        CHECK(c->foreign);
        CHECK(World_unloadEntities(c->world,(MCObject *)c->replacement)==(mode<2));
        CHECK(c->calls==1&&!MCObjectHeap_hasBorrowers(heap));
        CHECK(!MCObjectHeap_failed(other));
        if(mode<2) {
            CHECK(c->destination->size==3&&c->destination->modCount==1);
            CHECK(c->destination->storage->items[0]==(MCObject *)c->world);
            CHECK(c->destination->storage->items[1]==NULL&&c->destination->storage->items[2]==(MCObject *)c->world);
            CHECK(c->replacement->size==0);
            CHECK(c->world->unloadedEntityList==(mode==1?c->replacement:c->destination));
        } else CHECK(MCObjectHeap_failed(heap)&&c->destination->size==0&&c->destination->modCount==0);
        MCObjectHeap_free(other);MCObjectHeap_free(heap);
    }
    const WorldDependencies direct={.collectionAddAll=bulk_dispatch,.collectionToArray=snapshot};
    MCObjectHeap *heap=MCObjectHeap_new(1u<<20);Context *c=setup(heap,&direct);
    CHECK(World_unloadEntities(c->world,(MCObject *)c->replacement));CHECK(c->calls==1);
    CHECK(c->destination->size==3&&c->replacement->size==0);MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(1u<<20);c=setup(heap,&direct);c->mode=7;
    CHECK(World_unloadEntities(c->world,NULL));CHECK(c->calls==1&&c->destination->size==3);
    MCObjectHeap_free(heap);
    const WorldDependencies overrides={.markTileEntityForRemoval=virtual_mark,.unloadEntities=virtual_unload};
    heap=MCObjectHeap_new(1u<<20);c=setup(heap,&overrides);
    CHECK(World_markTileEntityForRemoval(c->world,NULL));CHECK(c->calls==1&&c->destination->size==1);
    CHECK(World_unloadEntities(c->world,(MCObject *)c->destination));
    CHECK(c->calls==2&&c->destination->size==2&&c->destination->modCount==2);
    MCObjectHeap_free(heap);
    for(unsigned which=0;which<3;which++) {
        heap=MCObjectHeap_new(1u<<20);c=setup(heap,which==2?&direct:&overrides);c->mode=6;
        CHECK(!(which==0?World_markTileEntityForRemoval(c->world,NULL):World_unloadEntities(c->world,(MCObject *)c->replacement)));
        CHECK(c->calls==1&&MCObjectHeap_failed(heap));
        CHECK(c->destination->size==0&&c->destination->modCount==0&&!MCObjectHeap_hasBorrowers(heap));
        if(which==2)CHECK(c->world->unloadedEntityList==c->replacement);
        MCObjectHeap_free(heap);
    }
}
static void failure_guards(void) {
    const WorldDependencies deps={.collectionToArray=snapshot};
    for(unsigned mode=0;mode<8;mode++) {
        MCObjectHeap *heap=MCObjectHeap_new(1u<<20),*other=MCObjectHeap_new(1u<<20);CHECK(heap&&other);
        Context *c=setup(heap,&deps);MCObject *argument=(MCObject *)c->replacement;
        MCObject *foreign=MCObjectHeap_alloc(other,sizeof(MCObject),&tokenClass);CHECK(foreign);
        if(mode==0)c->world->unloadedEntityList=NULL;
        if(mode==1)argument=NULL;
        if(mode==2)c->world->unloadedEntityList=(NativeReferenceList *)foreign;
        if(mode==3)argument=foreign;
        if(mode==4)c->world->dependencyContext=foreign;
        if(mode==5)c->world->unloadedEntityList=(NativeReferenceList *)MCObjectHeap_alloc(heap,sizeof(MCObject),c->destination->object.klass);
        if(mode==6)c->world->dependencies=NULL,argument=(MCObject *)c;
        if(mode==7)MCObjectHeap_fail(heap);
        CHECK(!World_unloadEntities(c->world,argument));CHECK(MCObjectHeap_failed(heap));
        CHECK(c->calls==0&&c->destination->size==0&&c->destination->modCount==0);
        CHECK(!MCObjectHeap_hasBorrowers(heap)&&!MCObjectHeap_failed(other));
        MCObjectHeap_free(other);MCObjectHeap_free(heap);
    }
    MCObjectHeap *heap=MCObjectHeap_new(1u<<20),*other=MCObjectHeap_new(1u<<20);
    Context *c=setup(heap,NULL);MCObject *foreign=MCObjectHeap_alloc(other,sizeof(MCObject),&tokenClass);
    CHECK(!World_markTileEntityForRemoval(c->world,foreign));CHECK(MCObjectHeap_failed(heap)&&!MCObjectHeap_failed(other));
    CHECK(c->destination->size==0);MCObjectHeap_free(other);MCObjectHeap_free(heap);
}
static void source_collection_paths(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1u<<20);Context *c=setup(heap,NULL);
    CHECK(NativeReferenceList_add(c->replacement,(MCObject *)c->world));CHECK(NativeReferenceList_add(c->replacement,NULL));
    CHECK(NativeReferenceList_add(c->replacement,(MCObject *)c->world));
    NativeObjectArray *array=NativeCollection_toArray((MCObject *)c->replacement);CHECK(array&&array->length==3);
    CHECK(array->values[0]==array->values[2]&&array->values[1]==NULL);
    CHECK(NativeReferenceList_set(c->replacement,0,(MCObject *)c)==(MCObject *)c->world);
    CHECK(array->values[0]==(MCObject *)c->world);
    CHECK(World_unloadEntities(c->world,(MCObject *)c->replacement));CHECK(c->destination->size==3&&c->destination->modCount==1);
    CHECK(World_unloadEntities(c->world,(MCObject *)c->destination));CHECK(c->destination->size==6&&c->destination->modCount==2);
    for(int32_t i=0;i<3;i++)CHECK(c->destination->storage->items[i]==c->destination->storage->items[i+3]);
    MCObjectHeap_free(heap);
}
static void class_map_snapshots(void) {
    for(unsigned mode=0;mode<5;mode++) {
        MCObjectHeap *heap=MCObjectHeap_new(1u<<20),*foreign=MCObjectHeap_new(1u<<20);CHECK(heap&&foreign);
        Context *c=setup(heap,NULL);
        ClassInheritanceMultiMap *map=ClassInheritanceMultiMap_new(heap,NativeJavaClass_Object(heap));CHECK(map);
        /* Real translated size/iterator and live values field; no invented
           Class.getClass implementation is required for this read boundary. */
        CHECK(NativeReferenceList_add(map->values,(MCObject *)c->world));
        CHECK(NativeReferenceList_add(map->values,NULL));
        CHECK(NativeReferenceList_add(map->values,(MCObject *)c->world));
        if(mode==1)map->values=NULL;
        if(mode==2)map->values->size=map->values->storage->capacity+1;
        if(mode==3)map->values->storage->items[1]=MCObjectHeap_alloc(foreign,sizeof(MCObject),&tokenClass);
        if(mode==4)map->values->storage=NULL;
        CHECK(World_unloadEntities(c->world,(MCObject *)map)==(mode==0));
        if(mode==0) {
            CHECK(c->destination->size==3&&c->destination->modCount==1);
            CHECK(c->destination->storage->items[0]==(MCObject *)c->world&&c->destination->storage->items[1]==NULL);
            CHECK(c->destination->storage->items[2]==(MCObject *)c->world);
        } else CHECK(MCObjectHeap_failed(heap)&&c->destination->size==0&&c->destination->modCount==0);
        CHECK(!MCObjectHeap_failed(foreign)&&!MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(foreign);MCObjectHeap_free(heap);
    }
    MCObjectHeap *heap=MCObjectHeap_new(1u<<20);Context *c=setup(heap,NULL);
    ClassInheritanceMultiMap *map=ClassInheritanceMultiMap_new(heap,NativeJavaClass_Object(heap));CHECK(map);
    CHECK(World_unloadEntities(c->world,(MCObject *)map));CHECK(c->destination->size==0&&c->destination->modCount==1);
    MCObjectHeap_free(heap);
}
static void oom_prefix(void) {
    bool observed=false,success=false;
    for(size_t budget=128;budget<4096;budget+=8) {
        MCObjectHeap *heap=MCObjectHeap_new(budget);CHECK(heap);
        NativeReferenceList *list=NativeReferenceList_new(heap);NativeObjectArray *array=NativeObjectArray_new(heap,40);
        if(list&&array&&!MCObjectHeap_failed(heap)) {
            NativeReferenceArray *storage=list->storage;bool changed=true;
            bool ok=NativeReferenceList_addAllArray(list,array,&changed);
            CHECK(list->modCount==1);
            if(ok){success=true;CHECK(list->size==40&&changed);}
            else {observed=true;CHECK(MCObjectHeap_failed(heap)&&list->storage==storage&&list->size==0&&changed);}
            CHECK(!MCObjectHeap_hasBorrowers(heap));
        }
        MCObjectHeap_free(heap);
    }
    CHECK(observed&&success);
}
static void graph_lifetime(void) {
    const WorldDependencies deps={.collectionToArray=snapshot};
    MCObjectHeap *heap=MCObjectHeap_new(1u<<20);Context *c=setup(heap,&deps);MCObjectRoot root={0};
    CHECK(MCObjectRoot_init(&root,heap,(MCObject *)c));c->mode=1;
    CHECK(World_unloadEntities(c->world,(MCObject *)c->replacement));
    CHECK(MCObjectHeap_collect(heap));CHECK(c->destination->storage->items[0]==(MCObject *)c->world);
    size_t count=MCObjectHeap_liveObjects(heap);
    MCObjectHeap *copy=MCObjectHeap_clone(heap);CHECK(copy);MCObjectRoot copied={0};CHECK(MCObjectRoot_rebind(&copied,copy,&root));
    Context *cc=(Context *)MCObjectRoot_get(&copied);CHECK(cc&&cc!=c&&cc->world!=c->world);
    CHECK(cc->world->dependencyContext==(MCObject *)cc&&cc->world->unloadedEntityList==cc->replacement);
    CHECK(cc->destination->storage->items[0]==(MCObject *)cc->world&&cc->destination->storage->items[2]==(MCObject *)cc->world);
    CHECK(cc->snapshot->values[0]==(MCObject *)cc->world);
    CHECK(MCObjectHeap_collect(copy)&&MCObjectHeap_liveObjects(copy)==count);
    CHECK(MCObjectHeap_adopt(heap,copy));MCObjectHeap_free(copy);
    c=(Context *)MCObjectRoot_get(&root);CHECK(c&&c->world->dependencyContext==(MCObject *)c);
    CHECK(MCObjectHeap_collect(heap)&&MCObjectHeap_liveObjects(heap)==count);
    MCObjectRoot_drop(&root);CHECK(MCObjectHeap_collect(heap)&&MCObjectHeap_liveObjects(heap)==0);MCObjectHeap_free(heap);
}
int main(void) {
    bulk_basic();callback_order();failure_guards();source_collection_paths();class_map_snapshots();oom_prefix();graph_lifetime();
    printf("World chunk lifecycle: %u checks passed\n",checks);return 0;
}
