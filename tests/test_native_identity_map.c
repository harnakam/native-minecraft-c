#include "util/NativeIdentityHashSet.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"identity check %u failed line %d: %s\n",checks,__LINE__,#x);exit(1);}}while(0)
typedef struct {MCObject object;int32_t value;} Key;
static const MCObjectClass keyClass={"test.IdentityKey",MCObjectHeap_plainClone,NULL,NULL};
static Key *key(MCObjectHeap *h,int32_t v){Key *k=(Key *)MCObjectHeap_alloc(h,sizeof(*k),&keyClass);if(k)k->value=v;return k;}
static void map_and_iteration(void){
 MCObjectHeap *h=MCObjectHeap_new(4u*1024u*1024u);CHECK(h);
 NativeIdentityHashMap *m=NativeIdentityHashMap_new(h,0);CHECK(m);
 Key *a=key(h,7),*b=key(h,7),*v=key(h,9);CHECK(a&&b&&v);
 CHECK(NativeIdentityHashMap_put(m,(MCObject *)a,(MCObject *)v));
 CHECK(NativeIdentityHashMap_get(m,(MCObject *)b)==NULL&&!NativeIdentityHashMap_containsKey(m,(MCObject *)b));
 CHECK(NativeIdentityHashMap_get(m,(MCObject *)a)==(MCObject *)v);
 MCObject *prev=(MCObject *)b;CHECK(NativeIdentityHashMap_putWithPrevious(m,NULL,NULL,&prev)&&prev==NULL);
 CHECK(NativeIdentityHashMap_containsKey(m,NULL)&&NativeIdentityHashMap_get(m,NULL)==NULL);
 CHECK(NativeIdentityHashMap_putWithPrevious(m,(MCObject *)a,(MCObject *)a,&prev)&&prev==(MCObject *)v);
 Key *keys[256];for(int i=0;i<256;i++){keys[i]=key(h,i);CHECK(keys[i]&&NativeIdentityHashMap_put(m,(MCObject *)keys[i],(MCObject *)keys[i]));}
 CHECK(NativeIdentityHashMap_size(m)==258);
 for(int i=0;i<256;i++)CHECK(NativeIdentityHashMap_get(m,(MCObject *)keys[i])==(MCObject *)keys[i]);
 for(int i=0;i<256;i+=2)CHECK(NativeIdentityHashMap_remove(m,(MCObject *)keys[i])==(MCObject *)keys[i]);
 for(int i=0;i<256;i++)CHECK(NativeIdentityHashMap_containsKey(m,(MCObject *)keys[i])==((i&1)!=0));
 NativeIdentityKeySet *view=NativeIdentityHashMap_keySet(m);CHECK(view==NativeIdentityHashMap_keySet(m));
 NativeIdentityIterator *it=NativeIdentityKeySet_iterator(view);CHECK(it);
 int seen=0;bool nullSeen=false;while(NativeIdentityIterator_hasNext(it)){MCObject *got=(MCObject *)b;CHECK(NativeIdentityIterator_next(it,&got));if(!got){CHECK(!nullSeen);nullSeen=true;}seen++;}
 CHECK(seen==130&&nullSeen);
 MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)m));
 CHECK(MCObjectHeap_collect(h));MCObjectHeap *clone=MCObjectHeap_clone(h);CHECK(clone);
 MCObjectRoot cr={0};CHECK(MCObjectRoot_rebind(&cr,clone,&root));m=(NativeIdentityHashMap *)MCObjectRoot_get(&cr);
 it=NativeIdentityKeySet_iterator(NativeIdentityHashMap_keySet(m));CHECK(it);seen=0;
 while(NativeIdentityIterator_hasNext(it)){MCObject *got=NULL;CHECK(NativeIdentityIterator_next(it,&got));if(got)CHECK(NativeIdentityHashMap_get(m,got)==got);seen++;}
 CHECK(seen==130&&NativeIdentityHashMap_containsKey(m,NULL));
 CHECK(MCObjectHeap_canAdopt(h,clone)&&MCObjectHeap_adopt(h,clone));m=(NativeIdentityHashMap *)MCObjectRoot_get(&root);
 CHECK(NativeIdentityHashMap_size(m)==130);CHECK(NativeIdentityHashMap_clear(m)&&NativeIdentityHashMap_size(m)==0);
 CHECK(MCObjectHeap_collect(h));MCObjectRoot_drop(&root);CHECK(MCObjectHeap_collect(h));
 /* The canonical NULL-key sentinel is a genuine static root. */
 CHECK(MCObjectHeap_liveObjects(h)==1);
 MCObjectHeap_free(clone);MCObjectHeap_free(h);
}
static void set_and_failures(void){
 MCObjectHeap *h=MCObjectHeap_new(65536);NativeIdentityHashSet *s=NativeIdentityHashSet_new(h);CHECK(s);
 Key *a=key(h,1),*b=key(h,1);bool changed=false;CHECK(a&&b);
 CHECK(NativeIdentityHashSet_add(s,(MCObject *)a,&changed)&&changed);
 CHECK(NativeIdentityHashSet_add(s,(MCObject *)a,&changed)&&!changed);
 CHECK(NativeIdentityHashSet_add(s,(MCObject *)b,&changed)&&changed);
 CHECK(NativeIdentityHashSet_add(s,NULL,&changed)&&changed);CHECK(NativeIdentityHashSet_size(s)==3);
 CHECK(NativeIdentityHashSet_remove(s,(MCObject *)a,&changed)&&changed);CHECK(!NativeIdentityHashSet_contains(s,(MCObject *)a));
 NativeIdentityIterator *it=NativeIdentityHashSet_iterator(s);CHECK(it);
 CHECK(NativeIdentityHashSet_add(s,(MCObject *)a,&changed)&&changed);
 MCObject *out=(MCObject *)a;CHECK(!NativeIdentityIterator_next(it,&out));CHECK(out==(MCObject *)a&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
 h=MCObjectHeap_new(65536);MCObjectHeap *other=MCObjectHeap_new(4096);NativeIdentityHashMap *m=NativeIdentityHashMap_new(h,21);a=key(other,1);CHECK(m&&a);
 out=(MCObject *)a;CHECK(!NativeIdentityHashMap_putWithPrevious(m,(MCObject *)a,NULL,&out));CHECK(out==(MCObject *)a&&MCObjectHeap_failed(h)&&!MCObjectHeap_failed(other));MCObjectHeap_free(h);MCObjectHeap_free(other);
 h=MCObjectHeap_new(4096);CHECK(!NativeIdentityHashMap_new(h,-1)&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
}

static void retained_table_and_allocation_failure(void){
 MCObjectHeap*h=MCObjectHeap_new(65536);NativeIdentityHashMap*m=NativeIdentityHashMap_new(h,0);Key*a=key(h,0),*b=key(h,1),*c=key(h,2);CHECK(m&&a&&b&&c);
 CHECK(NativeIdentityHashMap_put(m,(MCObject*)a,NULL)&&NativeIdentityHashMap_put(m,(MCObject*)b,NULL));NativeIdentityIterator*it=NativeIdentityKeySet_iterator(NativeIdentityHashMap_keySet(m));CHECK(it&&NativeIdentityIterator_hasNext(it));
 CHECK(NativeIdentityHashMap_put(m,(MCObject*)c,NULL));CHECK(!NativeIdentityIterator_hasNext(it)&&!MCObjectHeap_failed(h));MCObject*out=(MCObject*)a;CHECK(!NativeIdentityIterator_next(it,&out)&&out==(MCObject*)a&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
 h=MCObjectHeap_new(65536);m=NativeIdentityHashMap_new(h,0);a=key(h,0);b=key(h,1);c=key(h,2);CHECK(m&&a&&b&&c);size_t bytes=MCObjectHeap_liveBytes(h);MCObjectHeap_free(h);
 h=MCObjectHeap_new(bytes+sizeof(NativeObjectArray)+16*sizeof(MCObject*)-1);m=NativeIdentityHashMap_new(h,0);a=key(h,0);b=key(h,1);c=key(h,2);CHECK(m&&a&&b&&c);
 CHECK(NativeIdentityHashMap_put(m,(MCObject*)a,NULL)&&NativeIdentityHashMap_put(m,(MCObject*)b,NULL));out=(MCObject*)a;
 CHECK(!NativeIdentityHashMap_putWithPrevious(m,(MCObject*)c,NULL,&out)&&out==(MCObject*)a&&MCObjectHeap_failed(h));CHECK(!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);
 h=MCObjectHeap_new(65536);m=NativeIdentityHashMap_new(h,0);a=key(h,0);CHECK(m&&a);CHECK(NativeIdentityHashMap_put(m,(MCObject*)a,NULL));it=NativeIdentityKeySet_iterator(NativeIdentityHashMap_keySet(m));CHECK(it);
 CHECK(NativeIdentityHashMap_put(m,(MCObject*)a,(MCObject*)a));out=NULL;CHECK(NativeIdentityIterator_next(it,&out)&&out==(MCObject*)a&&!MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
int main(void){map_and_iteration();set_and_failures();retained_table_and_allocation_failure();printf("native identity map: %u checks passed\n",checks);return 0;}
