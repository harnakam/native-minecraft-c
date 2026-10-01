#include "util/NativePrimitiveArray.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"array check %u failed line %d: %s\n",checks,__LINE__,#x);exit(1);}}while(0)
static const MCObjectClass leaf={"test.array.leaf",MCObjectHeap_plainClone,NULL,NULL};
static void values_and_aliases(void) {
    MCObjectHeap *heap=MCObjectHeap_new(65536);CHECK(heap);
    NativeByteArray *bytes=NativeByteArray_new(heap,2);CHECK(bytes);
    NativeCharArray *chars=NativeCharArray_new(heap,2);CHECK(chars);
    NativeShortArray *shorts=NativeShortArray_new(heap,2);CHECK(shorts);
    NativeBooleanArray *flags=NativeBooleanArray_new(heap,2);CHECK(flags);
    NativeObjectArray *refs=NativeObjectArray_new(heap,4);CHECK(refs);
    int8_t b=12;uint16_t c=12;int16_t s=12;bool f=true;
    CHECK(NativeByteArray_get(bytes,0,&b)&&b==0);
    CHECK(NativeCharArray_get(chars,0,&c)&&c==0);
    CHECK(NativeShortArray_get(shorts,0,&s)&&s==0);
    CHECK(NativeBooleanArray_get(flags,0,&f)&&!f);
    CHECK(NativeByteArray_set(bytes,1,INT8_MIN));
    CHECK(NativeCharArray_set(chars,1,UINT16_MAX));
    CHECK(NativeShortArray_set(shorts,1,INT16_MIN));
    CHECK(NativeBooleanArray_set(flags,1,true));
    CHECK(NativeByteArray_get(bytes,1,&b)&&b==-128);
    CHECK(NativeCharArray_get(chars,1,&c)&&c==65535);
    CHECK(NativeShortArray_get(shorts,1,&s)&&s==-32768);
    CHECK(NativeBooleanArray_get(flags,1,&f)&&f);
    CHECK(NativeObjectArray_set(refs,0,(MCObject *)bytes));
    CHECK(NativeObjectArray_set(refs,1,(MCObject *)bytes));
    CHECK(NativeObjectArray_set(refs,2,(MCObject *)refs));
    CHECK(NativeObjectArray_set(refs,3,(MCObject *)chars));
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,heap,(MCObject *)refs));
    CHECK(MCObjectHeap_collect(heap));CHECK(MCObjectHeap_liveObjects(heap)==3);
    MCObjectHeap *copy=MCObjectHeap_clone(heap);CHECK(copy);
    MCObjectRoot branch={0};CHECK(MCObjectRoot_rebind(&branch,copy,&root));
    NativeObjectArray *cloned=(NativeObjectArray *)MCObjectRoot_get(&branch);CHECK(cloned&&cloned!=refs);
    CHECK(cloned->values[0]==cloned->values[1]&&cloned->values[0]!=(MCObject *)bytes);
    CHECK(cloned->values[2]==(MCObject *)cloned);
    CHECK(((NativeByteArray *)cloned->values[0])->values[1]==-128);
    CHECK(NativeByteArray_set((NativeByteArray *)cloned->values[0],1,127));
    CHECK(bytes->values[1]==-128);
    CHECK(MCObjectHeap_canAdopt(heap,copy));CHECK(MCObjectHeap_adopt(heap,copy));
    refs=(NativeObjectArray *)MCObjectRoot_get(&root);
    CHECK(refs->values[0]==refs->values[1]&&refs->values[2]==(MCObject *)refs);
    CHECK(((NativeByteArray *)refs->values[0])->values[1]==127);
    CHECK(MCObjectHeap_collect(heap)&&MCObjectHeap_liveObjects(heap)==3);
    MCObjectRoot_drop(&root);CHECK(MCObjectHeap_collect(heap)&&MCObjectHeap_liveObjects(heap)==0);
    MCObjectHeap_free(copy);MCObjectHeap_free(heap);
}
static void errors(void) {
    MCObjectHeap *heap=MCObjectHeap_new(4096);CHECK(heap);
    NativeByteArray *bytes=NativeByteArray_new(heap,1);CHECK(bytes);
    int8_t out=42;CHECK(!NativeByteArray_get(bytes,-1,&out));
    CHECK(out==42&&MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(4096);CHECK(heap);bytes=NativeByteArray_new(heap,1);CHECK(bytes);
    bytes->length=2;CHECK(!NativeByteArray_isInstance((MCObject *)bytes));
    out=42;CHECK(!NativeByteArray_get(bytes,1,&out));CHECK(out==42&&MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(4096);MCObjectHeap *foreign=MCObjectHeap_new(4096);CHECK(heap&&foreign);
    NativeObjectArray *refs=NativeObjectArray_new(heap,1);MCObject *obj=MCObjectHeap_alloc(foreign,sizeof(MCObject),&leaf);CHECK(refs&&obj);
    CHECK(!NativeObjectArray_set(refs,0,obj));CHECK(refs->values[0]==NULL&&MCObjectHeap_failed(heap)&&!MCObjectHeap_failed(foreign));
    MCObjectHeap_free(heap);MCObjectHeap_free(foreign);
    heap=MCObjectHeap_new(4096);CHECK(!NativeShortArray_new(heap,-1)&&MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(sizeof(NativeCharArray));CHECK(!NativeCharArray_new(heap,1)&&MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
}
int main(void){values_and_aliases();errors();printf("source native arrays: %u checks passed\n",checks);return 0;}
