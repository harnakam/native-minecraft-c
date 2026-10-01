#include "util/ObjectIntIdentityMap.h"
#include "util/NativeHashMap.h"
typedef struct {MCObject object;int32_t value;} NativeInteger;
typedef struct {MCObject object;NativeObjectArray *values;} NativeIntegerCache;
struct ObjectIntIdentityIterator {MCObject object;NativeIterator *input;MCObject *next;int32_t state;};
static const MCObjectClass mapClass,iteratorClass,cacheClass;
static const MCObjectClass integerClass={"native.Integer",MCObjectHeap_plainClone,NULL,NULL};
static bool fail(MCObjectHeap *h){MCObjectHeap_fail(h);return false;}
static bool any(const MCObject *o,void *c){(void)o;(void)c;return true;}
static void cache_trace(MCObject *o,MCObjectVisitor v,void *c){
    if(MCObjectHeap_objectSize(o)<sizeof(NativeIntegerCache)){fail(o->heap);return;}
    NativeIntegerCache *cache=(NativeIntegerCache *)o;cache->values=(NativeObjectArray *)v((MCObject *)cache->values,c);
}
static const MCObjectClass cacheClass={"native.IntegerCache.defaultRange",MCObjectHeap_plainClone,cache_trace,NULL};
/* Native java.lang.Integer/valueOf dependency. The default JDK8 cache range
   is retained; configurable extended IntegerCache and the complete class are
   not claimed. Cache refs are class-static roots in this native graph. */
