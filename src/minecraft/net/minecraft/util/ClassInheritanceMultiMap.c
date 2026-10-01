#include "util/ClassInheritanceMultiMap.h"
#include "entity/Entity.h"

static bool fail(MCObjectHeap *h){MCObjectHeap_fail(h);return false;}
static void trace(MCObject *o,MCObjectVisitor v,void *ctx){
    ClassInheritanceMultiMap *s=(ClassInheritanceMultiMap *)o;
    if(MCObjectHeap_objectSize(o)<sizeof(*s)){fail(o->heap);return;}
    s->map=(NativeHashMap *)v((MCObject *)s->map,ctx);
    s->knownKeys=(NativeIdentityHashSet *)v((MCObject *)s->knownKeys,ctx);
    s->baseClass=(NativeJavaClass *)v((MCObject *)s->baseClass,ctx);
    s->values=(NativeReferenceList *)v((MCObject *)s->values,ctx);
    s->dependencyContext=v(s->dependencyContext,ctx);
}
static const MCObjectClass klass={"net.minecraft.util.ClassInheritanceMultiMap",MCObjectHeap_plainClone,trace,NULL};
bool ClassInheritanceMultiMap_isInstance(const MCObject *o){return o&&o->klass==&klass&&MCObjectHeap_objectSize(o)>=sizeof(ClassInheritanceMultiMap);}
static bool begin(ClassInheritanceMultiMap *s,MCObjectRootScope *scope){
    MCObjectHeap *h=s?s->object.heap:NULL;
    if(!ClassInheritanceMultiMap_isInstance((MCObject *)s)||MCObjectHeap_failed(h)||
       !MCObjectRootScope_begin(scope,h))return fail(h);
    if(MCObjectRootScope_pin(scope,(MCObject *)s)&&MCObjectRootScope_pin(scope,s->dependencyContext))return true;
    MCObjectRootScope_end(scope);return fail(h);
}
static bool effect(ClassInheritanceMultiMap *s,bool ok){return ok&&!MCObjectHeap_failed(s->object.heap)?true:fail(s->object.heap);}
/* A preceding virtual/native call may replace the context. Capture and guard
   the newly read reference immediately before each subsequent dispatch. */
