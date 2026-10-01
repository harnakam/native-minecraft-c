#include "util/NativeHashSet.h"
static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    NativeHashSet *set=(NativeHashSet *)object;
    set->map=(NativeHashMap *)visitor((MCObject *)set->map,context);set->present=visitor(set->present,context);
}
static const MCObjectClass klass={"native.HashSet",MCObjectHeap_plainClone,trace,NULL};
static const MCObjectClass presentClass={"native.HashSet.PRESENT",MCObjectHeap_plainClone,NULL,NULL};
static bool any(const MCObject *object,void *context){(void)object;(void)context;return true;}
bool NativeHashSet_isInstance(const MCObject *object){return object&&object->klass==&klass&&MCObjectHeap_objectSize(object)>=sizeof(NativeHashSet);}
static bool valid(NativeHashSet *set) {
    if(NativeHashSet_isInstance((MCObject *)set)&&!MCObjectHeap_failed(set->object.heap)&&
       NativeHashMap_isInstance((MCObject *)set->map)&&((MCObject *)set->map)->heap==set->object.heap&&
       set->present&&set->present->klass==&presentClass&&set->present->heap==set->object.heap)return true;
    MCObjectHeap_fail(set?set->object.heap:NULL);return false;
}
NativeHashSet *NativeHashSet_newWithKeys(MCObjectHeap *heap,const NativeHashKeyMethods *methods,MCObject *context) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    NativeHashSet *set=(NativeHashSet *)MCObjectHeap_alloc(heap,sizeof(*set),&klass);bool ok=set!=NULL;
    MCObject *present=MCObjectHeap_findObject(heap,&presentClass,any,NULL);
    if(ok&&!present){present=MCObjectHeap_alloc(heap,sizeof(MCObject),&presentClass);MCObjectRoot root={0};ok=present&&MCObjectRoot_init(&root,heap,present);}
    if(ok){set->present=present;set->map=NativeHashMap_newWithKeys(heap,methods,context);ok=set->map!=NULL;}
    MCObjectRootScope_end(&scope);return ok?set:NULL;
}
int32_t NativeHashSet_size(NativeHashSet *set){return valid(set)?NativeHashMap_size(set->map):0;}
bool NativeHashSet_contains(NativeHashSet *set,MCObject *key){return valid(set)&&NativeHashMap_containsKey(set->map,key);}
bool NativeHashSet_add(NativeHashSet *set,MCObject *key,bool *changed) {
    if(!valid(set)||!changed){MCObjectHeap_fail(set?set->object.heap:NULL);return false;}
    MCObject *prior=NULL;
    if(!NativeHashMap_putWithPrevious(set->map,key,set->present,&prior))return false;
    *changed=prior==NULL;return true;
}
bool NativeHashSet_remove(NativeHashSet *set,MCObject *key,bool *changed) {
    if(!valid(set)||!changed){MCObjectHeap_fail(set?set->object.heap:NULL);return false;}
    MCObject *removed=NativeHashMap_remove(set->map,key);if(MCObjectHeap_failed(set->object.heap))return false;
    *changed=removed==set->present;return true;
}
bool NativeHashSet_clear(NativeHashSet *set){return valid(set)&&NativeHashMap_clear(set->map);}
NativeIterator *NativeHashSet_iterator(NativeHashSet *set){return valid(set)?NativeIterator_fromView(NativeHashMap_keys(set->map)):NULL;}
