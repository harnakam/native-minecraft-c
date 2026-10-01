#include "nbt/NBTInternal.h"
#include <limits.h>
static void entryTrace(MCObject *o,MCObjectVisitor v,void *x) {
    NBTCompoundEntry *e=(NBTCompoundEntry*)o;
    e->key=(NBTString*)v((MCObject*)e->key,x);e->value=(NBTBase*)v((MCObject*)e->value,x);
    e->next=(NBTCompoundEntry*)v((MCObject*)e->next,x);
    e->previous=(NBTCompoundEntry*)v((MCObject*)e->previous,x);
    e->left=(NBTCompoundEntry*)v((MCObject*)e->left,x);
    e->right=(NBTCompoundEntry*)v((MCObject*)e->right,x);
    e->parent=(NBTCompoundEntry*)v((MCObject*)e->parent,x);
}
static const MCObjectClass entryClass={"NBTTagCompound.HashMapEntry",MCObjectHeap_plainClone,entryTrace,NULL};
static void bucketTrace(MCObject *o,MCObjectVisitor v,void *x) {
    NBTBucketStorage *s=(NBTBucketStorage*)o;
    for(size_t i=0;i<(size_t)s->capacity*2;i++)s->data[i]=(NBTCompoundEntry*)v((MCObject*)s->data[i],x);
}
static const MCObjectClass bucketClass={"NBTTagCompound.HashMapBuckets",MCObjectHeap_plainClone,bucketTrace,NULL};
static void keySetTrace(MCObject *o,MCObjectVisitor v,void *x) {
    NBTCompoundKeySet *s=(NBTCompoundKeySet*)o;s->owner=(NBTTagCompound*)v((MCObject*)s->owner,x);
}
static const MCObjectClass keySetClass={"NBTTagCompound.HashMapKeySet",MCObjectHeap_plainClone,keySetTrace,NULL};
static void trace(MCObject *o,MCObjectVisitor v,void *x) {
    NBTTagCompound *t=(NBTTagCompound*)o;
    t->storage=(NBTBucketStorage*)v((MCObject*)t->storage,x);
    t->keySet=(NBTCompoundKeySet*)v((MCObject*)t->keySet,x);
    t->index=(NBTCompoundEntry*)v((MCObject*)t->index,x);
}
static const MCObjectClass tagClass={"NBTTagCompound",MCObjectHeap_plainClone,trace,NULL};
bool NBTTagCompound_isInstance(const MCObject *object) {
    return object&&object->klass==&tagClass&&MCObjectHeap_objectSize(object)>=sizeof(NBTTagCompound);
}
static uint32_t hash(const NBTString *s) { uint32_t h=(uint32_t)NBTString_hashCode(s);return h^(h>>16); }
static uint32_t asciiHash(const char *s) {
    uint32_t h=0;if(s)while(*s)h=h*31u+(unsigned char)*s++;
    return h^(h>>16);
}
NBTTagCompound *NBTTagCompound_new(MCObjectHeap *h) {
    NBTTagCompound *t=(NBTTagCompound*)MCObjectHeap_alloc(h,sizeof(*t),&tagClass);
    if(t)t->base.type=10;
    return t;
}
NBTCompoundEntry *nbt_compoundFind(const NBTTagCompound *t,const NBTString *key) {
    if(!t)return NULL;
    uint32_t h=hash(key);NBTCompoundEntry *e=t->index;
    while(e) {
        int compare;
        if(h!=e->hash)compare=h<e->hash?-1:1;
        else if(key==e->key)compare=0;
        else if(!key || !e->key)compare=key?1:-1;
        else {
            size_t n=key->length<e->key->length?key->length:e->key->length;
            compare=0;
            for(size_t i=0;i<n;i++)if(key->units[i]!=e->key->units[i]) { compare=key->units[i]<e->key->units[i]?-1:1;break; }
            if(!compare && key->length!=e->key->length)compare=key->length<e->key->length?-1:1;
        }
        if(!compare)return e;
        e=compare<0?e->left:e->right;
    }
    return NULL;
}
NBTCompoundEntry *nbt_compoundFindASCII(const NBTTagCompound *t,const char *key) {
    if(!t)return NULL;
    size_t length=0;
    if(key) { while(key[length]) { if((unsigned char)key[length]>127)return NULL;length++; } }
    uint32_t h=asciiHash(key);NBTCompoundEntry *e=t->index;
    while(e) {
        int compare;
        if(h!=e->hash)compare=h<e->hash?-1:1;
        else if(!key || !e->key)compare=key?1:e->key?-1:0;
        else {
            size_t n=length<e->key->length?length:e->key->length;
            compare=0;
            for(size_t i=0;i<n;i++)if((unsigned char)key[i]!=e->key->units[i]) { compare=(unsigned char)key[i]<e->key->units[i]?-1:1;break; }
            if(!compare && length!=e->key->length)compare=length<e->key->length?-1:1;
        }
        if(!compare)return e;
        e=compare<0?e->left:e->right;
    }
    return NULL;
}
/* The native dictionary keeps Java String key/value semantics and bucket
   traversal, with a balanced lookup index to bound adversarial hash collisions.
   It does not claim the JDK HashMap TreeNode iteration implementation. */