static bool capture_context(ClassInheritanceMultiMap *s,MCObjectRootScope *scope,MCObject **out){
    MCObject *context=s->dependencyContext;
    if(!MCObjectRootScope_pin(scope,context))return false;
    *out=context;return true;
}
static bool class_ref(ClassInheritanceMultiMap *s,NativeJavaClass *c,bool nullable){
    if(!c)return nullable?true:fail(s->object.heap);
    return NativeJavaClass_isInstance((MCObject *)c)&&c->object.heap==s->object.heap?true:fail(s->object.heap);
}
static bool map_ref(ClassInheritanceMultiMap *s,NativeHashMap *m){return NativeHashMap_isInstance((MCObject *)m)&&((MCObject *)m)->heap==s->object.heap?true:fail(s->object.heap);}
static bool list_ref(ClassInheritanceMultiMap *s,NativeReferenceList *l){return NativeReferenceList_isInstance((MCObject *)l)&&l->object.heap==s->object.heap?true:fail(s->object.heap);}
static bool hash_ref(ClassInheritanceMultiMap *s,NativeHashSet *set){return NativeHashSet_isInstance((MCObject *)set)&&set->object.heap==s->object.heap?true:fail(s->object.heap);}
static bool identity_ref(ClassInheritanceMultiMap *s,NativeIdentityHashSet *set){return NativeIdentityHashSet_isInstance((MCObject *)set)&&set->object.heap==s->object.heap?true:fail(s->object.heap);}
static bool key_hash(MCObject *ctx,MCObject *key,int32_t *out){(void)ctx;*out=MCObjectHeap_identityHashCode(key);return true;}
static bool key_equals(MCObject *ctx,MCObject *a,MCObject *b,bool *out){(void)ctx;*out=a==b;return true;}
static const NativeHashKeyMethods classKeys={key_hash,key_equals};
typedef struct {MCObject object;NativeHashSet *field_181158_a;} Statics;
static void statics_trace(MCObject *o,MCObjectVisitor v,void *ctx){
    if(MCObjectHeap_objectSize(o)<sizeof(Statics)){fail(o->heap);return;}
    Statics *s=(Statics *)o;s->field_181158_a=(NativeHashSet *)v((MCObject *)s->field_181158_a,ctx);
}
static const MCObjectClass staticsClass={"native.ClassInheritanceMultiMapStatics",MCObjectHeap_plainClone,statics_trace,NULL};
static bool any(const MCObject *o,void *ctx){(void)o;(void)ctx;return true;}
NativeHashSet *ClassInheritanceMultiMap_nativeKnownClasses(MCObjectHeap *h){
    if(MCObjectHeap_failed(h))return NULL;
    Statics *s=(Statics *)MCObjectHeap_findObject(h,&staticsClass,any,NULL);
    if(!s){
        s=(Statics *)MCObjectHeap_alloc(h,sizeof(*s),&staticsClass);if(!s)return NULL;
        NativeHashSet *set=NativeHashSet_newWithKeys(h,&classKeys,NULL);if(!set)return NULL;
        s->field_181158_a=set;
        MCObjectRoot root={0};if(!MCObjectRoot_init(&root,h,(MCObject *)s)){fail(h);return NULL;}
    }
    if(MCObjectHeap_objectSize((MCObject *)s)<sizeof(*s)||!NativeHashSet_isInstance((MCObject *)s->field_181158_a)||
       s->field_181158_a->object.heap!=h){fail(h);return NULL;}
    return s->field_181158_a;
}
static NativeHashSet *all_known(ClassInheritanceMultiMap *s){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return NULL;
    MCObject *context=NULL;NativeHashSet *out=NULL;
    const ClassInheritanceMultiMapDependencies *d=s->dependencies;
    if(capture_context(s,&scope,&context))out=d&&d->getAllKnownClasses?d->getAllKnownClasses(context,s):
        ClassInheritanceMultiMap_nativeKnownClasses(s->object.heap);
    bool ok=hash_ref(s,out)&&MCObjectRootScope_pin(&scope,(MCObject *)out)&&!MCObjectHeap_failed(s->object.heap);
    MCObjectRootScope_end(&scope);return ok?out:NULL;
}
ClassInheritanceMultiMap *ClassInheritanceMultiMap_nativeAllocate(MCObjectHeap *h,
        const ClassInheritanceMultiMapDependencies *d,MCObject *ctx){
    if((ctx&&ctx->heap!=h)||!ClassInheritanceMultiMap_nativeKnownClasses(h)){fail(h);return NULL;}
    ClassInheritanceMultiMap *s=(ClassInheritanceMultiMap *)MCObjectHeap_alloc(h,sizeof(*s),&klass);
    if(s){s->dependencies=d;s->dependencyContext=ctx;}
    return s;
}
static NativeJavaClass *object_class(ClassInheritanceMultiMap *s,MCObject *o){
    if(!o||o->heap!=s->object.heap){fail(s->object.heap);return NULL;}
    MCObjectRootScope scope={0};if(!begin(s,&scope))return NULL;
    MCObject *context=NULL;NativeJavaClass *c=NULL;
    const ClassInheritanceMultiMapDependencies *d=s->dependencies;
    if(MCObjectRootScope_pin(&scope,o)&&capture_context(s,&scope,&context))
        c=d&&d->objectGetClass?d->objectGetClass(context,o):NativeJavaClass_getClass(s->object.heap,o);
    bool ok=class_ref(s,c,false)&&MCObjectRootScope_pin(&scope,(MCObject *)c)&&!MCObjectHeap_failed(s->object.heap);
    MCObjectRootScope_end(&scope);return ok?c:NULL;
}
static bool assignable(ClassInheritanceMultiMap *s,NativeJavaClass *target,NativeJavaClass *candidate,bool *out){
    if(!class_ref(s,target,false)||!class_ref(s,candidate,true))return false;
    MCObjectRootScope scope={0};if(!begin(s,&scope))return false;
    MCObject *context=NULL;bool ok=false;
    const ClassInheritanceMultiMapDependencies *d=s->dependencies;
    if(MCObjectRootScope_pin(&scope,(MCObject *)target)&&MCObjectRootScope_pin(&scope,(MCObject *)candidate)&&
       capture_context(s,&scope,&context))ok=d&&d->classIsAssignableFrom?d->classIsAssignableFrom(context,target,candidate,out):
        NativeJavaClass_isAssignableFrom(target,candidate,out);
    MCObjectRootScope_end(&scope);return effect(s,ok);
}
static bool equals(ClassInheritanceMultiMap *s,MCObject *query,MCObject *stored,bool *out){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return false;
    MCObject *context=NULL;bool ok=false;
    const ClassInheritanceMultiMapDependencies *d=s->dependencies;
    if(MCObjectRootScope_pin(&scope,query)&&MCObjectRootScope_pin(&scope,stored)&&capture_context(s,&scope,&context)){
        if(d&&d->objectEquals)ok=d->objectEquals(context,query,stored,out);
        else if(Entity_isInstance(query)){*out=Entity_equals((Entity *)query,stored);ok=!MCObjectHeap_failed(s->object.heap);}
    }
    MCObjectRootScope_end(&scope);return effect(s,ok);
}
bool ClassInheritanceMultiMap_construct(ClassInheritanceMultiMap *s,NativeJavaClass *base){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)base);
    const ClassInheritanceMultiMapDependencies *d=s->dependencies;MCObject *context=NULL;
    if(!ok||!capture_context(s,&scope,&context))goto done;
    NativeHashMap *map=d&&d->newMap?d->newMap(context,s):NativeHashMap_new(s->object.heap,NATIVE_HASH_KEY_IDENTITY);
    if(!map_ref(s,map)||!MCObjectRootScope_pin(&scope,(MCObject *)map)||MCObjectHeap_failed(s->object.heap))goto done;
    s->map=map;MCObjectHeap_touch(s->object.heap);
    if(!capture_context(s,&scope,&context))goto done;
    NativeIdentityHashSet *keys=d&&d->newKnownKeys?d->newKnownKeys(context,s):NativeIdentityHashSet_new(s->object.heap);
    if(!identity_ref(s,keys)||!MCObjectRootScope_pin(&scope,(MCObject *)keys)||MCObjectHeap_failed(s->object.heap))goto done;
    s->knownKeys=keys;MCObjectHeap_touch(s->object.heap);
    if(!capture_context(s,&scope,&context))goto done;
    NativeReferenceList *values=d&&d->newValues?d->newValues(context,s):NativeReferenceList_new(s->object.heap);
    if(!list_ref(s,values)||!MCObjectRootScope_pin(&scope,(MCObject *)values)||MCObjectHeap_failed(s->object.heap))goto done;
    s->values=values;s->baseClass=base;MCObjectHeap_touch(s->object.heap);
    bool changed;
    if(!identity_ref(s,s->knownKeys)||!NativeIdentityHashSet_add(s->knownKeys,(MCObject *)base,&changed))goto done;
    if(!map_ref(s,s->map)||!NativeHashMap_put(s->map,(MCObject *)base,(MCObject *)s->values))goto done;
    NativeHashSet *all=all_known(s);NativeIterator *it=all?NativeHashSet_iterator(all):NULL;
    if(!it||!MCObjectRootScope_pin(&scope,(MCObject *)it))goto done;
    while(NativeIterator_hasNext(it)){
        MCObject *c=NULL;if(!NativeIterator_next(it,&c)||!ClassInheritanceMultiMap_createLookup(s,(NativeJavaClass *)c))goto done;
    }
    ok=!MCObjectHeap_failed(s->object.heap);goto finish;
