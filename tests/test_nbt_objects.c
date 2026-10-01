#include "nbt/NBTBase.h"
#include "nbt/NBTTagCompound.h"
#include "nbt/NBTTagList.h"
#include "nbt/NBTTagByte.h"
#include "nbt/NBTTagShort.h"
#include "nbt/NBTTagInt.h"
#include "nbt/NBTTagLong.h"
#include "nbt/NBTTagFloat.h"
#include "nbt/NBTTagDouble.h"
#include "nbt/NBTTagString.h"
#include "nbt/NBTTagByteArray.h"
#include "nbt/NBTTagIntArray.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#v);exit(1); } } while(0)
static MCObjectHeap *heap(void) { MCObjectHeap *h=MCObjectHeap_new(8u*1024u*1024u);CHECK(h);return h; }
static void object_refs(void) {
    MCObjectHeap *h=heap();MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));
    NBTTagCompound *root=NBTTagCompound_new(h),*child=NBTTagCompound_new(h);CHECK(root && child);
    CHECK(NBTTagCompound_setTag_ascii(root,"a",(NBTBase*)child));CHECK(NBTTagCompound_setTag_ascii(root,"b",(NBTBase*)child));
    CHECK(NBTTagCompound_getTag_ascii(root,"a")== (NBTBase*)child);
    CHECK(NBTTagCompound_getCompoundTag_ascii(root,"b")==child);
    CHECK(NBTTagCompound_getCompoundTag_ascii(root,"missing")!=NBTTagCompound_getCompoundTag_ascii(root,"missing"));
    CHECK(!NBTTagCompound_getTag_ascii(root,"missing"));
    NBTTagCompound *copy=(NBTTagCompound*)NBTBase_copy(h,(NBTBase*)root);CHECK(copy);
    CHECK(NBTTagCompound_getTag_ascii(copy,"a")!=(NBTBase*)child);
    CHECK(NBTTagCompound_getTag_ascii(copy,"a")!=NBTTagCompound_getTag_ascii(copy,"b"));
    CHECK(NBTTagCompound_setInteger_ascii(child,"n",17));
    CHECK(NBTTagCompound_getInteger_ascii(NBTTagCompound_getCompoundTag_ascii(root,"b"),"n")==17);
    CHECK(!NBTTagCompound_getInteger_ascii(NBTTagCompound_getCompoundTag_ascii(copy,"b"),"n"));
    NBTTagList *list=NBTTagList_new(h);CHECK(list);
    CHECK(NBTTagList_appendTag(list,(NBTBase*)child));CHECK(NBTTagList_appendTag(list,(NBTBase*)child));
    CHECK(NBTTagList_get(list,0)==(NBTBase*)child && NBTTagList_get(list,1)==(NBTBase*)child);
    NBTTagList *lc=(NBTTagList*)NBTBase_copy(h,(NBTBase*)list);CHECK(lc);
    CHECK(NBTTagList_get(lc,0)!=NBTTagList_get(lc,1));
    CHECK(NBTTagList_get(list,-1)!=NBTTagList_get(list,-1));
    CHECK(NBTBase_getId(NBTTagList_get(list,99))==0);
    NBTTagList *typed=NBTTagList_new(h);CHECK(NBTTagList_appendTag(typed,(NBTBase*)child));
    CHECK(NBTTagList_removeTag(typed,0)==(NBTBase*)child);CHECK(NBTTagList_getTagType(typed)==10);
    CHECK(NBTTagList_getTagType((NBTTagList*)NBTBase_copy(h,(NBTBase*)typed))==10);
    NBTTagList *empty=NBTTagList_new(h);CHECK(!NBTBase_equals((NBTBase*)typed,(NBTBase*)empty));
    mc_buf wire;mc_buf_init(&wire);CHECK(NBTBase_write((NBTBase*)typed,&wire));
    CHECK(wire.len==5 && NBTTagList_getTagType(typed)==0 && NBTBase_equals((NBTBase*)typed,(NBTBase*)empty));mc_buf_free(&wire);
    NBTTagList *ints=NBTTagList_new(h);CHECK(NBTTagList_appendTag(ints,(NBTBase*)NBTTagInt_new(h,7)));
    CHECK(NBTTagList_appendTag(ints,(NBTBase*)child));CHECK(NBTTagList_tagCount(ints)==1);
    CHECK(NBTTagList_set(ints,-1,(NBTBase*)child));CHECK(NBTTagList_set(ints,0,(NBTBase*)child));
    CHECK(NBTTagList_tagCount(ints)==1 && NBTBase_getId(NBTTagList_get(ints,0))==3);
    CHECK(!NBTTagList_getDoubleAt(ints,0) && !NBTTagList_getFloatAt(ints,0));
    CHECK(NBTTagList_getCompoundTagAt(ints,0)!=NBTTagList_getCompoundTagAt(ints,0));
    CHECK(NBTTagCompound_setTag_ascii(root,"ints",(NBTBase*)ints));
    CHECK(NBTTagCompound_getTagList_ascii(root,"ints",10)!=ints);
    CHECK(NBTTagCompound_setTag_ascii(root,"typed",(NBTBase*)typed));
    CHECK(NBTTagCompound_getTagList_ascii(root,"typed",8)==typed);
    NBTString *blank=NBTTagCompound_getString_ascii(root,"missing");
    CHECK(blank==NBTTagCompound_getString_ascii(root,"ints"));
    CHECK(blank==NBTString_literalASCII(h,""));
    CHECK(NBTTagCompound_setTag_ascii(root,"null",NULL));
    CHECK(NBTTagCompound_hasKey_ascii(root,"null") && !NBTTagCompound_getTag_ascii(root,"null"));
    CHECK(NBTTagCompound_getTagId_ascii(root,"null")==0);
    CHECK(NBTTagCompound_hasKeyType_ascii(root,"absent",0));
    NBTCompoundKeySet *keys=NBTTagCompound_getKeySet(root);CHECK(keys && keys==NBTTagCompound_getKeySet(root));
    CHECK(NBTCompoundKeySet_size(keys)==5);CHECK(NBTTagCompound_setTag_ascii(root,"late",(NBTBase*)child));
    CHECK(NBTCompoundKeySet_size(keys)==6);CHECK(NBTCompoundKeySet_contains(keys,NBTString_literalASCII(h,"late")));
    CHECK(NBTCompoundKeySet_remove(keys,NBTString_literalASCII(h,"late")));CHECK(NBTCompoundKeySet_size(keys)==5);
    CHECK(!NBTCompoundKeySet_remove(keys,NBTString_literalASCII(h,"late")));
    CHECK(NBTTagCompound_setTag(root,NULL,(NBTBase*)child));CHECK(NBTTagCompound_hasKey(root,NULL));
    CHECK(NBTTagCompound_getTag(root,NULL)==(NBTBase*)child);CHECK(NBTTagCompound_removeTag(root,NULL));
    NBTTagCompound *other=NBTTagCompound_new(h);NBTTagCompound *nested=NBTTagCompound_new(h);
    CHECK(NBTTagCompound_setInteger_ascii(nested,"merge",42));CHECK(NBTTagCompound_setTag_ascii(other,"a",(NBTBase*)nested));
    CHECK(NBTTagCompound_merge(root,other));CHECK(NBTTagCompound_getCompoundTag_ascii(root,"a")==child);
    CHECK(NBTTagCompound_getInteger_ascii(child,"merge")==42);CHECK(NBTTagCompound_getTag_ascii(other,"a")== (NBTBase*)nested);
    CHECK(!MCObjectHeap_failed(h));MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
}
static void arrays_and_strings(void) {
    MCObjectHeap *h=heap();MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));
    const int8_t bytes[]={1,2};NBTByteArrayStorage *b=NBTByteArrayStorage_new(h,bytes,2);CHECK(b);
    NBTTagByteArray *a=NBTTagByteArray_new(h,b),*a2=NBTTagByteArray_new(h,b);CHECK(a && a2);
    CHECK(NBTTagByteArray_getByteArray(a)==b && NBTTagByteArray_getByteArray(a2)==b);
    NBTTagByteArray *ac=(NBTTagByteArray*)NBTBase_copy(h,(NBTBase*)a);CHECK(ac && NBTTagByteArray_getByteArray(ac)!=b);
    CHECK(NBTByteArrayStorage_set(b,0,9));CHECK(NBTByteArrayStorage_data(NBTTagByteArray_getByteArray(a2))[0]==9);
    CHECK(NBTByteArrayStorage_data(NBTTagByteArray_getByteArray(ac))[0]==1);
    const int32_t iv[]={INT_MIN,INT_MAX};NBTIntArrayStorage *ia=NBTIntArrayStorage_new(h,iv,2);CHECK(ia);
    NBTTagIntArray *it=NBTTagIntArray_new(h,ia);CHECK(it && NBTTagIntArray_getIntArray(it)==ia);
    NBTTagIntArray *ic=(NBTTagIntArray*)NBTBase_copy(h,(NBTBase*)it);CHECK(ic && NBTTagIntArray_getIntArray(ic)!=ia);
    CHECK(NBTIntArrayStorage_set(ia,1,-7));CHECK(NBTIntArrayStorage_data(NBTTagIntArray_getIntArray(ic))[1]==INT_MAX);
    NBTTagCompound *c=NBTTagCompound_new(h);NBTString *key=NBTString_literalASCII(h,"b");
    CHECK(NBTTagCompound_setByteArray(c,key,b));CHECK(NBTTagCompound_getByteArray(c,key)==b);
    CHECK(NBTTagCompound_getByteArray(c,NULL)!=NBTTagCompound_getByteArray(c,NULL));
    NBTString *ik=NBTString_literalASCII(h,"i");CHECK(NBTTagCompound_setIntArray(c,ik,ia));CHECK(NBTTagCompound_getIntArray(c,ik)==ia);
    CHECK(NBTTagCompound_getIntArray(c,NULL)!=NBTTagCompound_getIntArray(c,NULL));
    const uint16_t units[]={0,0xd800,0x0022,0xd83d,0xde00};NBTString *s=NBTString_fromUTF16(h,units,5);CHECK(s);
    CHECK(NBTString_length(s)==5 && !memcmp(NBTString_units(s),units,sizeof(units)));
    NBTTagString *st=NBTTagString_new(h,s);CHECK(st && NBTTagString_getString(st)==s);
    NBTTagString *sc=(NBTTagString*)NBTBase_copy(h,(NBTBase*)st);CHECK(sc && NBTTagString_getString(sc)==s);
    char text[16]="keep";CHECK(!NBTString_toUTF8(s,text,sizeof(text)) && !strcmp(text,"keep"));
    NBTString *utf8=NBTString_fromUTF8(h,"\xc3\xa9\xf0\x9f\x98\x80");CHECK(utf8 && NBTString_length(utf8)==3);
    CHECK(NBTString_toUTF8(utf8,text,sizeof(text)) && !strcmp(text,"\xc3\xa9\xf0\x9f\x98\x80"));
    CHECK(!NBTString_toUTF8(utf8,text,3));
    mc_buf wire;mc_buf_init(&wire);CHECK(NBTBase_write((NBTBase*)st,&wire));CHECK(wire.len==2+2+3+1+3+3);
    NBTTagString *read=(NBTTagString*)NBTBase_createNewByType(h,8);NBTSizeTracker tracker;NBTSizeTracker_init(&tracker,2097152);
    CHECK(NBTBase_read((NBTBase*)read,&wire,0,&tracker));CHECK(NBTString_equals(s,NBTTagString_getString(read)));
    CHECK(tracker.read==36+2*5);mc_buf_free(&wire);
    CHECK(NBTString_hashCode(NBTString_fromASCII(h,"abc"))==96354);
    CHECK(NBTString_fromASCII(h,"")!=NBTString_literalASCII(h,""));
    CHECK(!MCObjectHeap_failed(h));MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
}
static void numbers_and_equality(void) {
    MCObjectHeap *h=heap();MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));
    for(int8_t id=0;id<12;id++) { NBTBase *n=NBTBase_createNewByType(h,id);CHECK(n && NBTBase_getId(n)==id); }
    CHECK(!NBTBase_createNewByType(h,12));
    NBTTagLong *l=NBTTagLong_new(h,INT64_MAX);CHECK(l);
    CHECK(NBTTagLong_getLong(l)==INT64_MAX && NBTTagLong_getInt(l)==-1 && NBTTagLong_getShort(l)==-1 && NBTTagLong_getByte(l)==-1);
    CHECK(NBTTagShort_getByte(NBTTagShort_new(h,256))==0);
    CHECK(NBTTagInt_getShort(NBTTagInt_new(h,65535))==-1);
    CHECK(NBTTagByte_getLong(NBTTagByte_new(h,-128))==-128);
    const float fs[]={-0.5f,NAN,INFINITY,-INFINITY,-1.5f};
    const double ds[]={-0.5,NAN,INFINITY,-INFINITY,-1.5};
    const int64_t fl[]={0,0,INT64_MAX,INT64_MIN,-1},dl[]={-1,0,INT64_MAX,INT64_MIN,-2};
    const int32_t expected[]={-1,0,INT32_MAX,INT32_MAX,-2};
    for(size_t i=0;i<sizeof(fs)/sizeof(fs[0]);i++) {
        NBTTagFloat *f=NBTTagFloat_new(h,fs[i]);NBTTagDouble *d=NBTTagDouble_new(h,ds[i]);CHECK(f && d);
        CHECK(NBTTagFloat_getLong(f)==fl[i]);CHECK(NBTTagDouble_getLong(d)==dl[i]);
        CHECK(NBTTagFloat_getInt(f)==expected[i]);CHECK(NBTTagDouble_getInt(d)==expected[i]);
        CHECK(NBTTagFloat_getByte(f)==(i==1?0:i==4?-2:-1));
    }
    NBTTagCompound *c=NBTTagCompound_new(h);CHECK(NBTTagCompound_setLong_ascii(c,"long",256));
    CHECK(NBTTagCompound_hasKeyType_ascii(c,"long",99) && !NBTTagCompound_getBoolean_ascii(c,"long"));
    CHECK(NBTTagCompound_setDouble_ascii(c,"double",-0.5));CHECK(NBTTagCompound_getBoolean_ascii(c,"double"));
    CHECK(!NBTTagCompound_hasKeyType_ascii(c,"missing",99) && !NBTTagCompound_getInteger_ascii(c,"missing"));
    NBTTagFloat *nan=NBTTagFloat_new(h,NAN);CHECK(!NBTBase_equals((NBTBase*)nan,(NBTBase*)nan));
    NBTTagCompound *a=NBTTagCompound_new(h),*b=NBTTagCompound_new(h);
    CHECK(NBTTagCompound_setTag_ascii(a,"n",(NBTBase*)nan));CHECK(NBTTagCompound_setTag_ascii(b,"n",(NBTBase*)nan));
    CHECK(NBTBase_equals((NBTBase*)a,(NBTBase*)b));CHECK(!NBTBase_equals((NBTBase*)a,NBTBase_copy(h,(NBTBase*)a)));
    NBTTagList *x=NBTTagList_new(h),*y=NBTTagList_new(h);CHECK(NBTTagList_appendTag(x,(NBTBase*)nan));CHECK(NBTTagList_appendTag(y,(NBTBase*)nan));
    CHECK(!NBTBase_equals((NBTBase*)x,(NBTBase*)y));CHECK(NBTBase_equals((NBTBase*)x,(NBTBase*)x));
    NBTTagFloat *plus=NBTTagFloat_new(h,0.0f),*minus=NBTTagFloat_new(h,-0.0f);
    CHECK(NBTBase_equals((NBTBase*)plus,(NBTBase*)minus));CHECK(NBTBase_hashCode((NBTBase*)plus)!=NBTBase_hashCode((NBTBase*)minus));
    CHECK((uint32_t)NBTBase_hashCode((NBTBase*)nan)==(UINT32_C(0x7fc00000)^5u));
    mc_buf wire;mc_buf_init(&wire);CHECK(NBTBase_write((NBTBase*)nan,&wire));
    const uint8_t fn[]={0x7f,0xc0,0,0};CHECK(wire.len==4 && !memcmp(wire.data,fn,4));mc_buf_clear(&wire);
    CHECK(NBTBase_write((NBTBase*)NBTTagDouble_new(h,-NAN),&wire));
    const uint8_t dn[]={0x7f,0xf8,0,0,0,0,0,0};CHECK(wire.len==8 && !memcmp(wire.data,dn,8));mc_buf_free(&wire);
    CHECK(!MCObjectHeap_failed(h));MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
}
static void graph_lifetime(void) {
    MCObjectHeap *h=heap();MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));
    NBTTagCompound *a=NBTTagCompound_new(h);NBTTagList *list=NBTTagList_new(h);
    CHECK(NBTTagCompound_setTag_ascii(a,"self",(NBTBase*)a));CHECK(NBTTagList_appendTag(list,(NBTBase*)list));
    CHECK(NBTTagCompound_setTag_ascii(a,"list",(NBTBase*)list));CHECK(NBTTagList_get(list,0)==(NBTBase*)list);
    NBTByteArrayStorage *data=NBTByteArrayStorage_new(h,NULL,3);
    CHECK(NBTTagCompound_setTag_ascii(a,"one",(NBTBase*)NBTTagByteArray_new(h,data)));
    CHECK(NBTTagCompound_setTag_ascii(a,"two",(NBTBase*)NBTTagByteArray_new(h,data)));
    NBTCompoundKeySet *view=NBTTagCompound_getKeySet(a);CHECK(view);
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject*)a));CHECK(MCObjectRootScope_pin(&scope,(MCObject*)view));
    CHECK(!MCObjectHeap_collect(h) && !MCObjectHeap_clone(h));MCObjectRootScope_end(&scope);
    CHECK(MCObjectHeap_collect(h));size_t objects=MCObjectHeap_liveObjects(h);CHECK(objects>5);
    MCObjectHeap *working=MCObjectHeap_clone(h);CHECK(working);
    MCObjectRoot workroot={0};CHECK(MCObjectRoot_rebind(&workroot,working,&root));
    MCObjectRootScope workScope={0};CHECK(MCObjectRootScope_begin(&workScope,working));
    NBTTagCompound *w=(NBTTagCompound*)MCObjectRoot_get(&workroot);CHECK(w && w!=a);
    CHECK(NBTTagCompound_getTag_ascii(w,"self")== (NBTBase*)w);
    NBTTagList *wl=(NBTTagList*)NBTTagCompound_getTag_ascii(w,"list");CHECK(NBTTagList_get(wl,0)==(NBTBase*)wl);
    NBTByteArrayStorage *one=NBTTagByteArray_getByteArray((NBTTagByteArray*)NBTTagCompound_getTag_ascii(w,"one"));
    NBTByteArrayStorage *two=NBTTagByteArray_getByteArray((NBTTagByteArray*)NBTTagCompound_getTag_ascii(w,"two"));
    CHECK(one==two && one!=data);CHECK(NBTByteArrayStorage_set(one,0,7));MCObjectRootScope_end(&workScope);
    CHECK(MCObjectRootScope_begin(&scope,h));
    a=(NBTTagCompound*)MCObjectRoot_get(&root);
    data=NBTTagByteArray_getByteArray((NBTTagByteArray*)NBTTagCompound_getTag_ascii(a,"one"));
    CHECK(!NBTByteArrayStorage_data(data)[0]);MCObjectRootScope_end(&scope);
    /* Ending a source borrow scope invalidates an older platform snapshot. */
    CHECK(!MCObjectHeap_adopt(h,working));MCObjectHeap_free(working);
    working=MCObjectHeap_clone(h);CHECK(working);CHECK(MCObjectRoot_rebind(&workroot,working,&root));
    CHECK(MCObjectRootScope_begin(&workScope,working));
    w=(NBTTagCompound*)MCObjectRoot_get(&workroot);
    one=NBTTagByteArray_getByteArray((NBTTagByteArray*)NBTTagCompound_getTag_ascii(w,"one"));
    CHECK(NBTByteArrayStorage_set(one,0,7));MCObjectRootScope_end(&workScope);
    CHECK(MCObjectHeap_adopt(h,working));CHECK((NBTTagCompound*)MCObjectRoot_get(&root)==w);
    CHECK(MCObjectRoot_rebind(&workroot,h,&workroot));MCObjectHeap_free(working);
    CHECK(MCObjectRootScope_begin(&scope,h));w=(NBTTagCompound*)MCObjectRoot_get(&root);
    two=NBTTagByteArray_getByteArray((NBTTagByteArray*)NBTTagCompound_getTag_ascii(w,"two"));
    CHECK(NBTByteArrayStorage_data(two)[0]==7 && ((MCObject*)two)->heap==h);
    CHECK(NBTCompoundKeySet_size(NBTTagCompound_getKeySet(w))==4);
    MCObjectRootScope_end(&scope);
    MCObjectRoot_drop(&root);CHECK(MCObjectHeap_collect(h));CHECK(!MCObjectHeap_liveObjects(h));MCObjectHeap_free(h);
}
static void wire_and_limits(void) {
    MCObjectHeap *h=heap();MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));
    NBTTagCompound *c=NBTTagCompound_new(h);CHECK(NBTTagCompound_setInteger_ascii(c,"answer",42));
    NBTTagList *l=NBTTagList_new(h);CHECK(NBTTagList_appendTag(l,(NBTBase*)NBTTagDouble_new(h,-0.5)));
    CHECK(NBTTagCompound_setTag_ascii(c,"list",(NBTBase*)l));
    mc_buf wire;mc_buf_init(&wire);CHECK(NBTWire_encodeCompound(&wire,c));
    NBTSizeTracker tracker;NBTSizeTracker_init(&tracker,2097152);NBTTagCompound *out=NULL;
    CHECK(NBTWire_decodeCompound(h,&wire,&tracker,&out));CHECK(out && out!=c && NBTBase_equals((NBTBase*)c,(NBTBase*)out));
    CHECK(wire.pos==wire.len && tracker.read>(int64_t)wire.len);mc_buf_clear(&wire);
    /* Duplicate keys are original HashMap last-wins, not wire rejection. */
    mc_put_u8(&wire,10);mc_put_i16(&wire,0);
    for(int i=1;i<=2;i++) { mc_put_u8(&wire,3);mc_put_i16(&wire,1);mc_put_u8(&wire,'x');mc_put_i32(&wire,i); }
    mc_put_u8(&wire,0);NBTSizeTracker_init(&tracker,2097152);
    CHECK(NBTWire_decodeCompound(h,&wire,&tracker,&out));CHECK(NBTTagCompound_getInteger_ascii(out,"x")==2);
    CHECK(NBTCompoundKeySet_size(NBTTagCompound_getKeySet(out))==1 && tracker.read==168);mc_buf_clear(&wire);
    /* Original DataInput.readUTF accepts overlong sequences and writes canonical UTF. */
    const uint8_t overlong[]={0,2,0xc0,0x81};mc_put_bytes(&wire,overlong,sizeof(overlong));
    NBTTagString *str=(NBTTagString*)NBTBase_createNewByType(h,8);NBTSizeTracker_initInfinite(&tracker);
    CHECK(NBTBase_read((NBTBase*)str,&wire,0,&tracker));CHECK(NBTString_units(NBTTagString_getString(str))[0]==1);
    mc_buf_clear(&wire);CHECK(NBTBase_write((NBTBase*)str,&wire));CHECK(wire.len==3 && wire.data[0]==0 && wire.data[1]==1 && wire.data[2]==1);mc_buf_clear(&wire);
    /* Unknown element type is legal on an empty original List until write. */
    mc_put_u8(&wire,255);mc_put_i32(&wire,0);l=NBTTagList_new(h);NBTSizeTracker_initInfinite(&tracker);
    CHECK(NBTBase_read((NBTBase*)l,&wire,0,&tracker));CHECK(NBTTagList_getTagType(l)==-1);
    mc_buf_clear(&wire);CHECK(NBTBase_write((NBTBase*)l,&wire));CHECK(NBTTagList_getTagType(l)==0);mc_buf_clear(&wire);
    mc_put_u8(&wire,0);mc_put_i32(&wire,1);NBTSizeTracker_initInfinite(&tracker);
    CHECK(!NBTBase_read((NBTBase*)NBTTagList_new(h),&wire,0,&tracker));mc_buf_clear(&wire);
    CHECK(NBTWire_encodeCompound(&wire,c));size_t size=wire.len;
    for(size_t cut=0;cut<size;cut++) {
        mc_buf truncated=wire;truncated.len=cut;truncated.pos=0;truncated.failed=false;
        NBTSizeTracker_init(&tracker,2097152);NBTTagCompound *keep=c;
        CHECK(!NBTWire_decodeCompound(h,&truncated,&tracker,&keep));CHECK(keep==c);
    }
    wire.pos=0;NBTSizeTracker_init(&tracker,1);out=c;CHECK(!NBTWire_decodeCompound(h,&wire,&tracker,&out));CHECK(out==c && tracker.failed);mc_buf_clear(&wire);
    mc_put_u8(&wire,0);NBTSizeTracker_init(&tracker,0);out=c;CHECK(NBTWire_decodeCompound(h,&wire,&tracker,&out) && !out);mc_buf_clear(&wire);
    /* Container depth512 and a scalar child at513 are allowed by the original. */
    mc_put_u8(&wire,10);mc_put_i16(&wire,0);
    for(int i=0;i<512;i++) { mc_put_u8(&wire,10);mc_put_i16(&wire,1);mc_put_u8(&wire,'n'); }
    mc_put_u8(&wire,1);mc_put_i16(&wire,1);mc_put_u8(&wire,'v');mc_put_u8(&wire,7);
    for(int i=0;i<=512;i++)mc_put_u8(&wire,0);
    NBTSizeTracker_init(&tracker,2097152);CHECK(NBTWire_decodeCompound(h,&wire,&tracker,&out));
    NBTTagCompound *deep=out;for(int i=0;i<512;i++)deep=NBTTagCompound_getCompoundTag_ascii(deep,"n");
    CHECK(NBTTagCompound_getByte_ascii(deep,"v")==7);CHECK(NBTBase_copy(h,(NBTBase*)out));
    mc_buf_clear(&wire);mc_put_u8(&wire,10);mc_put_i16(&wire,0);
    for(int i=0;i<513;i++) { mc_put_u8(&wire,10);mc_put_i16(&wire,1);mc_put_u8(&wire,'n'); }
    for(int i=0;i<=513;i++)mc_put_u8(&wire,0);
    NBTSizeTracker_init(&tracker,2097152);out=c;CHECK(!NBTWire_decodeCompound(h,&wire,&tracker,&out) && out==c);mc_buf_free(&wire);
    CHECK(!MCObjectHeap_failed(h));MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
}
static void allocation_and_foreign_edges(void) {
    MCObjectHeap *a=MCObjectHeap_new(512),*b=heap();CHECK(a);
    NBTTagCompound *root=NBTTagCompound_new(a);CHECK(root);
    CHECK(!NBTTagCompound_setString_ascii(root,"large",NBTString_fromASCII(b,"foreign")));
    CHECK(MCObjectHeap_failed(a));CHECK(!NBTTagCompound_hasKey_ascii(root,"large"));MCObjectHeap_free(a);MCObjectHeap_free(b);
    a=MCObjectHeap_new(200);CHECK(a);root=NBTTagCompound_new(a);CHECK(root);
    CHECK(!NBTTagCompound_setInteger_ascii(root,"cannot_fit",42));CHECK(MCObjectHeap_failed(a));MCObjectHeap_free(a);
}
static void colliding_keys_and_many_strings(void) {
    MCObjectHeap *h=heap();MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));
    NBTTagCompound *root=NBTTagCompound_new(h);CHECK(root);
    char key[21];key[20]=0;int32_t firstHash=0;
    for(unsigned i=0;i<1024;i++) {
        for(unsigned bit=0;bit<10;bit++) { key[bit*2]=(i&(1u<<bit))?'B':'A';key[bit*2+1]=(i&(1u<<bit))?'B':'a'; }
        NBTString *s=NBTString_fromASCII(h,key);CHECK(s);
        int32_t hash=NBTString_hashCode(s);if(!i)firstHash=hash;CHECK(hash==firstHash);
        CHECK(NBTTagCompound_setTag(root,s,(NBTBase*)NBTTagInt_new(h,(int32_t)i)));
    }
    CHECK(NBTCompoundKeySet_size(NBTTagCompound_getKeySet(root))==1024);
    for(unsigned i=0;i<1024;i++) {
        for(unsigned bit=0;bit<10;bit++) { key[bit*2]=(i&(1u<<bit))?'B':'A';key[bit*2+1]=(i&(1u<<bit))?'B':'a'; }
        CHECK(NBTTagCompound_getInteger_ascii(root,key)==(int32_t)i);
    }
    mc_buf wire;mc_buf_init(&wire);CHECK(NBTWire_encodeCompound(&wire,root));
    NBTSizeTracker tracker;NBTSizeTracker_init(&tracker,2097152);NBTTagCompound *decoded=NULL;
    CHECK(NBTWire_decodeCompound(h,&wire,&tracker,&decoded));CHECK(NBTBase_equals((NBTBase*)root,(NBTBase*)decoded));
    for(unsigned i=0;i<1024;i++) {
        unsigned number=(i*317u)&1023u;
        for(unsigned bit=0;bit<10;bit++) { key[bit*2]=(number&(1u<<bit))?'B':'A';key[bit*2+1]=(number&(1u<<bit))?'B':'a'; }
        CHECK(NBTTagCompound_removeTag_ascii(decoded,key));CHECK(!NBTTagCompound_hasKey_ascii(decoded,key));
        CHECK(NBTCompoundKeySet_size(NBTTagCompound_getKeySet(decoded))==(int32_t)(1023-i));
    }
    CHECK(NBTBase_hasNoTags((NBTBase*)decoded));mc_buf_clear(&wire);
    /* One empty literal lookup is shared through the whole decode; a large
       string list must not repeatedly scan all previously allocated nodes. */
    const int32_t count=20000;mc_put_u8(&wire,8);mc_put_i32(&wire,count);
    for(int32_t i=0;i<count;i++) { mc_put_i16(&wire,1);mc_put_u8(&wire,'x'); }
    NBTTagList *strings=NBTTagList_new(h);NBTSizeTracker_init(&tracker,2097152);
    CHECK(NBTBase_read((NBTBase*)strings,&wire,0,&tracker));CHECK(NBTTagList_tagCount(strings)==count);
    CHECK(tracker.read==37+(int64_t)count*42);
    CHECK(NBTString_equalsASCII(NBTTagString_getString((NBTTagString*)NBTTagList_get(strings,count-1)),"x"));
    CHECK(!MCObjectHeap_failed(h));mc_buf_free(&wire);MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
}
int main(void) {
    object_refs();arrays_and_strings();numbers_and_equality();graph_lifetime();wire_and_limits();allocation_and_foreign_edges();colliding_keys_and_many_strings();
    printf("NBT objects: %u checks passed\n",checks);return 0;
}
