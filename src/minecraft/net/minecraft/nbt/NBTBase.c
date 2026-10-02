#include "nbt/NBTInternal.h"
#include <math.h>
#include <string.h>
const char *const NBTBase_NBT_TYPES[12]={"END","BYTE","SHORT","INT","LONG","FLOAT","DOUBLE","BYTE[]","STRING","LIST","COMPOUND","INT[]"};
NBTBase *NBTBase_createNewByType(MCObjectHeap *h,int8_t id) {
    switch(id) {
    case 0:return (NBTBase*)NBTTagEnd_new(h);case 1:return (NBTBase*)NBTTagByte_new(h,0);
    case 2:return (NBTBase*)NBTTagShort_new(h,0);case 3:return (NBTBase*)NBTTagInt_new(h,0);
    case 4:return (NBTBase*)NBTTagLong_new(h,0);case 5:return (NBTBase*)NBTTagFloat_new(h,0);
    case 6:return (NBTBase*)NBTTagDouble_new(h,0);case 7:return (NBTBase*)NBTTagByteArray_new(h,NULL);
    case 8:return (NBTBase*)NBTTagString_new(h,NBTString_literalASCII(h,""));
    case 9:return (NBTBase*)NBTTagList_new(h);case 10:return (NBTBase*)NBTTagCompound_new(h);
    case 11:return (NBTBase*)NBTTagIntArray_new(h,NULL);default:return NULL;
    }
}
int8_t NBTBase_getId(const NBTBase *t) { return t?t->type:0; }
bool NBTBase_hasNoTags(const NBTBase *t) {
    if(!t)return false;
    switch(t->type) {
    case 8:return ((const NBTTagString*)t)->data && !((const NBTTagString*)t)->data->length;
    case 9:return !((const NBTTagList*)t)->count;case 10:return !((const NBTTagCompound*)t)->count;
    default:return false;
    }
}
static NBTBase *copy(MCObjectHeap *h,const NBTBase *t,int32_t depth) {
    if(!t || depth>513 || !nbt_sameHeap(h,(const MCObject*)t)) { MCObjectHeap_fail(h);return NULL; }
    switch(t->type) {
    case 0:return (NBTBase*)NBTTagEnd_new(h);
    case 1:return (NBTBase*)NBTTagByte_new(h,((const NBTTagByte*)t)->data);
    case 2:return (NBTBase*)NBTTagShort_new(h,((const NBTTagShort*)t)->data);
    case 3:return (NBTBase*)NBTTagInt_new(h,((const NBTTagInt*)t)->data);
    case 4:return (NBTBase*)NBTTagLong_new(h,((const NBTTagLong*)t)->data);
    case 5:return (NBTBase*)NBTTagFloat_new(h,((const NBTTagFloat*)t)->data);
    case 6:return (NBTBase*)NBTTagDouble_new(h,((const NBTTagDouble*)t)->data);
    case 7: {
        NBTByteArrayStorage *s=((const NBTTagByteArray*)t)->data;
        if(!s) { MCObjectHeap_fail(h);return NULL; }
        NBTByteArrayStorage *a=NBTByteArrayStorage_new(h,s->values,s->length);
        return a?(NBTBase*)NBTTagByteArray_new(h,a):NULL;
    }
    case 8:return (NBTBase*)NBTTagString_new(h,((const NBTTagString*)t)->data);
    case 9: {
        const NBTTagList *from=(const NBTTagList*)t;NBTTagList *to=NBTTagList_new(h);
        if(!to)return NULL;
        to->tagType=from->tagType;
        if(!nbt_listReserve(to,from->count))return NULL;
        for(int32_t i=0;i<from->count;i++) {
            NBTBase *child=copy(h,from->storage->data[i],depth+1);
            if(!child)return NULL;
            to->storage->data[to->count++]=child;
        }
        return (NBTBase*)to;
    }
    case 10: {
        const NBTTagCompound *from=(const NBTTagCompound*)t;NBTTagCompound *to=NBTTagCompound_new(h);
        if(!to)return NULL;
        if(from->storage)for(int32_t i=0;i<from->storage->capacity;i++)for(NBTCompoundEntry *e=from->storage->data[i];e;e=e->next) {
            NBTBase *child=copy(h,e->value,depth+1);
            if(!child || !NBTTagCompound_setTag(to,e->key,child))return NULL;
        }
        return (NBTBase*)to;
    }
    case 11: {
        NBTIntArrayStorage *s=((const NBTTagIntArray*)t)->data;
        if(!s) { MCObjectHeap_fail(h);return NULL; }
        NBTIntArrayStorage *a=NBTIntArrayStorage_new(h,s->data,s->length);
        return a?(NBTBase*)NBTTagIntArray_new(h,a):NULL;
    }
    default:MCObjectHeap_fail(h);return NULL;
    }
}
NBTBase *NBTBase_copy(MCObjectHeap *h,const NBTBase *t) { return copy(h,t,0); }
static bool equal(const NBTBase *a,const NBTBase *b,int32_t depth) {
    if(!a || !b || a->type!=b->type)return false;
    if(depth>513) { MCObjectHeap_fail(a->object.heap);return false; }
    switch(a->type) {
    case 0:return true;
    case 1:return ((const NBTTagByte*)a)->data==((const NBTTagByte*)b)->data;
    case 2:return ((const NBTTagShort*)a)->data==((const NBTTagShort*)b)->data;
    case 3:return ((const NBTTagInt*)a)->data==((const NBTTagInt*)b)->data;
    case 4:return ((const NBTTagLong*)a)->data==((const NBTTagLong*)b)->data;
    case 5:return ((const NBTTagFloat*)a)->data==((const NBTTagFloat*)b)->data;
    case 6:return ((const NBTTagDouble*)a)->data==((const NBTTagDouble*)b)->data;
    case 7: {
        const NBTByteArrayStorage *x=((const NBTTagByteArray*)a)->data,*y=((const NBTTagByteArray*)b)->data;
        return x==y || (x && y && x->length==y->length && (!x->length || memcmp(x->values,y->values,(size_t)x->length)==0));
    }
    case 8:return NBTString_equals(((const NBTTagString*)a)->data,((const NBTTagString*)b)->data);
    case 9: {
        const NBTTagList *x=(const NBTTagList*)a,*y=(const NBTTagList*)b;
        if(x->tagType!=y->tagType || x->count!=y->count)return false;
        if(x==y)return true;
        for(int32_t i=0;i<x->count;i++)if(!equal(x->storage->data[i],y->storage->data[i],depth+1))return false;
        return true;
    }
    case 10: {
        const NBTTagCompound *x=(const NBTTagCompound*)a,*y=(const NBTTagCompound*)b;
        if(x==y)return true;
        if(x->count!=y->count)return false;
        if(x->storage)for(int32_t i=0;i<x->storage->capacity;i++)for(NBTCompoundEntry *e=x->storage->data[i];e;e=e->next) {
            NBTCompoundEntry *other=nbt_compoundFind(y,e->key);
            if(!other)return false;
            if(e->value!=other->value && (!e->value || !other->value || !equal(e->value,other->value,depth+1)))return false;
        }
        return true;
    }
    case 11: {
        const NBTIntArrayStorage *x=((const NBTTagIntArray*)a)->data,*y=((const NBTTagIntArray*)b)->data;
        return x==y || (x && y && x->length==y->length && (!x->length || memcmp(x->data,y->data,(size_t)x->length*4)==0));
    }
    default:return false;
    }
}
bool NBTBase_equals(const NBTBase *a,const NBTBase *b) { return equal(a,b,0); }
static uint32_t floatBits(float v) { uint32_t u;memcpy(&u,&v,4);return isnan(v)?UINT32_C(0x7fc00000):u; }
static uint64_t doubleBits(double v) { uint64_t u;memcpy(&u,&v,8);return isnan(v)?UINT64_C(0x7ff8000000000000):u; }
static uint32_t hash(const NBTBase *t,int32_t depth) {
    if(!t)return 0;
    if(depth>513) { MCObjectHeap_fail(t->object.heap);return 0; }
    uint32_t value=0;
    switch(t->type) {
    case 0:return 0;
    case 1:value=(uint32_t)(int32_t)((const NBTTagByte*)t)->data;break;
    case 2:value=(uint32_t)(int32_t)((const NBTTagShort*)t)->data;break;
    case 3:value=(uint32_t)((const NBTTagInt*)t)->data;break;
    case 4:{uint64_t v=(uint64_t)((const NBTTagLong*)t)->data;value=(uint32_t)(v^(v>>32));break;}
    case 5:value=floatBits(((const NBTTagFloat*)t)->data);break;
    case 6:{uint64_t v=doubleBits(((const NBTTagDouble*)t)->data);value=(uint32_t)(v^(v>>32));break;}
    case 7: {
        const NBTByteArrayStorage *s=((const NBTTagByteArray*)t)->data;
        if(s) { value=1;for(int32_t i=0;i<s->length;i++)value=value*31u+(uint32_t)(int32_t)s->values[i]; }
        break;
    }
    case 8:value=(uint32_t)NBTString_hashCode(((const NBTTagString*)t)->data);break;
    case 9: {
        const NBTTagList *l=(const NBTTagList*)t;value=1;
        for(int32_t i=0;i<l->count;i++)value=value*31u+hash(l->storage->data[i],depth+1);
        break;
    }
    case 10: {
        const NBTTagCompound *c=(const NBTTagCompound*)t;
        if(c->storage)for(int32_t i=0;i<c->storage->capacity;i++)for(NBTCompoundEntry *e=c->storage->data[i];e;e=e->next)value+=(uint32_t)NBTString_hashCode(e->key)^hash(e->value,depth+1);
        break;
    }
    case 11: {
        const NBTIntArrayStorage *s=((const NBTTagIntArray*)t)->data;
        if(s) { value=1;for(int32_t i=0;i<s->length;i++)value=value*31u+(uint32_t)s->data[i]; }
        break;
    }
    default:MCObjectHeap_fail(t->object.heap);return 0;
    }
    return (uint32_t)t->type^value;
}
int32_t NBTBase_hashCode(const NBTBase *t) { return nbt_i32(hash(t,0)); }
static bool charged(NBTSizeTracker *tracker,int64_t bits,mc_buf *input) {
    if(NBTSizeTracker_read(tracker,bits))return true;
    input->failed=true;return false;
}
static bool available(mc_buf *input,size_t bytes) {
    if(input->failed || input->pos>input->len || bytes>input->len-input->pos) { input->failed=true;return false; }
    return true;
}
static NBTBase *readFactory(MCObjectHeap *heap,int8_t id,NBTString **empty) {
    if(id!=8)return NBTBase_createNewByType(heap,id);
    if(!*empty)*empty=NBTString_literalASCII(heap,"");
    return *empty?(NBTBase*)NBTTagString_new(heap,*empty):NULL;
}
static bool read(NBTBase *tag,mc_buf *input,int32_t depth,NBTSizeTracker *tracker,NBTString **empty) {
    if(!tag || !input || input->failed || !tracker)return false;
    MCObjectHeap *heap=tag->object.heap;
    switch(tag->type) {
    case 0:return charged(tracker,64,input);
    case 1:if(charged(tracker,72,input)) { int8_t v=nbt_i8(mc_get_u8(input));if(!input->failed) { ((NBTTagByte*)tag)->data=v;MCObjectHeap_touch(heap);return true; } }break;
    case 2:if(charged(tracker,80,input)) { int16_t v=mc_get_i16(input);if(!input->failed) { ((NBTTagShort*)tag)->data=v;MCObjectHeap_touch(heap);return true; } }break;
    case 3:if(charged(tracker,96,input)) { int32_t v=mc_get_i32(input);if(!input->failed) { ((NBTTagInt*)tag)->data=v;MCObjectHeap_touch(heap);return true; } }break;
    case 4:if(charged(tracker,128,input)) { int64_t v=mc_get_i64(input);if(!input->failed) { ((NBTTagLong*)tag)->data=v;MCObjectHeap_touch(heap);return true; } }break;
    case 5:if(charged(tracker,96,input)) { float v=mc_get_f32(input);if(!input->failed) { ((NBTTagFloat*)tag)->data=v;MCObjectHeap_touch(heap);return true; } }break;
    case 6:if(charged(tracker,128,input)) { double v=mc_get_f64(input);if(!input->failed) { ((NBTTagDouble*)tag)->data=v;MCObjectHeap_touch(heap);return true; } }break;
    case 7: {
        if(!charged(tracker,192,input))break;
        int32_t n=mc_get_i32(input);
        if(input->failed || !charged(tracker,nbt_i32((uint32_t)n*8u),input) || n<0 || !available(input,(size_t)n))break;
        NBTByteArrayStorage *s=NBTByteArrayStorage_new(heap,(const int8_t*)(input->data+input->pos),n);
        if(!s)break;
        input->pos+=(size_t)n;((NBTTagByteArray*)tag)->data=s;MCObjectHeap_touch(heap);return true;
    }
    case 8: {
        if(!charged(tracker,288,input))break;
        NBTString *s=nbt_readUTF(heap,input);if(!s)break;
        ((NBTTagString*)tag)->data=s;MCObjectHeap_touch(heap);
        return charged(tracker,nbt_i32((uint32_t)s->length*16u),input);
    }
    case 9: {
        if(!charged(tracker,296,input) || depth>512)break;
        NBTTagList *l=(NBTTagList*)tag;l->tagType=nbt_i8(mc_get_u8(input));MCObjectHeap_touch(heap);
        int32_t n=mc_get_i32(input);
        if(input->failed || (l->tagType==0 && n>0) || !charged(tracker,(int64_t)n*32,input) || n<0)break;
        /* Source reads allocate a new ArrayList rather than reuse old storage. */
        NBTRefStorage *s=NULL;
        if(n) {
            if((size_t)n>(SIZE_MAX-sizeof(NBTRefStorage))/sizeof(NBTBase*))break;
            if(!available(input,(size_t)n))break;
            NBTTagList *temporary=NBTTagList_new(heap);
            if(!temporary || !nbt_listReserve(temporary,n))break;
            s=temporary->storage;
        }
        l->storage=s;l->count=0;MCObjectHeap_touch(heap);
        for(int32_t i=0;i<n;i++) {
            NBTBase *child=readFactory(heap,l->tagType,empty);
            if(!child || !read(child,input,depth+1,tracker,empty)) { input->failed=true;return false; }
            l->storage->data[l->count++]=child;MCObjectHeap_touch(heap);
        }
        return true;
    }
    case 10: {
        if(!charged(tracker,384,input) || depth>512)break;
        NBTTagCompound *c=(NBTTagCompound*)tag;nbt_compoundClear(c);
        while(!input->failed) {
            int8_t id=nbt_i8(mc_get_u8(input));if(input->failed)break;
            if(!id)return true;
            NBTString *key=nbt_readUTF(heap,input);
            if(!key || !charged(tracker,nbt_i32(224u+16u*(uint32_t)key->length),input))break;
            NBTBase *child=readFactory(heap,id,empty);
            if(!child || !read(child,input,depth+1,tracker,empty))break;
            bool duplicate=NBTTagCompound_getTag(c,key)!=NULL;
            if(!NBTTagCompound_setTag(c,key,child))break;
            if(duplicate && !charged(tracker,288,input))break;
        }
        break;
    }
    case 11: {
        if(!charged(tracker,192,input))break;
        int32_t n=mc_get_i32(input);
        if(input->failed || !charged(tracker,nbt_i32((uint32_t)n*32u),input) || n<0 ||
           (size_t)n>SIZE_MAX/4 || !available(input,(size_t)n*4))break;
        NBTIntArrayStorage *s=NBTIntArrayStorage_new(heap,NULL,n);if(!s)break;
        ((NBTTagIntArray*)tag)->data=s;MCObjectHeap_touch(heap);
        for(int32_t i=0;i<n;i++)s->data[i]=mc_get_i32(input);
        return !input->failed;
    }
    default:break;
    }
    input->failed=true;return false;
}
bool NBTBase_read(NBTBase *tag,mc_buf *input,int32_t depth,NBTSizeTracker *tracker) {
    NBTString *empty=NULL;
    return read(tag,input,depth,tracker,&empty);
}
static bool write(NBTBase *tag,mc_buf *output,int32_t depth) {
    if(!tag || output->failed || depth>513) { output->failed=true;return false; }
    switch(tag->type) {
    case 0:return true;
    case 1:mc_put_u8(output,(uint8_t)((NBTTagByte*)tag)->data);break;
    case 2:mc_put_i16(output,((NBTTagShort*)tag)->data);break;
    case 3:mc_put_i32(output,((NBTTagInt*)tag)->data);break;
    case 4:mc_put_i64(output,((NBTTagLong*)tag)->data);break;
    case 5:mc_put_i32(output,nbt_i32(floatBits(((NBTTagFloat*)tag)->data)));break;
    case 6: {
        uint64_t raw=doubleBits(((NBTTagDouble*)tag)->data);int64_t bits;memcpy(&bits,&raw,8);mc_put_i64(output,bits);break;
    }
    case 7: {
        NBTByteArrayStorage *s=((NBTTagByteArray*)tag)->data;if(!s) { output->failed=true;break; }
        mc_put_i32(output,s->length);mc_put_bytes(output,s->values,(size_t)s->length);break;
    }
    case 8:return nbt_writeUTF(output,((NBTTagString*)tag)->data);
    case 9: {
        NBTTagList *l=(NBTTagList*)tag;
        int8_t type=l->count?l->storage->data[0]->type:0;
        if(l->tagType!=type) { l->tagType=type;MCObjectHeap_touch(tag->object.heap); }
        mc_put_u8(output,(uint8_t)l->tagType);mc_put_i32(output,l->count);
        for(int32_t i=0;i<l->count;i++)if(!write(l->storage->data[i],output,depth+1))return false;
        break;
    }
    case 10: {
        NBTTagCompound *c=(NBTTagCompound*)tag;
        if(c->storage)for(int32_t i=0;i<c->storage->capacity;i++)for(NBTCompoundEntry *e=c->storage->data[i];e;e=e->next) {
            if(!e->value) { output->failed=true;return false; }
            mc_put_u8(output,(uint8_t)e->value->type);
            if(e->value->type && (!nbt_writeUTF(output,e->key) || !write(e->value,output,depth+1)))return false;
        }
        mc_put_u8(output,0);break;
    }
    case 11: {
        NBTIntArrayStorage *s=((NBTTagIntArray*)tag)->data;if(!s) { output->failed=true;break; }
        mc_put_i32(output,s->length);
        for(int32_t i=0;i<s->length;i++)mc_put_i32(output,s->data[i]);
        break;
    }
    default:output->failed=true;break;
    }
    return !output->failed;
}
bool NBTBase_write(NBTBase *tag,mc_buf *output) { return output && write(tag,output,0); }
bool NBTWire_decodeCompound(MCObjectHeap *heap,mc_buf *input,NBTSizeTracker *tracker,NBTTagCompound **output) {
    if(!heap || !input || !output || !tracker || input->failed)return false;
    int8_t id=nbt_i8(mc_get_u8(input));if(input->failed)return false;
    if(id==0) { *output=NULL;return true; }
    if(id!=10 || !nbt_readUTF(heap,input)) { input->failed=true;return false; }
    NBTTagCompound *c=NBTTagCompound_new(heap);
    if(!c || !NBTBase_read((NBTBase*)c,input,0,tracker))return false;
    *output=c;return true;
}
bool NBTWire_encodeCompound(mc_buf *output,NBTTagCompound *tag) {
    if(!output)return false;
    mc_put_u8(output,tag?10:0);
    if(!tag)return !output->failed;
    mc_put_i16(output,0);
    return NBTBase_write((NBTBase*)tag,output);
}
