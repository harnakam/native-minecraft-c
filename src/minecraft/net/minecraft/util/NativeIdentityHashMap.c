#include "util/NativeIdentityHashMap.h"
#include <limits.h>
#include <string.h>
struct NativeIdentityHashMap {
    MCObject object;
    NativeObjectArray *table;
    int32_t size;
    uint32_t modCount;
    MCObject *nullKey;
    NativeIdentityKeySet *keys;
};
struct NativeIdentityKeySet {MCObject object;NativeIdentityHashMap *map;};
struct NativeIdentityIterator {
    MCObject object;NativeIdentityHashMap *map;NativeObjectArray *traversalTable;
    int32_t index;uint32_t expectedModCount;bool indexValid;
};
static const MCObjectClass mapClass,keySetClass,iteratorClass;
static const MCObjectClass nullClass={"native.IdentityHashMap.NULL_KEY",MCObjectHeap_plainClone,NULL,NULL};
static bool fail(MCObjectHeap *h){MCObjectHeap_fail(h);return false;}
static bool any(const MCObject *o,void *c){(void)o;(void)c;return true;}
bool NativeIdentityHashMap_isInstance(const MCObject *o){return o&&o->klass==&mapClass&&MCObjectHeap_objectSize(o)>=sizeof(NativeIdentityHashMap);}
bool NativeIdentityKeySet_isInstance(const MCObject *o){return o&&o->klass==&keySetClass&&MCObjectHeap_objectSize(o)>=sizeof(NativeIdentityKeySet);}
bool NativeIdentityIterator_isInstance(const MCObject *o){return o&&o->klass==&iteratorClass&&MCObjectHeap_objectSize(o)>=sizeof(NativeIdentityIterator);}
static bool table_valid(NativeObjectArray *a,MCObjectHeap *h){return NativeObjectArray_isInstance((MCObject *)a)&&a->object.heap==h&&a->length>=8&&(a->length&(a->length-1))==0;}
static bool valid(NativeIdentityHashMap *m){
    MCObjectHeap *h=m?m->object.heap:NULL;
    return (NativeIdentityHashMap_isInstance((MCObject *)m)&&!MCObjectHeap_failed(h)&&table_valid(m->table,h)&&
        m->size>=0&&m->size<m->table->length/2&&m->nullKey&&m->nullKey->heap==h&&m->nullKey->klass==&nullClass)||fail(h);
}
static bool edge(MCObjectHeap *h,MCObject *o){return !o||(o->heap==h&&MCObjectHeap_objectSize(o)>=sizeof(MCObject))||fail(h);}
static void map_trace(MCObject *o,MCObjectVisitor v,void *c){
    if(!NativeIdentityHashMap_isInstance(o)){fail(o->heap);return;}
    NativeIdentityHashMap *m=(NativeIdentityHashMap *)o;m->table=(NativeObjectArray *)v((MCObject *)m->table,c);
    m->nullKey=v(m->nullKey,c);m->keys=(NativeIdentityKeySet *)v((MCObject *)m->keys,c);
}
static void key_trace(MCObject *o,MCObjectVisitor v,void *c){
    if(!NativeIdentityKeySet_isInstance(o)){fail(o->heap);return;}
    NativeIdentityKeySet *s=(NativeIdentityKeySet *)o;s->map=(NativeIdentityHashMap *)v((MCObject *)s->map,c);
}
static void iterator_trace(MCObject *o,MCObjectVisitor v,void *c){
    if(!NativeIdentityIterator_isInstance(o)){fail(o->heap);return;}
    NativeIdentityIterator *i=(NativeIdentityIterator *)o;i->map=(NativeIdentityHashMap *)v((MCObject *)i->map,c);
    i->traversalTable=(NativeObjectArray *)v((MCObject *)i->traversalTable,c);
}
static const MCObjectClass mapClass={"native.IdentityHashMap",MCObjectHeap_plainClone,map_trace,NULL};
static const MCObjectClass keySetClass={"native.IdentityHashMap.KeySet",MCObjectHeap_plainClone,key_trace,NULL};
static const MCObjectClass iteratorClass={"native.IdentityHashMap.KeyIterator",MCObjectHeap_plainClone,iterator_trace,NULL};
static int32_t capacity(int32_t n){
    if(n>INT32_C(536870912)/3)return INT32_C(536870912);
    if(n<=2)return 4;
    uint32_t value=(uint32_t)n*3u,result=1;while(value>>1){value>>=1;result<<=1;}return (int32_t)result;
}
NativeIdentityHashMap *NativeIdentityHashMap_new(MCObjectHeap *h,int32_t expectedMaxSize){
    if(expectedMaxSize<0){fail(h);return NULL;}
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    NativeIdentityHashMap *m=(NativeIdentityHashMap *)MCObjectHeap_alloc(h,sizeof(*m),&mapClass);
    MCObject *nullKey=MCObjectHeap_findObject(h,&nullClass,any,NULL);
    if(m&&!nullKey){nullKey=MCObjectHeap_alloc(h,sizeof(MCObject),&nullClass);MCObjectRoot root={0};if(nullKey&&!MCObjectRoot_init(&root,h,nullKey))nullKey=NULL;}
    bool ok=m&&nullKey;
    if(ok){m->nullKey=nullKey;m->table=NativeObjectArray_new(h,capacity(expectedMaxSize)*2);ok=m->table!=NULL;}
    MCObjectRootScope_end(&scope);return ok?m:NULL;
}
static int32_t hash(MCObject *key,int32_t length){uint32_t h=(uint32_t)MCObjectHeap_identityHashCode(key);return (int32_t)(((h<<1)-(h<<8))&((uint32_t)length-1));}
static int32_t next_index(int32_t i,int32_t length){return i+2<length?i+2:0;}
static int32_t find(NativeIdentityHashMap *m,MCObject *key){
    int32_t n=m->table->length,i=hash(key,n);
    for(int32_t count=0;count<n/2;count++){MCObject *got=m->table->values[i];if(got==key||!got)return i;i=next_index(i,n);}
    fail(m->object.heap);return -1;
}
int32_t NativeIdentityHashMap_size(NativeIdentityHashMap *m){return valid(m)?m->size:0;}
MCObject *NativeIdentityHashMap_get(NativeIdentityHashMap *m,MCObject *key){
    if(!valid(m)||!edge(m->object.heap,key))return NULL;
    MCObject *k=key?key:m->nullKey;int32_t i=find(m,k);if(i<0||m->table->values[i]!=k)return NULL;
    MCObject *value=m->table->values[i+1];return edge(m->object.heap,value)?value:NULL;
}
bool NativeIdentityHashMap_containsKey(NativeIdentityHashMap *m,MCObject *key){
    if(!valid(m)||!edge(m->object.heap,key))return false;
    MCObject *k=key?key:m->nullKey;int32_t i=find(m,k);return i>=0&&m->table->values[i]==k;
}
static bool resize(NativeIdentityHashMap *m,bool *changed){
    NativeObjectArray *old=m->table;int32_t n=old->length;*changed=false;
    if(n==INT32_C(1073741824)){if(m->size==INT32_C(536870911))return fail(m->object.heap);return true;}
    NativeObjectArray *fresh=NativeObjectArray_new(m->object.heap,n*2);if(!fresh)return false;
    for(int32_t j=0;j<n;j+=2){MCObject *key=old->values[j];if(!key)continue;
        MCObject *value=old->values[j+1];old->values[j]=NULL;old->values[j+1]=NULL;
        int32_t i=hash(key,fresh->length);while(fresh->values[i])i=next_index(i,fresh->length);
        fresh->values[i]=key;fresh->values[i+1]=value;
    }
    m->table=fresh;MCObjectHeap_touch(m->object.heap);*changed=true;return true;
}
bool NativeIdentityHashMap_putWithPrevious(NativeIdentityHashMap *m,MCObject *key,MCObject *value,MCObject **previous){
    if(!valid(m)||!previous||!edge(m->object.heap,key)||!edge(m->object.heap,value))return fail(m?m->object.heap:NULL);
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,m->object.heap))return false;
    MCObject *k=key?key:m->nullKey;bool ok=false;
    for(;;){int32_t i=find(m,k);if(i<0)break;
        if(m->table->values[i]==k){MCObject *old=m->table->values[i+1];if(!edge(m->object.heap,old))break;
            m->table->values[i+1]=value;MCObjectHeap_touch(m->object.heap);*previous=old;ok=true;break;
        }
        int32_t size=m->size+1;
        if((int64_t)size*3>m->table->length){bool changed;if(!resize(m,&changed))break;if(changed)continue;}
        m->modCount++;m->table->values[i]=k;m->table->values[i+1]=value;m->size=size;
        MCObjectHeap_touch(m->object.heap);*previous=NULL;ok=true;break;
    }
    MCObjectRootScope_end(&scope);return ok;
}
bool NativeIdentityHashMap_put(NativeIdentityHashMap *m,MCObject *key,MCObject *value){MCObject *old=NULL;return NativeIdentityHashMap_putWithPrevious(m,key,value,&old);}
MCObject *NativeIdentityHashMap_remove(NativeIdentityHashMap *m,MCObject *key){
    if(!valid(m)||!edge(m->object.heap,key))return NULL;
    MCObject *k=key?key:m->nullKey;int32_t d=find(m,k);if(d<0||m->table->values[d]!=k)return NULL;
    MCObject *old=m->table->values[d+1];if(!edge(m->object.heap,old))return NULL;
    m->modCount++;m->size--;m->table->values[d]=NULL;m->table->values[d+1]=NULL;
    int32_t n=m->table->length;
    for(int32_t i=next_index(d,n);m->table->values[i];i=next_index(i,n)){
        MCObject *item=m->table->values[i];int32_t r=hash(item,n);
        if((i<r&&(r<=d||d<=i))||(r<=d&&d<=i)){
            m->table->values[d]=item;m->table->values[d+1]=m->table->values[i+1];
            m->table->values[i]=NULL;m->table->values[i+1]=NULL;d=i;
        }
    }
    MCObjectHeap_touch(m->object.heap);return old;
}
bool NativeIdentityHashMap_clear(NativeIdentityHashMap *m){
    if(!valid(m))return false;
    m->modCount++;memset(m->table->values,0,(size_t)m->table->length*sizeof(*m->table->values));m->size=0;MCObjectHeap_touch(m->object.heap);return true;
}
NativeIdentityKeySet *NativeIdentityHashMap_keySet(NativeIdentityHashMap *m){
    if(!valid(m))return NULL;
    if(!m->keys){NativeIdentityKeySet *keys=(NativeIdentityKeySet *)MCObjectHeap_alloc(m->object.heap,sizeof(*keys),&keySetClass);if(!keys)return NULL;keys->map=m;m->keys=keys;MCObjectHeap_touch(m->object.heap);}
    return m->keys;
}
NativeIdentityIterator *NativeIdentityKeySet_iterator(NativeIdentityKeySet *s){
    if(!NativeIdentityKeySet_isInstance((MCObject *)s)||!NativeIdentityHashMap_isInstance((MCObject *)s->map)||
       s->map->object.heap!=s->object.heap||!valid(s->map)){fail(s?s->object.heap:NULL);return NULL;}
    NativeIdentityIterator *i=(NativeIdentityIterator *)MCObjectHeap_alloc(s->object.heap,sizeof(*i),&iteratorClass);
    if(i){i->map=s->map;i->traversalTable=s->map->table;i->index=s->map->size?0:i->traversalTable->length;i->expectedModCount=s->map->modCount;}
    return i;
}
static bool iterator_valid(NativeIdentityIterator *i){
    MCObjectHeap *h=i?i->object.heap:NULL;
    return (NativeIdentityIterator_isInstance((MCObject *)i)&&NativeIdentityHashMap_isInstance((MCObject *)i->map)&&
       i->map->object.heap==h&&!MCObjectHeap_failed(h)&&table_valid(i->traversalTable,h)&&i->index>=0&&i->index<=i->traversalTable->length)||fail(h);
}
bool NativeIdentityIterator_hasNext(NativeIdentityIterator *i){
    if(!iterator_valid(i))return false;
    for(int32_t n=i->index;n<i->traversalTable->length;n+=2)if(i->traversalTable->values[n]){i->index=n;i->indexValid=true;MCObjectHeap_touch(i->object.heap);return true;}
    i->index=i->traversalTable->length;MCObjectHeap_touch(i->object.heap);return false;
}
bool NativeIdentityIterator_next(NativeIdentityIterator *i,MCObject **out){
    if(!iterator_valid(i)||!out)return fail(i?i->object.heap:NULL);
    if(i->map->modCount!=i->expectedModCount)return fail(i->object.heap);
    if(!i->indexValid&&!NativeIdentityIterator_hasNext(i))return fail(i->object.heap);
    int32_t index=i->index;i->indexValid=false;i->index+=2;MCObjectHeap_touch(i->object.heap);
    MCObject *key=i->traversalTable->values[index];if(!edge(i->object.heap,key))return false;
    *out=key==i->map->nullKey?NULL:key;return true;
}
