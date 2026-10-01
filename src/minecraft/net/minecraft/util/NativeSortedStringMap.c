#include "util/NativeSortedStringMap.h"
#include "nbt/NBTInternal.h"
#include <limits.h>
static bool fail(MCObjectHeap *h){MCObjectHeap_fail(h);return false;}
static void trace_entry(MCObject *o,MCObjectVisitor v,void *c) {
    NativeSortedStringEntry *e=(NativeSortedStringEntry *)o;
    e->key=(NBTString *)v((MCObject *)e->key,c);e->value=v(e->value,c);
    e->left=(NativeSortedStringEntry *)v((MCObject *)e->left,c);
    e->right=(NativeSortedStringEntry *)v((MCObject *)e->right,c);
    e->parent=(NativeSortedStringEntry *)v((MCObject *)e->parent,c);
}
static void trace_map(MCObject *o,MCObjectVisitor v,void *c) {
    NativeSortedStringMap *m=(NativeSortedStringMap *)o;
    m->root=(NativeSortedStringEntry *)v((MCObject *)m->root,c);
}
static const MCObjectClass entryClass={"native.SortedStringMap.Entry",MCObjectHeap_plainClone,trace_entry,NULL};
static const MCObjectClass mapClass={"native.SortedStringMap",MCObjectHeap_plainClone,trace_map,NULL};
bool NativeSortedStringMap_isInstance(const MCObject *o){return o&&o->klass==&mapClass&&MCObjectHeap_objectSize(o)>=sizeof(NativeSortedStringMap);}
static bool valid(NativeSortedStringMap *m,const NBTString *key,bool requireKey) {
    MCObjectHeap *h=m?m->object.heap:NULL;
    if(!NativeSortedStringMap_isInstance((MCObject *)m)||MCObjectHeap_failed(h))return fail(h);
    if(requireKey) {
        size_t size=key?MCObjectHeap_objectSize((const MCObject *)key):0;
        if(!key||key->object.heap!=h||!NBTString_isInstance((const MCObject *)key)||size<sizeof *key||
           key->length>(size-sizeof *key)/sizeof *key->units)return fail(h);
    }
    return true;
}
static int cmp(const NBTString *a,const NBTString *b) {
    size_t n=a->length<b->length?a->length:b->length;
    for(size_t i=0;i<n;i++)if(a->units[i]!=b->units[i])return a->units[i]>b->units[i]?1:-1;
    return a->length==b->length?0:a->length>b->length?1:-1;
}
NativeSortedStringMap *NativeSortedStringMap_new(MCObjectHeap *h){return (NativeSortedStringMap *)MCObjectHeap_alloc(h,sizeof(NativeSortedStringMap),&mapClass);}
static NativeSortedStringEntry *find(NativeSortedStringMap *m,const NBTString *key) {
    NativeSortedStringEntry *p=m->root;
    while(p){int c=cmp(key,p->key);if(!c)return p;p=c<0?p->left:p->right;}
    return NULL;
}
MCObject *NativeSortedStringMap_get(NativeSortedStringMap *m,const NBTString *key) {
    if(!valid(m,key,true))return NULL;
    NativeSortedStringEntry *e=find(m,key);return e?e->value:NULL;
}
bool NativeSortedStringMap_containsKey(NativeSortedStringMap *m,const NBTString *key){return valid(m,key,true)&&find(m,key)!=NULL;}
static bool red(NativeSortedStringEntry *e){return e&&e->red;}
static void rotateLeft(NativeSortedStringMap *m,NativeSortedStringEntry *p) {
    NativeSortedStringEntry *r=p->right;p->right=r->left;
    if(r->left)r->left->parent=p;
    r->parent=p->parent;
    if(!p->parent)m->root=r;else if(p->parent->left==p)p->parent->left=r;else p->parent->right=r;
    r->left=p;p->parent=r;
}
static void rotateRight(NativeSortedStringMap *m,NativeSortedStringEntry *p) {
    NativeSortedStringEntry *l=p->left;p->left=l->right;
    if(l->right)l->right->parent=p;
    l->parent=p->parent;
    if(!p->parent)m->root=l;else if(p->parent->right==p)p->parent->right=l;else p->parent->left=l;
    l->right=p;p->parent=l;
}
static void balance(NativeSortedStringMap *m,NativeSortedStringEntry *x) {
    x->red=true;
    while(x!=m->root&&red(x->parent)) {
        NativeSortedStringEntry *p=x->parent,*g=p->parent;
        if(p==g->left) {
            NativeSortedStringEntry *uncle=g->right;
            if(red(uncle)){p->red=false;uncle->red=false;g->red=true;x=g;}
            else {if(x==p->right){x=p;rotateLeft(m,x);p=x->parent;g=p->parent;}p->red=false;g->red=true;rotateRight(m,g);}
        }else {
            NativeSortedStringEntry *uncle=g->left;
            if(red(uncle)){p->red=false;uncle->red=false;g->red=true;x=g;}
            else {if(x==p->left){x=p;rotateRight(m,x);p=x->parent;g=p->parent;}p->red=false;g->red=true;rotateLeft(m,g);}
        }
    }
    m->root->red=false;
}
bool NativeSortedStringMap_put(NativeSortedStringMap *m,NBTString *key,MCObject *value) {
    if(!valid(m,key,true)||(value&&value->heap!=m->object.heap))return fail(m?m->object.heap:NULL);
    MCObjectRootScope scope={0};MCObjectHeap *h=m->object.heap;
    if(!MCObjectRootScope_begin(&scope,h)||!MCObjectRootScope_pin(&scope,(MCObject *)m)||
       !MCObjectRootScope_pin(&scope,(MCObject *)key)||!MCObjectRootScope_pin(&scope,value)) {MCObjectRootScope_end(&scope);return fail(h);}
    NativeSortedStringEntry *parent=NULL,*p=m->root;int comparison=0;bool ok=false;
    while(p) {
        parent=p;comparison=cmp(key,p->key);
        if(!comparison){p->value=value;MCObjectHeap_touch(h);ok=true;goto done;}
        p=comparison<0?p->left:p->right;
    }
    if(m->size==INT32_MAX){fail(h);goto done;}
    p=(NativeSortedStringEntry *)MCObjectHeap_alloc(h,sizeof *p,&entryClass);
    if(!p)goto done;
    p->key=key;p->value=value;p->parent=parent;
    if(!parent)m->root=p;else if(comparison<0)parent->left=p;else parent->right=p;
    balance(m,p);m->size++;m->modCount++;MCObjectHeap_touch(h);ok=true;
done:
    MCObjectRootScope_end(&scope);return ok;
}
bool NativeSortedStringMap_entryAt(NativeSortedStringMap *m,int32_t index,NBTString **key,MCObject **value) {
    if(!valid(m,NULL,false)||!key||!value||index<0||index>=m->size)return fail(m?m->object.heap:NULL);
    NativeSortedStringEntry *p=m->root;
    while(p&&p->left)p=p->left;
    for(int32_t i=0;i<index;i++) {
        if(p->right){p=p->right;while(p->left)p=p->left;}
        else {NativeSortedStringEntry *child=p;p=p->parent;while(p&&child==p->right){child=p;p=p->parent;}}
    }
    if(!p)return fail(m->object.heap);
    *key=p->key;*value=p->value;return true;
}
bool NativeSortedStringMap_beginCursor(NativeSortedStringMap *m,NativeSortedStringCursor *c) {
    if(!valid(m,NULL,false)||!c||!MCObjectHeap_hasBorrowers(m->object.heap))return fail(m?m->object.heap:NULL);
    NativeSortedStringEntry *p=m->root;while(p&&p->left)p=p->left;
    *c=(NativeSortedStringCursor){m,p,m->modCount};return true;
}
bool NativeSortedStringMap_next(NativeSortedStringCursor *c,NBTString **key,MCObject **value) {
    NativeSortedStringMap *m=c?c->map:NULL;
    if(!valid(m,NULL,false)||!key||!value||!MCObjectHeap_hasBorrowers(m->object.heap)||c->expectedModCount!=m->modCount||!c->next)return fail(m?m->object.heap:NULL);
    NativeSortedStringEntry *p=c->next;*key=p->key;*value=p->value;
    if(p->right){p=p->right;while(p->left)p=p->left;}
    else {NativeSortedStringEntry *child=p;p=p->parent;while(p&&child==p->right){child=p;p=p->parent;}}
    c->next=p;return true;
}
