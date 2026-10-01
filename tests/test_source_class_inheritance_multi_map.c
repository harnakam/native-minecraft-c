#include "util/ClassInheritanceMultiMap.h"
#include "client/entity/EntityPlayerSP.h"
#include "entity/player/EntityPlayerMP.h"
#include <stdio.h>
#include <stdlib.h>

static int checks;
#define CHECK(test) do {++checks;if(!(test)){fprintf(stderr,"Source class map check %d failed at %d: %s\n",checks,__LINE__,#test);exit(1);}}while(0)
static EntityPlayerSP *sp(MCObjectHeap *h,int32_t id){
    EntityPlayerSP *p=EntityPlayerSP_nativeAllocate(h);CHECK(p);Entity_setEntityId((Entity *)p,id);return p;
}
static EntityPlayerMP *mp(MCObjectHeap *h,int32_t id){
    EntityPlayerMP *p=EntityPlayerMP_nativeAllocate(h);CHECK(p);Entity_setEntityId((Entity *)p,id);return p;
}
static NativeReferenceList *lookup(ClassInheritanceMultiMap *m,NativeJavaClass *c){return (NativeReferenceList *)NativeHashMap_get(m->map,(MCObject *)c);}
static int read_all(ClassInheritanceMultiMapIterator *it,MCObject **out,int maximum){
    CHECK(it);int n=0;while(ClassInheritanceMultiMapIterator_hasNext(it)){
        CHECK(n<maximum);CHECK(ClassInheritanceMultiMapIterator_next(it,&out[n]));++n;
    }return n;
}
static void original_collection_behavior(void){
    MCObjectHeap *h=MCObjectHeap_new(4000000);CHECK(h);
    NativeJavaClass *entity=NativeJavaClass_Entity(h);CHECK(entity);
    ClassInheritanceMultiMap *m=ClassInheritanceMultiMap_new(h,entity);CHECK(m);
    CHECK(m->baseClass==entity);CHECK(NativeIdentityHashSet_size(m->knownKeys)==1);
    CHECK(lookup(m,entity)==m->values);CHECK(ClassInheritanceMultiMap_size(m)==0);
    CHECK(NativeHashSet_size(ClassInheritanceMultiMap_nativeKnownClasses(h))==0);
    EntityPlayerSP *a=sp(h,7),*same=sp(h,7),*last=sp(h,8);EntityPlayerMP *other=mp(h,7);
    CHECK(Entity_equals((Entity *)a,(MCObject *)same));CHECK(Entity_equals((Entity *)a,(MCObject *)other));
    CHECK(!Entity_equals((Entity *)a,NULL));CHECK(Entity_hashCode((Entity *)a)==7);
    CHECK(ClassInheritanceMultiMap_add(m,(MCObject *)a));CHECK(ClassInheritanceMultiMap_add(m,(MCObject *)a));
    CHECK(ClassInheritanceMultiMap_add(m,(MCObject *)other));CHECK(ClassInheritanceMultiMap_add(m,(MCObject *)last));
    CHECK(ClassInheritanceMultiMap_size(m)==4);
    MCObject *all[8]={0};CHECK(read_all(ClassInheritanceMultiMap_iterator(m),all,8)==4);
    CHECK(all[0]==(MCObject *)a&&all[1]==(MCObject *)a&&all[2]==(MCObject *)other&&all[3]==(MCObject *)last);
    NativeJavaClass *sc=NativeJavaClass_getClass(h,(MCObject *)a);CHECK(sc);
    ClassInheritanceMultiMapIterable *deferred=ClassInheritanceMultiMap_getByClass(m,sc);CHECK(deferred);
    CHECK(!NativeIdentityHashSet_contains(m->knownKeys,(MCObject *)sc));CHECK(!lookup(m,sc));
    CHECK(read_all(ClassInheritanceMultiMapIterable_iterator(deferred),all,8)==3);
    CHECK(all[0]==(MCObject *)a&&all[1]==(MCObject *)a&&all[2]==(MCObject *)last);
    NativeReferenceList *selected=lookup(m,sc);CHECK(selected&&selected!=m->values&&selected->size==3);
    CHECK(ClassInheritanceMultiMap_contains(m,(MCObject *)same));
    CHECK(ClassInheritanceMultiMap_remove(m,(MCObject *)same));
    CHECK(m->values->size==3&&selected->size==2);
    CHECK(m->values->storage->items[0]==(MCObject *)a&&selected->storage->items[0]==(MCObject *)a);
    /* Original remove visits classes assignable from the query's class; equal
       entity IDs in different subclasses can leave a different lookup live. */
    CHECK(ClassInheritanceMultiMap_remove(m,(MCObject *)other));
    CHECK(m->values->size==2&&m->values->storage->items[0]==(MCObject *)other);
    CHECK(selected->size==2&&selected->storage->items[0]==(MCObject *)a);
    CHECK(!MCObjectHeap_failed(h));
    ClassInheritanceMultiMap *next=ClassInheritanceMultiMap_new(h,entity);CHECK(next);
    CHECK(NativeIdentityHashSet_contains(next->knownKeys,(MCObject *)sc));
    CHECK(!lookup(next,sc));CHECK(lookup(next,entity)==next->values);
    CHECK(ClassInheritanceMultiMap_add(next,(MCObject *)last));CHECK(lookup(next,sc)->size==1);
    CHECK(!ClassInheritanceMultiMap_remove(next,(MCObject *)a));CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void direct_lookup_and_iterator_failures(void){
    MCObjectHeap *h=MCObjectHeap_new(4000000);CHECK(h);
    ClassInheritanceMultiMap *m=ClassInheritanceMultiMap_new(h,NativeJavaClass_Entity(h));CHECK(m);
    EntityPlayerSP *a=sp(h,10);CHECK(ClassInheritanceMultiMap_add(m,(MCObject *)a));
    NativeJavaClass *sc=NativeJavaClass_getClass(h,(MCObject *)a);CHECK(sc);
    CHECK(ClassInheritanceMultiMap_createLookup(m,sc));NativeReferenceList *selected=lookup(m,sc);CHECK(selected->size==1);
    CHECK(ClassInheritanceMultiMap_createLookup(m,sc));CHECK(selected->size==2&&m->values->size==1);
    CHECK(!ClassInheritanceMultiMap_createLookup(m,m->baseClass));
    CHECK(MCObjectHeap_failed(h)&&m->values->size==2);
    MCObjectHeap_free(h);
    h=MCObjectHeap_new(4000000);CHECK(h);m=ClassInheritanceMultiMap_new(h,NativeJavaClass_Entity(h));CHECK(m);
    a=sp(h,1);CHECK(ClassInheritanceMultiMap_add(m,(MCObject *)a));
    ClassInheritanceMultiMapIterator *it=ClassInheritanceMultiMap_iterator(m);CHECK(it);
    CHECK(ClassInheritanceMultiMap_add(m,(MCObject *)a));
    CHECK(ClassInheritanceMultiMapIterator_hasNext(it));MCObject *sentinel=(MCObject *)m;
    CHECK(!ClassInheritanceMultiMapIterator_next(it,&sentinel)&&sentinel==(MCObject *)m&&MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h=MCObjectHeap_new(4000000);CHECK(h);m=ClassInheritanceMultiMap_new(h,NativeJavaClass_Entity(h));CHECK(m);
    a=sp(h,2);CHECK(ClassInheritanceMultiMap_add(m,(MCObject *)a));sc=NativeJavaClass_getClass(h,(MCObject *)a);
    it=ClassInheritanceMultiMapIterable_iterator(ClassInheritanceMultiMap_getByClass(m,sc));CHECK(it);
    CHECK(ClassInheritanceMultiMapIterator_hasNext(it));CHECK(ClassInheritanceMultiMap_add(m,(MCObject *)a));
    MCObject *out=NULL;CHECK(ClassInheritanceMultiMapIterator_next(it,&out)&&out==(MCObject *)a);
    CHECK(!ClassInheritanceMultiMapIterator_hasNext(it)&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
static void deferred_failure_and_nullable_constructor(void){
    MCObjectHeap *h=MCObjectHeap_new(4000000);CHECK(h);
    ClassInheritanceMultiMap *m=ClassInheritanceMultiMap_new(h,NativeJavaClass_Entity(h));CHECK(m);
    ClassInheritanceMultiMapIterable *bad=ClassInheritanceMultiMap_getByClass(m,NativeJavaClass_Object(h));
    CHECK(bad&&!MCObjectHeap_failed(h));CHECK(NativeHashSet_size(ClassInheritanceMultiMap_nativeKnownClasses(h))==0);
    CHECK(!ClassInheritanceMultiMapIterable_iterator(bad)&&MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h=MCObjectHeap_new(4000000);CHECK(h);m=ClassInheritanceMultiMap_new(h,NULL);CHECK(m);
    CHECK(!m->baseClass&&lookup(m,NULL)==m->values&&NativeIdentityHashSet_contains(m->knownKeys,NULL));
    CHECK(!MCObjectHeap_failed(h));EntityPlayerSP *a=sp(h,1);
    CHECK(!ClassInheritanceMultiMap_add(m,(MCObject *)a)&&MCObjectHeap_failed(h)&&m->values->size==0);
    MCObjectHeap_free(h);
    h=MCObjectHeap_new(4000000);CHECK(h);m=ClassInheritanceMultiMap_new(h,NativeJavaClass_Entity(h));CHECK(m);
    CHECK(ClassInheritanceMultiMap_createLookup(m,NULL));CHECK(NativeIdentityHashSet_contains(m->knownKeys,NULL));
    CHECK(NativeHashSet_contains(ClassInheritanceMultiMap_nativeKnownClasses(h),NULL));
    CHECK(!ClassInheritanceMultiMap_contains(m,NULL)&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
typedef struct {MCObject object;NativeReferenceList *sentinel;NativeJavaClass *earlyBase;int calls;} Context;
static void context_trace(MCObject *o,MCObjectVisitor v,void *ctx){Context *c=(Context *)o;
    c->sentinel=(NativeReferenceList *)v((MCObject *)c->sentinel,ctx);c->earlyBase=(NativeJavaClass *)v((MCObject *)c->earlyBase,ctx);}
static const MCObjectClass contextClass={"fixture.ClassMapContext",MCObjectHeap_plainClone,context_trace,NULL};
static NativeHashMap *new_map(MCObject *o,ClassInheritanceMultiMap *m){Context *c=(Context *)o;++c->calls;
    m->values=c->sentinel;m->baseClass=c->earlyBase;return NativeHashMap_new(o->heap,NATIVE_HASH_KEY_IDENTITY);}
static NativeIdentityHashSet *fail_keys(MCObject *o,ClassInheritanceMultiMap *m){(void)m;++((Context *)o)->calls;return NULL;}
static const ClassInheritanceMultiMapDependencies failingConstructor={.newMap=new_map,.newKnownKeys=fail_keys};
static NativeJavaClass *swap_map(MCObject *o,ClassInheritanceMultiMap *m,NativeJavaClass *c){
    ++((Context *)o)->calls;m->map=NativeHashMap_new(o->heap,NATIVE_HASH_KEY_IDENTITY);return c;}
static const ClassInheritanceMultiMapDependencies lookupSwap={.initializeClassLookup=swap_map};
static ClassInheritanceMultiMapIterable *null_iterable(MCObject *o,ClassInheritanceMultiMap *m,NativeJavaClass *c){
    (void)m;(void)c;++((Context *)o)->calls;return NULL;}
static const ClassInheritanceMultiMapDependencies nullIterable={.getByClass=null_iterable};
static void captured_receivers_and_failure_prefixes(void){
    MCObjectHeap *h=MCObjectHeap_new(4000000);CHECK(h);
    Context *ctx=(Context *)MCObjectHeap_alloc(h,sizeof(*ctx),&contextClass);CHECK(ctx);
    ctx->sentinel=NativeReferenceList_new(h);ctx->earlyBase=NativeJavaClass_Object(h);CHECK(ctx->sentinel&&ctx->earlyBase);
    ClassInheritanceMultiMap *m=ClassInheritanceMultiMap_nativeAllocate(h,&failingConstructor,(MCObject *)ctx);CHECK(m);
    CHECK(!ClassInheritanceMultiMap_construct(m,NativeJavaClass_Entity(h))&&MCObjectHeap_failed(h));
    CHECK(ctx->calls==2&&m->map&&!m->knownKeys&&m->values==ctx->sentinel&&m->baseClass==ctx->earlyBase);
    MCObjectHeap_free(h);
    h=MCObjectHeap_new(4000000);CHECK(h);ctx=(Context *)MCObjectHeap_alloc(h,sizeof(*ctx),&contextClass);CHECK(ctx);
    m=ClassInheritanceMultiMap_nativeAllocate(h,&lookupSwap,(MCObject *)ctx);CHECK(m);
    CHECK(ClassInheritanceMultiMap_construct(m,NativeJavaClass_Entity(h)));EntityPlayerSP *a=sp(h,77);
    CHECK(ClassInheritanceMultiMap_add(m,(MCObject *)a));NativeHashMap *old=m->map;
    ClassInheritanceMultiMapIterator *it=ClassInheritanceMultiMapIterable_iterator(ClassInheritanceMultiMap_getByClass(m,m->baseClass));
    MCObject *out=NULL;CHECK(it&&m->map!=old&&ctx->calls==1);
    CHECK(ClassInheritanceMultiMapIterator_next(it,&out)&&out==(MCObject *)a&&!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h=MCObjectHeap_new(4000000);CHECK(h);ctx=(Context *)MCObjectHeap_alloc(h,sizeof(*ctx),&contextClass);CHECK(ctx);
    m=ClassInheritanceMultiMap_nativeAllocate(h,&nullIterable,(MCObject *)ctx);CHECK(m);
    CHECK(ClassInheritanceMultiMap_construct(m,NativeJavaClass_Entity(h)));a=sp(h,78);
    CHECK(!ClassInheritanceMultiMap_contains(m,(MCObject *)a)&&ctx->calls==1&&MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void invalid_captured_map_receiver_preserves_argument_effects(void){
    MCObjectHeap *h=MCObjectHeap_new(4000000);CHECK(h);
    Context *ctx=(Context *)MCObjectHeap_alloc(h,sizeof(*ctx),&contextClass);CHECK(ctx);
    ClassInheritanceMultiMap *m=ClassInheritanceMultiMap_nativeAllocate(h,&lookupSwap,(MCObject *)ctx);CHECK(m);
    CHECK(ClassInheritanceMultiMap_construct(m,NativeJavaClass_Entity(h)));
    ClassInheritanceMultiMapIterable *deferred=ClassInheritanceMultiMap_getByClass(m,m->baseClass);CHECK(deferred);
    m->map=NULL;MCObjectHeap_touch(h);
    CHECK(!ClassInheritanceMultiMapIterable_iterator(deferred)&&MCObjectHeap_failed(h));
    CHECK(ctx->calls==1&&m->map); /* argument callback ran before NULL Map.get */
    MCObjectHeap_free(h);

    h=MCObjectHeap_new(4000000);MCObjectHeap *other=MCObjectHeap_new(4000000);CHECK(h&&other);
    ctx=(Context *)MCObjectHeap_alloc(h,sizeof(*ctx),&contextClass);CHECK(ctx);
    m=ClassInheritanceMultiMap_nativeAllocate(h,&lookupSwap,(MCObject *)ctx);CHECK(m);
    CHECK(ClassInheritanceMultiMap_construct(m,NativeJavaClass_Entity(h)));
    deferred=ClassInheritanceMultiMap_getByClass(m,m->baseClass);CHECK(deferred);
    NativeHashMap *foreignMap=NativeHashMap_new(other,NATIVE_HASH_KEY_IDENTITY);CHECK(foreignMap);
    m->map=foreignMap;MCObjectHeap_touch(h);
    CHECK(!ClassInheritanceMultiMapIterable_iterator(deferred)&&MCObjectHeap_failed(h));
    CHECK(ctx->calls==1&&m->map&&m->map!=foreignMap&&!MCObjectHeap_failed(other));
    MCObjectHeap_free(h);MCObjectHeap_free(other);
}
static void graph_lifecycle(void){
    MCObjectHeap *h=MCObjectHeap_new(4000000);CHECK(h);
    ClassInheritanceMultiMap *m=ClassInheritanceMultiMap_new(h,NativeJavaClass_Entity(h));CHECK(m);
    EntityPlayerSP *a=sp(h,4);CHECK(ClassInheritanceMultiMap_add(m,(MCObject *)a));
    NativeJavaClass *sc=NativeJavaClass_getClass(h,(MCObject *)a);
    ClassInheritanceMultiMapIterable *iterable=ClassInheritanceMultiMap_getByClass(m,sc);CHECK(iterable);
    ClassInheritanceMultiMapIterator *it=ClassInheritanceMultiMapIterable_iterator(iterable);CHECK(it);
    CHECK(ClassInheritanceMultiMapIterator_hasNext(it));
    MCObjectRoot mr={0},ir={0},vr={0};CHECK(MCObjectRoot_init(&mr,h,(MCObject *)m));
    CHECK(MCObjectRoot_init(&ir,h,(MCObject *)it));CHECK(MCObjectRoot_init(&vr,h,(MCObject *)iterable));
    CHECK(MCObjectHeap_collect(h));CHECK(lookup(m,m->baseClass)==m->values);
    MCObjectHeap *copy=MCObjectHeap_clone(h);CHECK(copy);
    MCObjectRoot cm={0},ci={0},cv={0};CHECK(MCObjectRoot_rebind(&cm,copy,&mr));
    CHECK(MCObjectRoot_rebind(&ci,copy,&ir));CHECK(MCObjectRoot_rebind(&cv,copy,&vr));
    ClassInheritanceMultiMap *clone=(ClassInheritanceMultiMap *)MCObjectRoot_get(&cm);CHECK(clone&&clone!=m);
    ClassInheritanceMultiMapIterable *iv=(ClassInheritanceMultiMapIterable *)MCObjectRoot_get(&cv);
    CHECK(iv->owner==clone&&iv->clazz!=sc);CHECK(lookup(clone,clone->baseClass)==clone->values);
    MCObject *value=NULL;CHECK(ClassInheritanceMultiMapIterator_next((ClassInheritanceMultiMapIterator *)MCObjectRoot_get(&ci),&value));
    CHECK(value==clone->values->storage->items[0]&&value!=(MCObject *)a);
    CHECK(NativeHashSet_contains(ClassInheritanceMultiMap_nativeKnownClasses(copy),(MCObject *)iv->clazz));
    CHECK(MCObjectHeap_canAdopt(h,copy));CHECK(MCObjectHeap_adopt(h,copy));MCObjectHeap_free(copy);
    m=(ClassInheritanceMultiMap *)MCObjectRoot_get(&mr);iterable=(ClassInheritanceMultiMapIterable *)MCObjectRoot_get(&vr);
    CHECK(iterable->owner==m&&NativeHashSet_contains(ClassInheritanceMultiMap_nativeKnownClasses(h),(MCObject *)iterable->clazz));
    CHECK(lookup(m,m->baseClass)==m->values);CHECK(MCObjectHeap_collect(h));
    MCObjectRoot_drop(&mr);MCObjectRoot_drop(&ir);MCObjectRoot_drop(&vr);CHECK(MCObjectHeap_collect(h));
    CHECK(!MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
typedef struct {MCObject object;ClassInheritanceMultiMap *owner;int calls;bool staticPrefix;} GuardContext;
static void guard_trace(MCObject *o,MCObjectVisitor v,void *ctx){
    GuardContext *c=(GuardContext *)o;c->owner=(ClassInheritanceMultiMap *)v((MCObject *)c->owner,ctx);
}
static const MCObjectClass guardClass={"fixture.ClassMapGuardContext",MCObjectHeap_plainClone,guard_trace,NULL};
static GuardContext *foreignContext;
static NativeHashMap *guard_swap_new_map(MCObject *o,ClassInheritanceMultiMap *m){
    ++((GuardContext *)o)->calls;m->dependencyContext=(MCObject *)foreignContext;MCObjectHeap_touch(o->heap);
    return NativeHashMap_new(o->heap,NATIVE_HASH_KEY_IDENTITY);
}
static NativeIdentityHashSet *guard_count_new_keys(MCObject *o,ClassInheritanceMultiMap *m){
    (void)m;++((GuardContext *)o)->calls;MCObjectHeap_touch(o->heap);return NativeIdentityHashSet_new(o->heap);
}
static NativeJavaClass *guard_swap_object_class(MCObject *o,MCObject *value){
    GuardContext *c=(GuardContext *)o;++c->calls;NativeJavaClass *actual=NativeJavaClass_getClass(o->heap,value);
    c->staticPrefix=NativeHashSet_contains(ClassInheritanceMultiMap_nativeKnownClasses(o->heap),(MCObject *)actual);
    c->owner->dependencyContext=(MCObject *)foreignContext;MCObjectHeap_touch(o->heap);return actual;
}
static bool guard_count_assignable(MCObject *o,NativeJavaClass *target,NativeJavaClass *candidate,bool *out){
    ++((GuardContext *)o)->calls;MCObjectHeap_touch(o->heap);return NativeJavaClass_isAssignableFrom(target,candidate,out);
}
static const ClassInheritanceMultiMapDependencies guardConstructor={.newMap=guard_swap_new_map,.newKnownKeys=guard_count_new_keys};
static const ClassInheritanceMultiMapDependencies guardLookup={.objectGetClass=guard_swap_object_class,.classIsAssignableFrom=guard_count_assignable};
static const ClassInheritanceMultiMapDependencies guardNullable={.objectGetClass=guard_swap_object_class};
static void foreign_context_stops_before_the_next_callback(void){
    MCObjectHeap *h=MCObjectHeap_new(4000000),*otherHeap=MCObjectHeap_new(4000000);CHECK(h&&otherHeap);
    GuardContext *ctx=(GuardContext *)MCObjectHeap_alloc(h,sizeof(*ctx),&guardClass);CHECK(ctx);
    foreignContext=(GuardContext *)MCObjectHeap_alloc(otherHeap,sizeof(*foreignContext),&guardClass);CHECK(foreignContext);
    ClassInheritanceMultiMap *m=ClassInheritanceMultiMap_nativeAllocate(h,&guardConstructor,(MCObject *)ctx);CHECK(m);ctx->owner=m;
    CHECK(!ClassInheritanceMultiMap_construct(m,NativeJavaClass_Entity(h))&&MCObjectHeap_failed(h));
    CHECK(ctx->calls==1&&foreignContext->calls==0&&!MCObjectHeap_failed(otherHeap));
    CHECK(m->map&&!m->knownKeys&&!m->values&&!m->baseClass&&m->dependencyContext==(MCObject *)foreignContext);
    MCObjectHeap_free(h);MCObjectHeap_free(otherHeap);

    h=MCObjectHeap_new(4000000);otherHeap=MCObjectHeap_new(4000000);CHECK(h&&otherHeap);
    ctx=(GuardContext *)MCObjectHeap_alloc(h,sizeof(*ctx),&guardClass);CHECK(ctx);
    foreignContext=(GuardContext *)MCObjectHeap_alloc(otherHeap,sizeof(*foreignContext),&guardClass);CHECK(foreignContext);
    m=ClassInheritanceMultiMap_nativeAllocate(h,NULL,(MCObject *)ctx);CHECK(m);ctx->owner=m;
    CHECK(ClassInheritanceMultiMap_construct(m,NativeJavaClass_Entity(h)));
    EntityPlayerSP *a=sp(h,91);CHECK(ClassInheritanceMultiMap_add(m,(MCObject *)a));
    NativeJavaClass *sc=NativeJavaClass_getClass(h,(MCObject *)a);CHECK(sc);
    m->dependencies=&guardLookup;MCObjectHeap_touch(h);
    CHECK(!ClassInheritanceMultiMap_createLookup(m,sc)&&MCObjectHeap_failed(h));
    CHECK(ctx->calls==1&&foreignContext->calls==0&&!MCObjectHeap_failed(otherHeap));
    CHECK(m->values->size==1&&m->values->storage->items[0]==(MCObject *)a);
    /* The Source static add precedes the callback; it remains after failure. */
    CHECK(ctx->staticPrefix);
    MCObjectHeap_free(h);MCObjectHeap_free(otherHeap);foreignContext=NULL;

    h=MCObjectHeap_new(4000000);CHECK(h);ctx=(GuardContext *)MCObjectHeap_alloc(h,sizeof(*ctx),&guardClass);CHECK(ctx);
    m=ClassInheritanceMultiMap_nativeAllocate(h,NULL,(MCObject *)ctx);CHECK(m);ctx->owner=m;
    CHECK(ClassInheritanceMultiMap_construct(m,NativeJavaClass_Entity(h)));
    a=sp(h,93);CHECK(ClassInheritanceMultiMap_add(m,(MCObject *)a));sc=NativeJavaClass_getClass(h,(MCObject *)a);CHECK(sc);
    m->dependencies=&guardNullable;MCObjectHeap_touch(h);
    CHECK(ClassInheritanceMultiMap_createLookup(m,sc)&&!MCObjectHeap_failed(h));
    CHECK(ctx->calls==1&&ctx->staticPrefix&&!m->dependencyContext&&lookup(m,sc)->size==1);
    MCObjectHeap_free(h);

    h=MCObjectHeap_new(4000000);CHECK(h);ctx=(GuardContext *)MCObjectHeap_alloc(h,sizeof(*ctx),&guardClass);CHECK(ctx);
    foreignContext=(GuardContext *)MCObjectHeap_alloc(h,sizeof(*foreignContext),&guardClass);CHECK(foreignContext);
    m=ClassInheritanceMultiMap_nativeAllocate(h,NULL,(MCObject *)ctx);CHECK(m);ctx->owner=m;foreignContext->owner=m;
    CHECK(ClassInheritanceMultiMap_construct(m,NativeJavaClass_Entity(h)));
    a=sp(h,94);CHECK(ClassInheritanceMultiMap_add(m,(MCObject *)a));sc=NativeJavaClass_getClass(h,(MCObject *)a);CHECK(sc);
    m->dependencies=&guardLookup;MCObjectHeap_touch(h);
    CHECK(ClassInheritanceMultiMap_createLookup(m,sc)&&!MCObjectHeap_failed(h));
    CHECK(ctx->calls==1&&foreignContext->calls==1&&m->dependencyContext==(MCObject *)foreignContext);
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)m));CHECK(MCObjectHeap_collect(h));
    MCObjectHeap *copy=MCObjectHeap_clone(h);CHECK(copy);MCObjectRoot copyRoot={0};CHECK(MCObjectRoot_rebind(&copyRoot,copy,&root));
    ClassInheritanceMultiMap *copied=(ClassInheritanceMultiMap *)MCObjectRoot_get(&copyRoot);CHECK(copied&&copied!=m);
    GuardContext *copiedContext=(GuardContext *)copied->dependencyContext;
    CHECK(copiedContext&&copiedContext!=foreignContext&&copiedContext->owner==copied&&copiedContext->calls==1);
    CHECK(MCObjectHeap_adopt(h,copy));MCObjectHeap_free(copy);CHECK(MCObjectHeap_collect(h));
    MCObjectHeap_free(h);foreignContext=NULL;
}
int main(void){original_collection_behavior();direct_lookup_and_iterator_failures();deferred_failure_and_nullable_constructor();
    captured_receivers_and_failure_prefixes();invalid_captured_map_receiver_preserves_argument_effects();
    graph_lifecycle();foreign_context_stops_before_the_next_callback();
    printf("Source ClassInheritanceMultiMap: %d checks\n",checks);return 0;}
