#include "util/ObjectIntIdentityMap.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"object id map check %u failed line %d: %s\n",checks,__LINE__,#x);exit(1);}}while(0)
static const MCObjectClass leaf={"test.stateIdentity",MCObjectHeap_plainClone,NULL,NULL};
static void actual_methods(void){
 MCObjectHeap *h=MCObjectHeap_new(1024u*1024u);CHECK(h);
 ObjectIntIdentityMap *m=ObjectIntIdentityMap_new(h);CHECK(m);
 MCObject *a=MCObjectHeap_alloc(h,sizeof(MCObject),&leaf),*b=MCObjectHeap_alloc(h,sizeof(MCObject),&leaf);CHECK(a&&b);
 CHECK(ObjectIntIdentityMap_get(m,a)==-1&&ObjectIntIdentityMap_getByValue(m,0)==NULL);
 CHECK(ObjectIntIdentityMap_put(m,a,3));CHECK(ObjectIntIdentityMap_get(m,a)==3);
 CHECK(ObjectIntIdentityMap_getByValue(m,0)==NULL&&ObjectIntIdentityMap_getByValue(m,3)==a);
 CHECK(ObjectIntIdentityMap_put(m,b,3));CHECK(ObjectIntIdentityMap_getByValue(m,3)==b&&ObjectIntIdentityMap_get(m,a)==3);
 CHECK(ObjectIntIdentityMap_put(m,b,5));CHECK(ObjectIntIdentityMap_getByValue(m,3)==b&&ObjectIntIdentityMap_getByValue(m,5)==b&&ObjectIntIdentityMap_get(m,b)==5);
 CHECK(ObjectIntIdentityMap_put(m,NULL,2));CHECK(ObjectIntIdentityMap_get(m,NULL)==2&&ObjectIntIdentityMap_getByValue(m,2)==NULL);
 ObjectIntIdentityIterator *it=ObjectIntIdentityMap_iterator(m);CHECK(it);
 MCObject *out=NULL;CHECK(ObjectIntIdentityIterator_hasNext(it)&&ObjectIntIdentityIterator_next(it,&out)&&out==b);
 CHECK(ObjectIntIdentityIterator_hasNext(it)&&ObjectIntIdentityIterator_next(it,&out)&&out==b);
 CHECK(!ObjectIntIdentityIterator_hasNext(it)&&!MCObjectHeap_failed(h));
 MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)m));CHECK(MCObjectHeap_collect(h));
 MCObjectHeap *clone=MCObjectHeap_clone(h);CHECK(clone);MCObjectRoot croot={0};CHECK(MCObjectRoot_rebind(&croot,clone,&root));
 ObjectIntIdentityMap *cm=(ObjectIntIdentityMap *)MCObjectRoot_get(&croot);MCObject *cb=ObjectIntIdentityMap_getByValue(cm,3);CHECK(cb&&cb!=b&&ObjectIntIdentityMap_getByValue(cm,5)==cb);
 CHECK(ObjectIntIdentityMap_get(cm,cb)==5&&ObjectIntIdentityMap_get(cm,NULL)==2);
 CHECK(MCObjectHeap_canAdopt(h,clone)&&MCObjectHeap_adopt(h,clone));m=(ObjectIntIdentityMap *)MCObjectRoot_get(&root);
 CHECK(ObjectIntIdentityMap_getByValue(m,3)==ObjectIntIdentityMap_getByValue(m,5));
 MCObjectHeap_free(clone);MCObjectRoot_drop(&root);CHECK(MCObjectHeap_collect(h));MCObjectHeap_free(h);
}
static void failure_prefix(void){
 MCObjectHeap *h=MCObjectHeap_new(1024u*1024u);ObjectIntIdentityMap *m=ObjectIntIdentityMap_new(h);CHECK(m);
 MCObject *a=MCObjectHeap_alloc(h,sizeof(MCObject),&leaf);CHECK(a);
 CHECK(!ObjectIntIdentityMap_put(m,a,-2)&&MCObjectHeap_failed(h));
 /* Negative list index throws after the real identity-map mutation. */
 CHECK(m->objectList->size==0);CHECK(m->identityMap!=NULL);
 MCObjectHeap_free(h);
 h=MCObjectHeap_new(1024u*1024u);m=ObjectIntIdentityMap_new(h);CHECK(m);m->objectList=NULL;
 CHECK(ObjectIntIdentityMap_getByValue(m,-1)==NULL&&!MCObjectHeap_failed(h));
 CHECK(ObjectIntIdentityMap_get(m,NULL)==-1&&!MCObjectHeap_failed(h));
 CHECK(ObjectIntIdentityMap_getByValue(m,0)==NULL&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
}

static void cached_iterator_and_live_list(void){
 MCObjectHeap*h=MCObjectHeap_new(1024u*1024u);ObjectIntIdentityMap*m=ObjectIntIdentityMap_new(h);CHECK(m);MCObject*a=MCObjectHeap_alloc(h,sizeof(MCObject),&leaf),*b=MCObjectHeap_alloc(h,sizeof(MCObject),&leaf);CHECK(a&&b);
 CHECK(ObjectIntIdentityMap_put(m,a,0));CHECK(ObjectIntIdentityMap_put(m,b,1));ObjectIntIdentityIterator*i=ObjectIntIdentityMap_iterator(m);CHECK(i&&ObjectIntIdentityIterator_hasNext(i));
 CHECK(ObjectIntIdentityMap_put(m,b,0));MCObject*out=NULL;CHECK(ObjectIntIdentityIterator_next(i,&out)&&out==a);
 CHECK(ObjectIntIdentityIterator_next(i,&out)&&out==b);CHECK(!ObjectIntIdentityIterator_hasNext(i)&&!MCObjectHeap_failed(h));
 i=ObjectIntIdentityMap_iterator(m);CHECK(i&&ObjectIntIdentityIterator_hasNext(i));CHECK(ObjectIntIdentityMap_put(m,a,3));CHECK(ObjectIntIdentityIterator_next(i,&out)&&out==b);
 out=a;CHECK(!ObjectIntIdentityIterator_next(i,&out)&&out==a&&MCObjectHeap_failed(h));CHECK(!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);
}
static void constructor_failure_and_ownership(void){
 /* Find the precise source map-to-list assignment boundary without changing the
    native allocator or treating a failed constructor as a usable instance. */
 bool mapAssigned=false,success=false;
 for(size_t budget=16000;budget<=20000;budget+=16){
  MCObjectHeap*h=MCObjectHeap_new(budget);ObjectIntIdentityMap*m=ObjectIntIdentityMap_nativeAllocate(h);CHECK(m);bool ok=ObjectIntIdentityMap_construct(m);
  if(!ok){CHECK(MCObjectHeap_failed(h)&&!MCObjectHeap_hasBorrowers(h));if(m->identityMap){CHECK(m->objectList==NULL);mapAssigned=true;}}
  else{CHECK(m->identityMap&&m->objectList&&m->objectList->size==0);success=true;MCObjectHeap_free(h);break;}
  MCObjectHeap_free(h);
 }
 CHECK(mapAssigned&&success);
 MCObjectHeap*h=MCObjectHeap_new(1024u*1024u);ObjectIntIdentityMap*m=ObjectIntIdentityMap_new(h);MCObjectHeap*f=MCObjectHeap_new(4096);MCObject*other=MCObjectHeap_alloc(f,sizeof(MCObject),&leaf);CHECK(m&&other);
 CHECK(!ObjectIntIdentityMap_put(m,other,3)&&m->objectList->size==0&&MCObjectHeap_failed(h)&&!MCObjectHeap_failed(f));MCObjectHeap_free(f);MCObjectHeap_free(h);
 h=MCObjectHeap_new(1024u*1024u);m=ObjectIntIdentityMap_new(h);CHECK(m);MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject*)m));CHECK(ObjectIntIdentityMap_put(m,NULL,0));CHECK(MCObjectHeap_collect(h));size_t baseline=MCObjectHeap_liveObjects(h);
 for(int j=0;j<100;j++){ObjectIntIdentityIterator*it=ObjectIntIdentityMap_iterator(m);CHECK(it&&!ObjectIntIdentityIterator_hasNext(it));}
 CHECK(MCObjectHeap_collect(h)&&MCObjectHeap_liveObjects(h)==baseline);MCObjectRoot_drop(&root);CHECK(MCObjectHeap_collect(h));
 /* Source Java Integer cache + NULL-key class-static refs remain live. */
 CHECK(MCObjectHeap_liveObjects(h)==259);MCObjectHeap_free(h);
}
int main(void){actual_methods();failure_prefix();cached_iterator_and_live_list();constructor_failure_and_ownership();printf("source object int identity map: %u checks passed\n",checks);return 0;}