static NativeInteger *integer_value_of(MCObjectHeap *h,int32_t value){
    if(value>=-128){
        NativeIntegerCache *cache=(NativeIntegerCache *)MCObjectHeap_findObject(h,&cacheClass,any,NULL);
        if(!cache){
            cache=(NativeIntegerCache *)MCObjectHeap_alloc(h,sizeof(*cache),&cacheClass);if(!cache)return NULL;
            NativeObjectArray *array=NativeObjectArray_new(h,256);if(!array)return NULL;cache->values=array;
            for(int32_t i=0;i<256;i++){NativeInteger *box=(NativeInteger *)MCObjectHeap_alloc(h,sizeof(*box),&integerClass);if(!box)return NULL;box->value=i-128;array->values[i]=(MCObject *)box;}
            MCObjectRoot root={0};if(!MCObjectRoot_init(&root,h,(MCObject *)cache))return NULL;
        }
        if(MCObjectHeap_objectSize((MCObject *)cache)<sizeof(*cache)){fail(h);return NULL;}
        if(!NativeObjectArray_isInstance((MCObject *)cache->values)||cache->values->object.heap!=h||cache->values->length!=256){fail(h);return NULL;}
        if(value<=127)return (NativeInteger *)cache->values->values[value+128];
    }
    NativeInteger *box=(NativeInteger *)MCObjectHeap_alloc(h,sizeof(*box),&integerClass);if(box)box->value=value;return box;
}
bool ObjectIntIdentityMap_isInstance(const MCObject *o){return o&&o->klass==&mapClass&&MCObjectHeap_objectSize(o)>=sizeof(ObjectIntIdentityMap);}
bool ObjectIntIdentityIterator_isInstance(const MCObject *o){return o&&o->klass==&iteratorClass&&MCObjectHeap_objectSize(o)>=sizeof(ObjectIntIdentityIterator);}
static bool begin(ObjectIntIdentityMap *m,MCObjectRootScope *scope){
    if(!ObjectIntIdentityMap_isInstance((MCObject *)m)){return fail(m?m->object.heap:NULL);}
    return MCObjectRootScope_begin(scope,m->object.heap)&&MCObjectRootScope_pin(scope,(MCObject *)m);
}
static bool map_valid(ObjectIntIdentityMap *m,NativeIdentityHashMap *map){return (NativeIdentityHashMap_isInstance((MCObject *)map)&&((MCObject *)map)->heap==m->object.heap)||fail(m->object.heap);}
static bool list_valid(ObjectIntIdentityMap *m,NativeReferenceList *list){return (NativeReferenceList_isInstance((MCObject *)list)&&list->object.heap==m->object.heap)||fail(m->object.heap);}
static void map_trace(MCObject *o,MCObjectVisitor v,void *c){
    if(!ObjectIntIdentityMap_isInstance(o)){fail(o->heap);return;}
    ObjectIntIdentityMap *m=(ObjectIntIdentityMap *)o;m->identityMap=(NativeIdentityHashMap *)v((MCObject *)m->identityMap,c);m->objectList=(NativeReferenceList *)v((MCObject *)m->objectList,c);
}
static void iterator_trace(MCObject *o,MCObjectVisitor v,void *c){
    if(!ObjectIntIdentityIterator_isInstance(o)){fail(o->heap);return;}
    ObjectIntIdentityIterator *i=(ObjectIntIdentityIterator *)o;i->input=(NativeIterator *)v((MCObject *)i->input,c);i->next=v(i->next,c);
}
static const MCObjectClass mapClass={"ObjectIntIdentityMap",MCObjectHeap_plainClone,map_trace,NULL};
static const MCObjectClass iteratorClass={"native.ObjectIntIdentityMap.FilteredIterator",MCObjectHeap_plainClone,iterator_trace,NULL};
ObjectIntIdentityMap *ObjectIntIdentityMap_nativeAllocate(MCObjectHeap *h){return (ObjectIntIdentityMap *)MCObjectHeap_alloc(h,sizeof(ObjectIntIdentityMap),&mapClass);}
bool ObjectIntIdentityMap_construct(ObjectIntIdentityMap *m){
    MCObjectRootScope scope={0};if(!begin(m,&scope))return false;
    NativeIdentityHashMap *map=NativeIdentityHashMap_new(m->object.heap,512);bool ok=map!=NULL;
    if(ok){m->identityMap=map;MCObjectHeap_touch(m->object.heap);NativeReferenceList *list=NativeReferenceList_new(m->object.heap);ok=list!=NULL;if(ok){m->objectList=list;MCObjectHeap_touch(m->object.heap);}}
    MCObjectRootScope_end(&scope);return ok;
}
ObjectIntIdentityMap *ObjectIntIdentityMap_new(MCObjectHeap *h){ObjectIntIdentityMap *m=ObjectIntIdentityMap_nativeAllocate(h);return m&&ObjectIntIdentityMap_construct(m)?m:NULL;}
bool ObjectIntIdentityMap_put(ObjectIntIdentityMap *m,MCObject *key,int32_t value){
    MCObjectRootScope scope={0};if(!begin(m,&scope))return false;
    /* Capture receiver before Integer.valueOf evaluates, as the Java call does. */
    NativeIdentityHashMap *map=m->identityMap;NativeInteger *box=integer_value_of(m->object.heap,value);
    bool ok=box&&map_valid(m,map)&&NativeIdentityHashMap_put(map,key,(MCObject *)box);
    while(ok){NativeReferenceList *list=m->objectList;if(!list_valid(m,list)){ok=false;break;}
        int32_t size=NativeReferenceList_size(list);if(MCObjectHeap_failed(m->object.heap)){ok=false;break;}
        if(size>value)break;
        ok=NativeReferenceList_add(list,NULL);
    }
    if(ok){NativeReferenceList *list=m->objectList;ok=list_valid(m,list);if(ok){(void)NativeReferenceList_set(list,value,key);ok=!MCObjectHeap_failed(m->object.heap);}}
    MCObjectRootScope_end(&scope);return ok;
}
int32_t ObjectIntIdentityMap_get(ObjectIntIdentityMap *m,MCObject *key){
    MCObjectRootScope scope={0};if(!begin(m,&scope))return -1;int32_t result=-1;
    NativeIdentityHashMap *map=m->identityMap;
    if(map_valid(m,map)){MCObject *box=NativeIdentityHashMap_get(map,key);
        if(box){if(box->heap!=m->object.heap||box->klass!=&integerClass||MCObjectHeap_objectSize(box)<sizeof(NativeInteger))fail(m->object.heap);else result=((NativeInteger *)box)->value;}}
    MCObjectRootScope_end(&scope);return result;
}
MCObject *ObjectIntIdentityMap_getByValue(ObjectIntIdentityMap *m,int32_t value){
    MCObjectRootScope scope={0};if(!begin(m,&scope))return NULL;MCObject *result=NULL;
    if(value>=0){NativeReferenceList *list=m->objectList;
        if(list_valid(m,list)){int32_t size=NativeReferenceList_size(list);if(!MCObjectHeap_failed(m->object.heap)&&value<size)result=NativeReferenceList_get(m->objectList,value);}}
    MCObjectRootScope_end(&scope);return result;
}
ObjectIntIdentityIterator *ObjectIntIdentityMap_iterator(ObjectIntIdentityMap *m){
    MCObjectRootScope scope={0};if(!begin(m,&scope))return NULL;
    NativeReferenceList *list=m->objectList;NativeIterator *input=list_valid(m,list)?NativeIterator_fromList(list):NULL;
    ObjectIntIdentityIterator *iterator=input?(ObjectIntIdentityIterator *)MCObjectHeap_alloc(m->object.heap,sizeof(*iterator),&iteratorClass):NULL;
    if(iterator)iterator->input=input;
    MCObjectRootScope_end(&scope);return iterator;
}
static bool iterator_valid(ObjectIntIdentityIterator *i){return (ObjectIntIdentityIterator_isInstance((MCObject *)i)&&!MCObjectHeap_failed(i->object.heap)&&NativeIterator_isInstance((MCObject *)i->input)&&((MCObject *)i->input)->heap==i->object.heap)||fail(i?i->object.heap:NULL);}
bool ObjectIntIdentityIterator_hasNext(ObjectIntIdentityIterator *i){
    if(!iterator_valid(i))return false;
    if(i->state==1)return true;
    if(i->state==2)return false;
    if(i->state==3)return fail(i->object.heap);
    i->state=3;MCObjectHeap_touch(i->object.heap);
    while(NativeIterator_hasNext(i->input)){MCObject *next=NULL;if(!NativeIterator_next(i->input,&next))return false;
        if(next){i->next=next;i->state=1;MCObjectHeap_touch(i->object.heap);return true;}}
    if(MCObjectHeap_failed(i->object.heap))return false;
    i->state=2;MCObjectHeap_touch(i->object.heap);return false;
}
bool ObjectIntIdentityIterator_next(ObjectIntIdentityIterator *i,MCObject **out){
    if(!out||!ObjectIntIdentityIterator_hasNext(i))return fail(i?i->object.heap:NULL);
    MCObject *next=i->next;i->next=NULL;i->state=0;MCObjectHeap_touch(i->object.heap);*out=next;return true;
}