done:ok=false;
finish:MCObjectRootScope_end(&scope);return effect(s,ok);
}
ClassInheritanceMultiMap *ClassInheritanceMultiMap_new(MCObjectHeap *h,NativeJavaClass *base){
    ClassInheritanceMultiMap *s=ClassInheritanceMultiMap_nativeAllocate(h,NULL,NULL);
    return s&&ClassInheritanceMultiMap_construct(s,base)?s:NULL;
}
static bool add_for_class(ClassInheritanceMultiMap *s,MCObject *value,NativeJavaClass *parent){
    NativeHashMap *map=s->map;if(!map_ref(s,map))return false;
    NativeReferenceList *list=(NativeReferenceList *)NativeHashMap_get(map,(MCObject *)parent);
    if(MCObjectHeap_failed(s->object.heap))return false;
    if(!list){
        /* Capture put's map receiver before constructing its list argument. */
        map=s->map;if(!map_ref(s,map))return false;
        MCObjectRootScope scope={0};if(!begin(s,&scope))return false;
        const ClassInheritanceMultiMapDependencies *d=s->dependencies;MCObject *context=NULL;bool ok=false;
        if(!MCObjectRootScope_pin(&scope,(MCObject *)map)||!MCObjectRootScope_pin(&scope,value)||
           !MCObjectRootScope_pin(&scope,(MCObject *)parent)||!capture_context(s,&scope,&context))goto done;
        list=d&&d->newSingleList?d->newSingleList(context,s,value):NativeReferenceList_new(s->object.heap);
        if(!list_ref(s,list)||!MCObjectRootScope_pin(&scope,(MCObject *)list)||MCObjectHeap_failed(s->object.heap))goto done;
        if(!(d&&d->newSingleList)&&!NativeReferenceList_add(list,value))goto done;
        ok=NativeHashMap_put(map,(MCObject *)parent,(MCObject *)list);
done:MCObjectRootScope_end(&scope);return effect(s,ok);
    }
    return list_ref(s,list)&&NativeReferenceList_add(list,value);
}
bool ClassInheritanceMultiMap_createLookup_base(ClassInheritanceMultiMap *s,NativeJavaClass *c){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)c)&&class_ref(s,c,true),changed;
    NativeHashSet *all=ok?all_known(s):NULL;if(!all||!NativeHashSet_add(all,(MCObject *)c,&changed))goto done;
    NativeReferenceList *values=s->values;if(!list_ref(s,values))goto done;
    NativeIterator *it=NativeIterator_fromList(values);if(!it||!MCObjectRootScope_pin(&scope,(MCObject *)it))goto done;
    while(NativeIterator_hasNext(it)){
        MCObject *value=NULL;if(!NativeIterator_next(it,&value))goto done;
        NativeJavaClass *actual=object_class(s,value);bool match=false;
        if(!actual||!assignable(s,c,actual,&match))goto done;
        if(match&&!add_for_class(s,value,c))goto done;
    }
    if(MCObjectHeap_failed(s->object.heap)||!identity_ref(s,s->knownKeys)||
       !NativeIdentityHashSet_add(s->knownKeys,(MCObject *)c,&changed))goto done;
    ok=true;goto finish;