static int keyOrder(const NBTCompoundEntry *a,const NBTCompoundEntry *b) {
    if(a->hash!=b->hash)return a->hash<b->hash?-1:1;
    if(a->key==b->key)return 0;
    if(!a->key || !b->key)return a->key?1:-1;
    size_t n=a->key->length<b->key->length?a->key->length:b->key->length;
    for(size_t i=0;i<n;i++)if(a->key->units[i]!=b->key->units[i])return a->key->units[i]<b->key->units[i]?-1:1;
    return a->key->length==b->key->length?0:a->key->length<b->key->length?-1:1;
}
static int32_t height(const NBTCompoundEntry *e) { return e?e->height:0; }
static void updateHeight(NBTCompoundEntry *e) {
    int32_t left=height(e->left),right=height(e->right);e->height=1+(left>right?left:right);
}
static void replaceIndex(NBTTagCompound *t,NBTCompoundEntry *old,NBTCompoundEntry *value) {
    if(!old->parent)t->index=value;
    else if(old->parent->left==old)old->parent->left=value;
    else old->parent->right=value;
    if(value)value->parent=old->parent;
}
static NBTCompoundEntry *rotateLeft(NBTTagCompound *t,NBTCompoundEntry *e) {
    NBTCompoundEntry *right=e->right;replaceIndex(t,e,right);e->right=right->left;
    if(e->right)e->right->parent=e;
    right->left=e;e->parent=right;updateHeight(e);updateHeight(right);return right;
}
static NBTCompoundEntry *rotateRight(NBTTagCompound *t,NBTCompoundEntry *e) {
    NBTCompoundEntry *left=e->left;replaceIndex(t,e,left);e->left=left->right;
    if(e->left)e->left->parent=e;
    left->right=e;e->parent=left;updateHeight(e);updateHeight(left);return left;
}
static void balance(NBTTagCompound *t,NBTCompoundEntry *e) {
    while(e) {
        updateHeight(e);
        if(height(e->left)-height(e->right)>1) {
            if(height(e->left->left)<height(e->left->right))rotateLeft(t,e->left);
            e=rotateRight(t,e);
        } else if(height(e->right)-height(e->left)>1) {
            if(height(e->right->right)<height(e->right->left))rotateRight(t,e->right);
            e=rotateLeft(t,e);
        }
        e=e->parent;
    }
}
static void insertIndex(NBTTagCompound *t,NBTCompoundEntry *entry) {
    NBTCompoundEntry *parent=NULL,**p=&t->index;
    while(*p) { parent=*p;p=keyOrder(entry,parent)<0?&parent->left:&parent->right; }
    *p=entry;entry->parent=parent;entry->height=1;balance(t,parent);
}
static void removeIndex(NBTTagCompound *t,NBTCompoundEntry *entry) {
    NBTCompoundEntry *from;
    if(!entry->left || !entry->right) {
        from=entry->parent;replaceIndex(t,entry,entry->left?entry->left:entry->right);
    } else {
        NBTCompoundEntry *next=entry->right;while(next->left)next=next->left;
        if(next->parent==entry)from=next;
        else {
            from=next->parent;replaceIndex(t,next,next->right);
            next->right=entry->right;next->right->parent=next;
        }
        replaceIndex(t,entry,next);next->left=entry->left;next->left->parent=next;
        next->height=entry->height;
    }
    entry->parent=entry->left=entry->right=NULL;entry->height=1;balance(t,from);
}
static bool resize(NBTTagCompound *t) {
    int32_t old=t->storage?t->storage->capacity:0;
    if(old>=1073741824)return true;
    int32_t cap=old?old*2:16;
    if((size_t)cap>(SIZE_MAX-sizeof(NBTBucketStorage))/(2*sizeof(NBTCompoundEntry*))) { MCObjectHeap_fail(t->base.object.heap);return false; }
    NBTBucketStorage *s=(NBTBucketStorage*)MCObjectHeap_alloc(t->base.object.heap,sizeof(*s)+(size_t)cap*2*sizeof(NBTCompoundEntry*),&bucketClass);
    if(!s)return false;
    s->capacity=cap;
    if(old)for(int32_t i=0;i<old;i++) {
        NBTCompoundEntry *e=t->storage->data[i],*lo=NULL,*hi=NULL,*lot=NULL,*hit=NULL;
        while(e) {
            NBTCompoundEntry *next=e->next;
            if(e->hash&(uint32_t)old) { e->previous=hit;if(hit)hit->next=e;else hi=e;hit=e; }
            else { e->previous=lot;if(lot)lot->next=e;else lo=e;lot=e; }
            e=next;
        }
        if(lot)lot->next=NULL;
        if(hit)hit->next=NULL;
        s->data[i]=lo;s->data[i+old]=hi;
        s->data[(size_t)cap+i]=lot;s->data[(size_t)cap+i+old]=hit;
    }
    t->storage=s;MCObjectHeap_touch(t->base.object.heap);return true;
}
bool NBTTagCompound_setTag(NBTTagCompound *t,NBTString *key,NBTBase *value) {
    if(!t || !nbt_sameHeap(t->base.object.heap,(MCObject*)key) || !nbt_sameHeap(t->base.object.heap,(MCObject*)value))return false;
    NBTCompoundEntry *found=nbt_compoundFind(t,key);
    if(found) { found->value=value;MCObjectHeap_touch(t->base.object.heap);return true; }
    if(!t->storage && !resize(t))return false;
    if(t->count==INT32_MAX) { MCObjectHeap_fail(t->base.object.heap);return false; }
    uint32_t h=hash(key),slot=h&((uint32_t)t->storage->capacity-1u);
    NBTCompoundEntry *e=(NBTCompoundEntry*)MCObjectHeap_alloc(t->base.object.heap,sizeof(*e),&entryClass);
    if(!e)return false;
    e->key=key;e->value=value;e->hash=h;
    NBTCompoundEntry *tail=t->storage->data[(size_t)t->storage->capacity+slot];
    e->previous=tail;
    if(tail)tail->next=e;else t->storage->data[slot]=e;
    t->storage->data[(size_t)t->storage->capacity+slot]=e;
    insertIndex(t,e);t->count++;t->modification++;MCObjectHeap_touch(t->base.object.heap);
    return t->count<=t->storage->capacity-t->storage->capacity/4 || resize(t);
}
NBTBase *NBTTagCompound_getTag(const NBTTagCompound *t,const NBTString *key) { NBTCompoundEntry *e=nbt_compoundFind(t,key);return e?e->value:NULL; }
NBTBase *NBTTagCompound_getTag_ascii(const NBTTagCompound *t,const char *key) { NBTCompoundEntry *e=nbt_compoundFindASCII(t,key);return e?e->value:NULL; }
int8_t NBTTagCompound_getTagId(const NBTTagCompound *t,const NBTString *key) { return NBTBase_getId(NBTTagCompound_getTag(t,key)); }
int8_t NBTTagCompound_getTagId_ascii(const NBTTagCompound *t,const char *key) { return NBTBase_getId(NBTTagCompound_getTag_ascii(t,key)); }
bool NBTTagCompound_hasKey(const NBTTagCompound *t,const NBTString *key) { return nbt_compoundFind(t,key)!=NULL; }
bool NBTTagCompound_hasKey_ascii(const NBTTagCompound *t,const char *key) { return nbt_compoundFindASCII(t,key)!=NULL; }
static bool idMatches(int8_t id,int32_t type) { return id==type || (type==99 && id>=1 && id<=6); }
bool NBTTagCompound_hasKeyType(const NBTTagCompound *t,const NBTString *key,int32_t type) { return idMatches(NBTTagCompound_getTagId(t,key),type); }
bool NBTTagCompound_hasKeyType_ascii(const NBTTagCompound *t,const char *key,int32_t type) { return idMatches(NBTTagCompound_getTagId_ascii(t,key),type); }
static bool removeEntry(NBTTagCompound *t,NBTCompoundEntry *target) {
    if(!target)return true;
    uint32_t slot=target->hash&((uint32_t)t->storage->capacity-1u);
    if(target->previous)target->previous->next=target->next;else t->storage->data[slot]=target->next;
    if(target->next)target->next->previous=target->previous;
    else t->storage->data[(size_t)t->storage->capacity+slot]=target->previous;
    target->next=target->previous=NULL;removeIndex(t,target);
    t->count--;t->modification++;MCObjectHeap_touch(t->base.object.heap);return true;
}
bool NBTTagCompound_removeTag(NBTTagCompound *t,const NBTString *key) { return t && removeEntry(t,nbt_compoundFind(t,key)); }
bool NBTTagCompound_removeTag_ascii(NBTTagCompound *t,const char *key) { return t && removeEntry(t,nbt_compoundFindASCII(t,key)); }
bool nbt_compoundClear(NBTTagCompound *t) {
    if(!t)return false;
    if(t->storage)for(size_t i=0;i<(size_t)t->storage->capacity*2;i++)t->storage->data[i]=NULL;
    t->index=NULL;t->count=0;t->modification++;MCObjectHeap_touch(t->base.object.heap);return true;
}
static NBTTagCompound *getCompound(NBTTagCompound *t,NBTBase *v) { return v && v->type==10?(NBTTagCompound*)v:NBTTagCompound_new(t->base.object.heap); }
NBTTagCompound *NBTTagCompound_getCompoundTag(NBTTagCompound *t,const NBTString *key) { return t?getCompound(t,NBTTagCompound_getTag(t,key)):NULL; }
NBTTagCompound *NBTTagCompound_getCompoundTag_ascii(NBTTagCompound *t,const char *key) { return t?getCompound(t,NBTTagCompound_getTag_ascii(t,key)):NULL; }
static NBTTagList *getList(NBTTagCompound *t,NBTBase *v,int32_t type) {
    if(v && v->type==9 && (!((NBTTagList*)v)->count || ((NBTTagList*)v)->tagType==type))return (NBTTagList*)v;
    return NBTTagList_new(t->base.object.heap);
}
NBTTagList *NBTTagCompound_getTagList(NBTTagCompound *t,const NBTString *key,int32_t type) { return t?getList(t,NBTTagCompound_getTag(t,key),type):NULL; }
NBTTagList *NBTTagCompound_getTagList_ascii(NBTTagCompound *t,const char *key,int32_t type) { return t?getList(t,NBTTagCompound_getTag_ascii(t,key),type):NULL; }
static NBTString *getString(NBTTagCompound *t,NBTBase *v) { return v && v->type==8?((NBTTagString*)v)->data:NBTString_literalASCII(t->base.object.heap,""); }
NBTString *NBTTagCompound_getString(NBTTagCompound *t,const NBTString *key) { return t?getString(t,NBTTagCompound_getTag(t,key)):NULL; }
NBTString *NBTTagCompound_getString_ascii(NBTTagCompound *t,const char *key) { return t?getString(t,NBTTagCompound_getTag_ascii(t,key)):NULL; }
NBTByteArrayStorage *NBTTagCompound_getByteArray(NBTTagCompound *t,const NBTString *key) {
    if(!t)return NULL;
    NBTBase *v=NBTTagCompound_getTag(t,key);
    return v && v->type==7?((NBTTagByteArray*)v)->data:NBTByteArrayStorage_new(t->base.object.heap,NULL,0);
}
NBTIntArrayStorage *NBTTagCompound_getIntArray(NBTTagCompound *t,const NBTString *key) {
    if(!t)return NULL;
    NBTBase *v=NBTTagCompound_getTag(t,key);
    return v && v->type==11?((NBTTagIntArray*)v)->data:NBTIntArrayStorage_new(t->base.object.heap,NULL,0);
}
bool NBTTagCompound_setString(NBTTagCompound *t,NBTString *key,NBTString *value) {
    if(!t)return false;
    NBTBase *v=(NBTBase*)NBTTagString_new(t->base.object.heap,value);
    return v && NBTTagCompound_setTag(t,key,v);
}
bool NBTTagCompound_setByteArray(NBTTagCompound *t,NBTString *key,NBTByteArrayStorage *value) {
    if(!t)return false;
    NBTBase *v=(NBTBase*)NBTTagByteArray_new(t->base.object.heap,value);
    return v && NBTTagCompound_setTag(t,key,v);
}
bool NBTTagCompound_setIntArray(NBTTagCompound *t,NBTString *key,NBTIntArrayStorage *value) {
    if(!t)return false;
    NBTBase *v=(NBTBase*)NBTTagIntArray_new(t->base.object.heap,value);
    return v && NBTTagCompound_setTag(t,key,v);
}
static bool primitive(const NBTBase *v) { return v && v->type>=1 && v->type<=6; }
int8_t NBTTagCompound_getByte(const NBTTagCompound *t,const NBTString *key) { NBTBase *v=NBTTagCompound_getTag(t,key);return primitive(v)?NBTPrimitive_getByte((NBTPrimitive*)v):0; }
int8_t NBTTagCompound_getByte_ascii(const NBTTagCompound *t,const char *key) { NBTBase *v=NBTTagCompound_getTag_ascii(t,key);return primitive(v)?NBTPrimitive_getByte((NBTPrimitive*)v):0; }
bool NBTTagCompound_setByte(NBTTagCompound *t,NBTString *key,int8_t value) { if(!t)return false;NBTBase *v=(NBTBase*)NBTTagByte_new(t->base.object.heap,value);return v && NBTTagCompound_setTag(t,key,v); }
bool NBTTagCompound_setByte_ascii(NBTTagCompound *t,const char *key,int8_t value) { if(!t)return false;NBTBase *v=(NBTBase*)NBTTagByte_new(t->base.object.heap,value);return v && NBTTagCompound_setTag_ascii(t,key,v); }
int16_t NBTTagCompound_getShort(const NBTTagCompound *t,const NBTString *key) { NBTBase *v=NBTTagCompound_getTag(t,key);return primitive(v)?NBTPrimitive_getShort((NBTPrimitive*)v):0; }
int16_t NBTTagCompound_getShort_ascii(const NBTTagCompound *t,const char *key) { NBTBase *v=NBTTagCompound_getTag_ascii(t,key);return primitive(v)?NBTPrimitive_getShort((NBTPrimitive*)v):0; }
bool NBTTagCompound_setShort(NBTTagCompound *t,NBTString *key,int16_t value) { if(!t)return false;NBTBase *v=(NBTBase*)NBTTagShort_new(t->base.object.heap,value);return v && NBTTagCompound_setTag(t,key,v); }
bool NBTTagCompound_setShort_ascii(NBTTagCompound *t,const char *key,int16_t value) { if(!t)return false;NBTBase *v=(NBTBase*)NBTTagShort_new(t->base.object.heap,value);return v && NBTTagCompound_setTag_ascii(t,key,v); }
int32_t NBTTagCompound_getInteger(const NBTTagCompound *t,const NBTString *key) { NBTBase *v=NBTTagCompound_getTag(t,key);return primitive(v)?NBTPrimitive_getInt((NBTPrimitive*)v):0; }
int32_t NBTTagCompound_getInteger_ascii(const NBTTagCompound *t,const char *key) { NBTBase *v=NBTTagCompound_getTag_ascii(t,key);return primitive(v)?NBTPrimitive_getInt((NBTPrimitive*)v):0; }
bool NBTTagCompound_setInteger(NBTTagCompound *t,NBTString *key,int32_t value) { if(!t)return false;NBTBase *v=(NBTBase*)NBTTagInt_new(t->base.object.heap,value);return v && NBTTagCompound_setTag(t,key,v); }
bool NBTTagCompound_setInteger_ascii(NBTTagCompound *t,const char *key,int32_t value) { if(!t)return false;NBTBase *v=(NBTBase*)NBTTagInt_new(t->base.object.heap,value);return v && NBTTagCompound_setTag_ascii(t,key,v); }
int64_t NBTTagCompound_getLong(const NBTTagCompound *t,const NBTString *key) { NBTBase *v=NBTTagCompound_getTag(t,key);return primitive(v)?NBTPrimitive_getLong((NBTPrimitive*)v):0; }
int64_t NBTTagCompound_getLong_ascii(const NBTTagCompound *t,const char *key) { NBTBase *v=NBTTagCompound_getTag_ascii(t,key);return primitive(v)?NBTPrimitive_getLong((NBTPrimitive*)v):0; }
bool NBTTagCompound_setLong(NBTTagCompound *t,NBTString *key,int64_t value) { if(!t)return false;NBTBase *v=(NBTBase*)NBTTagLong_new(t->base.object.heap,value);return v && NBTTagCompound_setTag(t,key,v); }
bool NBTTagCompound_setLong_ascii(NBTTagCompound *t,const char *key,int64_t value) { if(!t)return false;NBTBase *v=(NBTBase*)NBTTagLong_new(t->base.object.heap,value);return v && NBTTagCompound_setTag_ascii(t,key,v); }
float NBTTagCompound_getFloat(const NBTTagCompound *t,const NBTString *key) { NBTBase *v=NBTTagCompound_getTag(t,key);return primitive(v)?NBTPrimitive_getFloat((NBTPrimitive*)v):0; }
float NBTTagCompound_getFloat_ascii(const NBTTagCompound *t,const char *key) { NBTBase *v=NBTTagCompound_getTag_ascii(t,key);return primitive(v)?NBTPrimitive_getFloat((NBTPrimitive*)v):0; }
bool NBTTagCompound_setFloat(NBTTagCompound *t,NBTString *key,float value) { if(!t)return false;NBTBase *v=(NBTBase*)NBTTagFloat_new(t->base.object.heap,value);return v && NBTTagCompound_setTag(t,key,v); }
bool NBTTagCompound_setFloat_ascii(NBTTagCompound *t,const char *key,float value) { if(!t)return false;NBTBase *v=(NBTBase*)NBTTagFloat_new(t->base.object.heap,value);return v && NBTTagCompound_setTag_ascii(t,key,v); }
double NBTTagCompound_getDouble(const NBTTagCompound *t,const NBTString *key) { NBTBase *v=NBTTagCompound_getTag(t,key);return primitive(v)?NBTPrimitive_getDouble((NBTPrimitive*)v):0; }
double NBTTagCompound_getDouble_ascii(const NBTTagCompound *t,const char *key) { NBTBase *v=NBTTagCompound_getTag_ascii(t,key);return primitive(v)?NBTPrimitive_getDouble((NBTPrimitive*)v):0; }
bool NBTTagCompound_setDouble(NBTTagCompound *t,NBTString *key,double value) { if(!t)return false;NBTBase *v=(NBTBase*)NBTTagDouble_new(t->base.object.heap,value);return v && NBTTagCompound_setTag(t,key,v); }
bool NBTTagCompound_setDouble_ascii(NBTTagCompound *t,const char *key,double value) { if(!t)return false;NBTBase *v=(NBTBase*)NBTTagDouble_new(t->base.object.heap,value);return v && NBTTagCompound_setTag_ascii(t,key,v); }
bool NBTTagCompound_getBoolean(const NBTTagCompound *t,const NBTString *key) { return NBTTagCompound_getByte(t,key)!=0; }
bool NBTTagCompound_getBoolean_ascii(const NBTTagCompound *t,const char *key) { return NBTTagCompound_getByte_ascii(t,key)!=0; }
bool NBTTagCompound_setBoolean(NBTTagCompound *t,NBTString *key,bool v) { return NBTTagCompound_setByte(t,key,v?1:0); }
bool NBTTagCompound_setBoolean_ascii(NBTTagCompound *t,const char *key,bool v) { return NBTTagCompound_setByte_ascii(t,key,v?1:0); }
bool NBTTagCompound_setTag_ascii(NBTTagCompound *t,const char *key,NBTBase *v) {
    if(!t)return false;
    NBTCompoundEntry *e=nbt_compoundFindASCII(t,key);
    if(e) { if(!nbt_sameHeap(t->base.object.heap,(MCObject*)v))return false;e->value=v;MCObjectHeap_touch(t->base.object.heap);return true; }
    NBTString *k=key?NBTString_literalASCII(t->base.object.heap,key):NULL;
    return (!key || k) && NBTTagCompound_setTag(t,k,v);
}
bool NBTTagCompound_setString_ascii(NBTTagCompound *t,const char *key,NBTString *v) {
    if(!t)return false;
    NBTBase *s=(NBTBase*)NBTTagString_new(t->base.object.heap,v);
    return s && NBTTagCompound_setTag_ascii(t,key,s);
}
NBTCompoundKeySet *NBTTagCompound_getKeySet(NBTTagCompound *t) {
    if(!t)return NULL;
    if(!t->keySet) {
        NBTCompoundKeySet *s=(NBTCompoundKeySet*)MCObjectHeap_alloc(t->base.object.heap,sizeof(*s),&keySetClass);
        if(!s)return NULL;
        s->owner=t;t->keySet=s;MCObjectHeap_touch(t->base.object.heap);
    }
    return t->keySet;
}
int32_t NBTCompoundKeySet_size(const NBTCompoundKeySet *s) { return s && s->owner?s->owner->count:0; }
bool NBTCompoundKeySet_contains(const NBTCompoundKeySet *s,const NBTString *key) { return s && NBTTagCompound_hasKey(s->owner,key); }
bool NBTCompoundKeySet_remove(NBTCompoundKeySet *s,const NBTString *key) {
    if(!s || !s->owner || !NBTTagCompound_hasKey(s->owner,key))return false;
    return NBTTagCompound_removeTag(s->owner,key);
}
bool NBTCompoundKeySet_clear(NBTCompoundKeySet *s) { return s && nbt_compoundClear(s->owner); }
NBTString *NBTCompoundKeySet_keyAt(const NBTCompoundKeySet *s,int32_t index) {
    if(!s || !s->owner || index<0 || index>=s->owner->count || !s->owner->storage)return NULL;
    for(int32_t i=0;i<s->owner->storage->capacity;i++)for(NBTCompoundEntry *e=s->owner->storage->data[i];e;e=e->next)if(index--==0)return e->key;
    return NULL;
}
static bool merge(NBTTagCompound *t,const NBTTagCompound *other,int32_t depth) {
    if(depth>512) { MCObjectHeap_fail(t->base.object.heap);return false; }
    if(!other->storage)return true;
    for(int32_t i=0;i<other->storage->capacity;i++)for(NBTCompoundEntry *e=other->storage->data[i];e;e=e->next) {
        if(!e->value) { MCObjectHeap_fail(t->base.object.heap);return false; }
        if(e->value->type==10 && NBTTagCompound_hasKeyType(t,e->key,10)) {
            if(!merge(NBTTagCompound_getCompoundTag(t,e->key),(NBTTagCompound*)e->value,depth+1))return false;
        } else {
            NBTBase *value=NBTBase_copy(t->base.object.heap,e->value);
            if(!value || !NBTTagCompound_setTag(t,e->key,value))return false;
        }
    }
    return true;
}
bool NBTTagCompound_merge(NBTTagCompound *t,const NBTTagCompound *other) {
    return t && other && nbt_sameHeap(t->base.object.heap,(MCObject*)other) && merge(t,other,0);
}
