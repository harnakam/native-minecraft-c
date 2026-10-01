#include "util/NativeIdentityHashSet.h"
static const MCObjectClass setClass;
static const MCObjectClass presentClass={"native.IdentityHashSet.TRUE",MCObjectHeap_plainClone,NULL,NULL};
static bool fail(MCObjectHeap *h){MCObjectHeap_fail(h);return false;}
static bool any(const MCObject *o,void *c){(void)o;(void)c;return true;}
bool NativeIdentityHashSet_isInstance(const MCObject *o){return o&&o->klass==&setClass&&MCObjectHeap_objectSize(o)>=sizeof(NativeIdentityHashSet);}
static void trace(MCObject *o,MCObjectVisitor v,void *c){
    if(!NativeIdentityHashSet_isInstance(o)){fail(o->heap);return;}
    NativeIdentityHashSet *s=(NativeIdentityHashSet *)o;s->map=(NativeIdentityHashMap *)v((MCObject *)s->map,c);s->present=v(s->present,c);
}
static const MCObjectClass setClass={"native.IdentityHashSet",MCObjectHeap_plainClone,trace,NULL};
static bool valid(NativeIdentityHashSet *s){return (NativeIdentityHashSet_isInstance((MCObject *)s)&&!MCObjectHeap_failed(s->object.heap)&&
    NativeIdentityHashMap_isInstance((MCObject *)s->map)&&((MCObject *)s->map)->heap==s->object.heap&&s->present&&s->present->heap==s->object.heap&&s->present->klass==&presentClass)||fail(s?s->object.heap:NULL);}
NativeIdentityHashSet *NativeIdentityHashSet_new(MCObjectHeap *h){
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    NativeIdentityHashSet *s=(NativeIdentityHashSet *)MCObjectHeap_alloc(h,sizeof(*s),&setClass);
    MCObject *present=MCObjectHeap_findObject(h,&presentClass,any,NULL);bool ok=s!=NULL;
    if(ok&&!present){present=MCObjectHeap_alloc(h,sizeof(MCObject),&presentClass);MCObjectRoot root={0};ok=present&&MCObjectRoot_init(&root,h,present);}
    if(ok){s->present=present;s->map=NativeIdentityHashMap_new(h,21);ok=s->map&&NativeIdentityHashMap_keySet(s->map);}
    MCObjectRootScope_end(&scope);return ok?s:NULL;
}
int32_t NativeIdentityHashSet_size(NativeIdentityHashSet *s){return valid(s)?NativeIdentityHashMap_size(s->map):0;}
bool NativeIdentityHashSet_contains(NativeIdentityHashSet *s,MCObject *key){return valid(s)&&NativeIdentityHashMap_containsKey(s->map,key);}
bool NativeIdentityHashSet_add(NativeIdentityHashSet *s,MCObject *key,bool *changed){
    if(!valid(s)||!changed)return fail(s?s->object.heap:NULL);
    MCObject *old=NULL;if(!NativeIdentityHashMap_putWithPrevious(s->map,key,s->present,&old))return false;*changed=old==NULL;return true;
}
bool NativeIdentityHashSet_remove(NativeIdentityHashSet *s,MCObject *key,bool *changed){
    if(!valid(s)||!changed)return fail(s?s->object.heap:NULL);
    MCObject *old=NativeIdentityHashMap_remove(s->map,key);if(MCObjectHeap_failed(s->object.heap))return false;*changed=old==s->present;return true;
}
bool NativeIdentityHashSet_clear(NativeIdentityHashSet *s){return valid(s)&&NativeIdentityHashMap_clear(s->map);}
NativeIdentityIterator *NativeIdentityHashSet_iterator(NativeIdentityHashSet *s){return valid(s)?NativeIdentityKeySet_iterator(NativeIdentityHashMap_keySet(s->map)):NULL;}