done:ok=false;
finish:MCObjectRootScope_end(&scope);return effect(s,ok);
}
bool ClassInheritanceMultiMap_createLookup(ClassInheritanceMultiMap *s,NativeJavaClass *c){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return false;
    MCObject *context=NULL;bool ok=MCObjectRootScope_pin(&scope,(MCObject *)c)&&class_ref(s,c,true)&&capture_context(s,&scope,&context);
    const ClassInheritanceMultiMapDependencies *d=s->dependencies;
    if(ok)ok=d&&d->createLookup?d->createLookup(context,s,c):ClassInheritanceMultiMap_createLookup_base(s,c);
    MCObjectRootScope_end(&scope);return effect(s,ok);
}
NativeJavaClass *ClassInheritanceMultiMap_initializeClassLookup_base(ClassInheritanceMultiMap *s,NativeJavaClass *c){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return NULL;
    NativeJavaClass *out=NULL;bool match=false;
    if(!MCObjectRootScope_pin(&scope,(MCObject *)c)||!assignable(s,s->baseClass,c,&match)||!match){fail(s->object.heap);goto done;}
    if(!identity_ref(s,s->knownKeys))goto done;
    if(!NativeIdentityHashSet_contains(s->knownKeys,(MCObject *)c)&&
       (MCObjectHeap_failed(s->object.heap)||!ClassInheritanceMultiMap_createLookup(s,c)))goto done;
    out=c;
done:MCObjectRootScope_end(&scope);return MCObjectHeap_failed(s->object.heap)?NULL:out;
}
NativeJavaClass *ClassInheritanceMultiMap_initializeClassLookup(ClassInheritanceMultiMap *s,NativeJavaClass *c){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return NULL;
    NativeJavaClass *out=NULL;MCObject *context=NULL;const ClassInheritanceMultiMapDependencies *d=s->dependencies;
    if(MCObjectRootScope_pin(&scope,(MCObject *)c)&&class_ref(s,c,true)&&capture_context(s,&scope,&context))out=d&&d->initializeClassLookup?
        d->initializeClassLookup(context,s,c):ClassInheritanceMultiMap_initializeClassLookup_base(s,c);
    if(!class_ref(s,out,true)||!MCObjectRootScope_pin(&scope,(MCObject *)out))out=NULL;
    MCObjectRootScope_end(&scope);return MCObjectHeap_failed(s->object.heap)?NULL:out;
}
bool ClassInheritanceMultiMap_add(ClassInheritanceMultiMap *s,MCObject *value){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return false;
    bool ok=MCObjectRootScope_pin(&scope,value)&&identity_ref(s,s->knownKeys);
    NativeIdentityIterator *it=ok?NativeIdentityHashSet_iterator(s->knownKeys):NULL;
    if(!it||!MCObjectRootScope_pin(&scope,(MCObject *)it)){ok=false;goto done;}
    while(NativeIdentityIterator_hasNext(it)){
        MCObject *key=NULL;if(!NativeIdentityIterator_next(it,&key)){ok=false;goto done;}
        NativeJavaClass *actual=object_class(s,value);bool match=false;
        if(!actual||!assignable(s,(NativeJavaClass *)key,actual,&match)||
           (match&&!add_for_class(s,value,(NativeJavaClass *)key))){ok=false;goto done;}
    }
done:MCObjectRootScope_end(&scope);return effect(s,ok);
}
static bool remove_from_list(ClassInheritanceMultiMap *s,NativeReferenceList *list,MCObject *query,bool *changed){
    if(!list_ref(s,list))return false;
    *changed=false;
    for(int32_t i=0;i<NativeReferenceList_size(list);++i){
        MCObject *stored=NativeReferenceList_get(list,i);bool equal=false;
        if(MCObjectHeap_failed(s->object.heap)||!equals(s,query,stored,&equal))return false;
        if(equal){NativeReferenceList_remove(list,i);*changed=!MCObjectHeap_failed(s->object.heap);return *changed;}
    }
    return !MCObjectHeap_failed(s->object.heap);
}
bool ClassInheritanceMultiMap_remove(ClassInheritanceMultiMap *s,MCObject *query){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return false;
    bool changed=false,ok=MCObjectRootScope_pin(&scope,query)&&identity_ref(s,s->knownKeys);
    NativeIdentityIterator *it=ok?NativeIdentityHashSet_iterator(s->knownKeys):NULL;
    if(!it||!MCObjectRootScope_pin(&scope,(MCObject *)it)){ok=false;goto done;}
    while(NativeIdentityIterator_hasNext(it)){
        MCObject *key=NULL;if(!NativeIdentityIterator_next(it,&key)){ok=false;goto done;}
        NativeJavaClass *actual=object_class(s,query);bool match=false;
        if(!actual||!assignable(s,(NativeJavaClass *)key,actual,&match)){ok=false;goto done;}
        if(match){
            if(!map_ref(s,s->map)){ok=false;goto done;}
            NativeReferenceList *list=(NativeReferenceList *)NativeHashMap_get(s->map,key);
            if(MCObjectHeap_failed(s->object.heap)){ok=false;goto done;}
            if(list){bool removed=false;if(!remove_from_list(s,list,query,&removed)){ok=false;goto done;}if(removed)changed=true;}
        }
    }
done:MCObjectRootScope_end(&scope);return effect(s,ok)&&changed;
}
static void iterable_trace(MCObject *o,MCObjectVisitor v,void *ctx){
    ClassInheritanceMultiMapIterable *s=(ClassInheritanceMultiMapIterable *)o;
    if(MCObjectHeap_objectSize(o)<sizeof(*s)){fail(o->heap);return;}
    s->clazz=(NativeJavaClass *)v((MCObject *)s->clazz,ctx);s->owner=(ClassInheritanceMultiMap *)v((MCObject *)s->owner,ctx);
}
static const MCObjectClass iterableClass={"net.minecraft.util.ClassInheritanceMultiMap$1",MCObjectHeap_plainClone,iterable_trace,NULL};
bool ClassInheritanceMultiMapIterable_isInstance(const MCObject *o){return o&&o->klass==&iterableClass&&MCObjectHeap_objectSize(o)>=sizeof(ClassInheritanceMultiMapIterable);}
ClassInheritanceMultiMapIterable *ClassInheritanceMultiMap_getByClass_base(ClassInheritanceMultiMap *s,NativeJavaClass *c){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return NULL;
    ClassInheritanceMultiMapIterable *out=NULL;
    if(MCObjectRootScope_pin(&scope,(MCObject *)c)&&class_ref(s,c,true)){
        out=(ClassInheritanceMultiMapIterable *)MCObjectHeap_alloc(s->object.heap,sizeof(*out),&iterableClass);
        if(out){out->clazz=c;out->owner=s;}
    }
    MCObjectRootScope_end(&scope);return out;
}
ClassInheritanceMultiMapIterable *ClassInheritanceMultiMap_getByClass(ClassInheritanceMultiMap *s,NativeJavaClass *c){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return NULL;
    ClassInheritanceMultiMapIterable *out=NULL;MCObject *context=NULL;const ClassInheritanceMultiMapDependencies *d=s->dependencies;
    if(MCObjectRootScope_pin(&scope,(MCObject *)c)&&class_ref(s,c,true)&&capture_context(s,&scope,&context))out=d&&d->getByClass?d->getByClass(context,s,c):
        ClassInheritanceMultiMap_getByClass_base(s,c);
    if(out&&(!ClassInheritanceMultiMapIterable_isInstance((MCObject *)out)||out->object.heap!=s->object.heap)){fail(s->object.heap);out=NULL;}
    if(out&&!MCObjectRootScope_pin(&scope,(MCObject *)out))out=NULL;
    MCObjectRootScope_end(&scope);return MCObjectHeap_failed(s->object.heap)?NULL:out;
}
struct ClassInheritanceMultiMapIterator{
    MCObject object;
    NativeIterator *delegate;
    NativeJavaClass *filterClass;
    bool filtered,buffered,exhausted;
    MCObject *nextValue,*dependencyContext;
    const ClassInheritanceMultiMapDependencies *dependencies;
};
static void iterator_trace(MCObject *o,MCObjectVisitor v,void *ctx){
    ClassInheritanceMultiMapIterator *it=(ClassInheritanceMultiMapIterator *)o;
    if(MCObjectHeap_objectSize(o)<sizeof(*it)){fail(o->heap);return;}
    it->delegate=(NativeIterator *)v((MCObject *)it->delegate,ctx);
    it->filterClass=(NativeJavaClass *)v((MCObject *)it->filterClass,ctx);
    it->nextValue=v(it->nextValue,ctx);it->dependencyContext=v(it->dependencyContext,ctx);
}
static const MCObjectClass iteratorClass={"native.ClassInheritanceMultiMap.Iterator",MCObjectHeap_plainClone,iterator_trace,NULL};
bool ClassInheritanceMultiMapIterator_isInstance(const MCObject *o){return o&&o->klass==&iteratorClass&&MCObjectHeap_objectSize(o)>=sizeof(ClassInheritanceMultiMapIterator);}
static ClassInheritanceMultiMapIterator *new_iterator(ClassInheritanceMultiMap *s,NativeReferenceList *list,bool filtered,NativeJavaClass *c){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return NULL;
    MCObject *context=NULL;ClassInheritanceMultiMapIterator *out=NULL;
    if(!capture_context(s,&scope,&context)||!MCObjectRootScope_pin(&scope,(MCObject *)list)||
       !MCObjectRootScope_pin(&scope,(MCObject *)c)||!class_ref(s,c,!filtered))goto done;
    NativeIterator *delegate=NULL;
    if(list){if(!list_ref(s,list))goto done;delegate=NativeIterator_fromList(list);if(!delegate)goto done;}
    out=(ClassInheritanceMultiMapIterator *)MCObjectHeap_alloc(s->object.heap,sizeof(*out),&iteratorClass);
    if(out){out->delegate=delegate;out->filtered=filtered;out->filterClass=c;out->exhausted=!delegate;
        out->dependencyContext=context;out->dependencies=s->dependencies;}
done:MCObjectRootScope_end(&scope);return MCObjectHeap_failed(s->object.heap)?NULL:out;
}
ClassInheritanceMultiMapIterator *ClassInheritanceMultiMapIterable_iterator(ClassInheritanceMultiMapIterable *iterable){
    MCObjectHeap *h=iterable?iterable->object.heap:NULL;MCObjectRootScope scope={0};
    if(!ClassInheritanceMultiMapIterable_isInstance((MCObject *)iterable)||MCObjectHeap_failed(h)||
       !MCObjectRootScope_begin(&scope,h)){fail(h);return NULL;}
    ClassInheritanceMultiMapIterator *out=NULL;
    if(!MCObjectRootScope_pin(&scope,(MCObject *)iterable))goto done;
    ClassInheritanceMultiMap *s=iterable->owner;
    if(!ClassInheritanceMultiMap_isInstance((MCObject *)s)||s->object.heap!=h){fail(h);goto done;}
    /* Map.get receiver precedes the initializeClassLookup argument call. */
    NativeHashMap *map=s->map;
    /* Evaluate the argument even when the captured get receiver is NULL.
       Record a foreign/malformed native receiver's validity without invoking
       it or dereferencing it again after an argument callback. */
    bool capturedMapValid=NativeHashMap_isInstance((MCObject *)map)&&((MCObject *)map)->heap==h;
    if(capturedMapValid&&!MCObjectRootScope_pin(&scope,(MCObject *)map))goto done;
    NativeJavaClass *key=ClassInheritanceMultiMap_initializeClassLookup(s,iterable->clazz);
    if(MCObjectHeap_failed(h))goto done;
    if(!capturedMapValid){fail(h);goto done;}
    NativeReferenceList *list=(NativeReferenceList *)NativeHashMap_get(map,(MCObject *)key);
    if(MCObjectHeap_failed(h))goto done;
    out=new_iterator(s,list,list!=NULL,iterable->clazz);
done:MCObjectRootScope_end(&scope);return MCObjectHeap_failed(h)?NULL:out;
}
ClassInheritanceMultiMapIterator *ClassInheritanceMultiMap_iterator(ClassInheritanceMultiMap *s){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return NULL;
    ClassInheritanceMultiMapIterator *out=NULL;
    if(list_ref(s,s->values)){
        bool empty=NativeReferenceList_size(s->values)==0;
        if(!MCObjectHeap_failed(s->object.heap))out=new_iterator(s,empty?NULL:s->values,false,NULL);
    }
    MCObjectRootScope_end(&scope);return out;
}
static bool iterator_begin(ClassInheritanceMultiMapIterator *it,MCObjectRootScope *scope){
    MCObjectHeap *h=it?it->object.heap:NULL;
    if(!ClassInheritanceMultiMapIterator_isInstance((MCObject *)it)||MCObjectHeap_failed(h)||
       !MCObjectRootScope_begin(scope,h))return fail(h);
    if(MCObjectRootScope_pin(scope,(MCObject *)it)&&MCObjectRootScope_pin(scope,it->dependencyContext)&&
       MCObjectRootScope_pin(scope,(MCObject *)it->filterClass)&&MCObjectRootScope_pin(scope,it->nextValue))return true;
    MCObjectRootScope_end(scope);return fail(h);
}
bool ClassInheritanceMultiMapIterator_hasNext(ClassInheritanceMultiMapIterator *it){
    MCObjectRootScope scope={0};if(!iterator_begin(it,&scope))return false;
    bool out=false;MCObjectHeap *h=it->object.heap;
    if(it->buffered){out=true;goto done;}if(it->exhausted)goto done;
    if(!NativeIterator_isInstance((MCObject *)it->delegate)||((MCObject *)it->delegate)->heap!=h){fail(h);goto done;}
    if(!it->filtered){out=NativeIterator_hasNext(it->delegate);goto done;}
    while(NativeIterator_hasNext(it->delegate)){
        MCObject *value=NULL;if(!NativeIterator_next(it->delegate,&value))goto done;
        bool match=false;const ClassInheritanceMultiMapDependencies *d=it->dependencies;
        MCObject *context=it->dependencyContext;NativeJavaClass *filter=it->filterClass;
        if(!MCObjectRootScope_pin(&scope,context)||!MCObjectRootScope_pin(&scope,(MCObject *)filter)||
           !NativeJavaClass_isInstance((MCObject *)filter)||!MCObjectRootScope_pin(&scope,value)){fail(h);goto done;}
        bool ok=d&&d->classIsInstance?d->classIsInstance(context,filter,value,&match):
            NativeJavaClass_isInstanceOf(filter,value,&match);
        if(!ok||MCObjectHeap_failed(h)){fail(h);goto done;}
        if(match){it->nextValue=value;it->buffered=true;out=true;MCObjectHeap_touch(h);goto done;}
    }
    if(!MCObjectHeap_failed(h)){it->exhausted=true;MCObjectHeap_touch(h);}
done:MCObjectRootScope_end(&scope);return out&&!MCObjectHeap_failed(h);
}
bool ClassInheritanceMultiMapIterator_next(ClassInheritanceMultiMapIterator *it,MCObject **out){
    MCObjectRootScope scope={0};if(!iterator_begin(it,&scope))return false;
    bool ok=out&&ClassInheritanceMultiMapIterator_hasNext(it);MCObject *value=NULL;
    if(ok){
        if(it->filtered){value=it->nextValue;it->nextValue=NULL;it->buffered=false;MCObjectHeap_touch(it->object.heap);}
        else ok=NativeIterator_next(it->delegate,&value);
    }
    ok=ok&&!MCObjectHeap_failed(it->object.heap);
    if(ok)*out=value;else fail(it->object.heap);
    MCObjectRootScope_end(&scope);return ok;
}
bool ClassInheritanceMultiMap_contains(ClassInheritanceMultiMap *s,MCObject *query){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return false;
    bool out=false;
    if(!MCObjectRootScope_pin(&scope,query))goto done;
    NativeJavaClass *c=object_class(s,query);if(!c)goto done;
    ClassInheritanceMultiMapIterable *iterable=ClassInheritanceMultiMap_getByClass(s,c);
    if(!iterable){fail(s->object.heap);goto done;}
    ClassInheritanceMultiMapIterator *it=ClassInheritanceMultiMapIterable_iterator(iterable);
    if(!it||!MCObjectRootScope_pin(&scope,(MCObject *)it))goto done;
    while(ClassInheritanceMultiMapIterator_hasNext(it)){
        MCObject *value=NULL;bool equal=false;
        if(!ClassInheritanceMultiMapIterator_next(it,&value)||!equals(s,query,value,&equal))goto done;
        if(equal){out=true;break;}
    }
done:MCObjectRootScope_end(&scope);return out&&!MCObjectHeap_failed(s->object.heap);
}
int32_t ClassInheritanceMultiMap_size(ClassInheritanceMultiMap *s){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return 0;
    int32_t out=list_ref(s,s->values)?NativeReferenceList_size(s->values):0;
    MCObjectRootScope_end(&scope);return MCObjectHeap_failed(s->object.heap)?0:out;
}
