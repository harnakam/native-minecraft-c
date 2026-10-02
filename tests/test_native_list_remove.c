#include "util/NativeReferenceList.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"list removal check %u line %d: %s\n",checks,__LINE__,#x); exit(1); } } while(0)
typedef struct Leaf { MCObject object; int32_t value; } Leaf;
static const MCObjectClass leafClass={"test.ListRemoval.Leaf",MCObjectHeap_plainClone,NULL,NULL};
typedef enum { IDENTITY,VALUE,REJECT,CLEAR_TRUE,CLEAR_FALSE,GROW,TRUNCATE,NULL_STORAGE,BAD_COPY,
               THROW_PREFIX,FAIL_PREFIX,INVALID_RESULT,FOREIGN_CONTEXT,FOREIGN_STORAGE } Action;
typedef struct Fixture {
    MCObject object;
    NativeReferenceList *list;
    Leaf *query,*added;
    MCObjectHeap *foreign;
    Action action;
    unsigned calls;
    MCObject *seen[16];
} Fixture;
static void trace(MCObject *object,MCObjectVisitor visit,void *context) {
    Fixture *f=(Fixture *)object;
    f->list=(NativeReferenceList *)visit((MCObject *)f->list,context);
    f->query=(Leaf *)visit((MCObject *)f->query,context);
    f->added=(Leaf *)visit((MCObject *)f->added,context);
    for(unsigned i=0;i<16;++i)f->seen[i]=visit(f->seen[i],context);
}
static const MCObjectClass fixtureClass={"test.ListRemoval.Context",MCObjectHeap_plainClone,trace,NULL};
static Leaf *leaf(MCObjectHeap *heap,int32_t value) {
    Leaf *result=(Leaf *)MCObjectHeap_alloc(heap,sizeof(*result),&leafClass);
    CHECK(result);result->value=value;return result;
}
static Fixture *fixture(MCObjectHeap *heap,Action action) {
    Fixture *f=(Fixture *)MCObjectHeap_alloc(heap,sizeof(*f),&fixtureClass);
    CHECK(f);f->list=NativeReferenceList_new(heap);f->query=leaf(heap,7);f->added=leaf(heap,9);
    CHECK(f->list);f->action=action;return f;
}
static NativeArrayResult equals(MCObject *object,MCObject *query,MCObject *stored,bool *out) {
    Fixture *f=(Fixture *)object;
    CHECK(query==(MCObject *)f->query);
    CHECK(MCObjectHeap_hasBorrowers(object->heap));
    CHECK(!MCObjectHeap_collect(object->heap));
    CHECK(!MCObjectHeap_failed(object->heap));
    CHECK(f->calls<16);f->seen[f->calls++]=stored;
    switch(f->action) {
    case IDENTITY:*out=query==stored;return NATIVE_ARRAY_OK;
    case VALUE:*out=stored&&((Leaf *)stored)->value==f->query->value;return NATIVE_ARRAY_OK;
    case REJECT:*out=false;return NATIVE_ARRAY_OK;
    case CLEAR_TRUE:case CLEAR_FALSE:
        CHECK(NativeReferenceList_clear(f->list));*out=f->action==CLEAR_TRUE;return NATIVE_ARRAY_OK;
    case GROW:
        if(f->calls==1)for(int i=0;i<8;++i)CHECK(NativeReferenceList_add(f->list,(MCObject *)f->added));
        *out=stored==(MCObject *)f->added;return NATIVE_ARRAY_OK;
    case TRUNCATE:
        if(f->calls==1){*out=false;return NATIVE_ARRAY_OK;}
        CHECK(NativeReferenceList_clear(f->list));CHECK(NativeReferenceList_add(f->list,(MCObject *)f->added));
        *out=true;return NATIVE_ARRAY_OK;
    case NULL_STORAGE:f->list->storage=NULL;f->list->size=0;MCObjectHeap_touch(object->heap);
        *out=true;return NATIVE_ARRAY_OK;
    case BAD_COPY:f->list->size=f->list->storage->capacity+1;MCObjectHeap_touch(object->heap);
        *out=true;return NATIVE_ARRAY_OK;
    case THROW_PREFIX:case FAIL_PREFIX:
        NativeReferenceList_set(f->list,0,(MCObject *)f->added);
        return f->action==THROW_PREFIX?NATIVE_ARRAY_EXCEPTION:NATIVE_ARRAY_FAILURE;
    case INVALID_RESULT:return (NativeArrayResult)99;
    case FOREIGN_CONTEXT:f->object.heap=f->foreign;return NATIVE_ARRAY_EXCEPTION;
    case FOREIGN_STORAGE: {
        NativeReferenceList *foreign=NativeReferenceList_new(f->foreign);CHECK(foreign);
        f->list->storage=foreign->storage;return NATIVE_ARRAY_EXCEPTION;
    }
    }
    return NATIVE_ARRAY_FAILURE;
}
static const NativeReferenceListEqualsMethods methods={equals};
static NativeArrayResult remove_query(Fixture *f,bool *out) {
#ifdef NATIVE_LIST_REMOVE_PRIOR_RED
    bool equal=false;
    NativeArrayResult result=methods.equals((MCObject *)f,(MCObject *)f->query,
        NativeReferenceList_get(f->list,0),&equal);
    if(result==NATIVE_ARRAY_OK&&equal)NativeReferenceList_remove(f->list,0);
    if(MCObjectHeap_failed(f->object.heap))return NATIVE_ARRAY_FAILURE;
    if(result==NATIVE_ARRAY_OK)*out=equal;
    return result;
#else
    return NativeReferenceList_removeObjectSource(f->list,(MCObject *)f->query,&methods,(MCObject *)f,out);
#endif
}
static void clear_during_equals(void) {
    MCObjectHeap *heap=MCObjectHeap_new(65536);Fixture *f=fixture(heap,CLEAR_TRUE);
    CHECK(NativeReferenceList_add(f->list,(MCObject *)f->query));
    uint32_t before=f->list->modCount;bool removed=true;
    CHECK(remove_query(f,&removed)==NATIVE_ARRAY_EXCEPTION);
    CHECK(removed&&f->calls==1&&f->list->size==-1&&f->list->modCount==before+2);
    CHECK(!MCObjectHeap_failed(heap));
#ifndef NATIVE_LIST_REMOVE_PRIOR_RED
    int32_t size=88;MCObject *sentinel=(MCObject *)f->query;
    CHECK(NativeReferenceList_sizeSource(f->list,&size)==NATIVE_ARRAY_OK&&size==-1);
    CHECK(NativeReferenceList_getSource(f->list,0,&sentinel)==NATIVE_ARRAY_EXCEPTION&&sentinel==(MCObject *)f->query);
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,heap,(MCObject *)f));
    CHECK(MCObjectHeap_collect(heap));
    MCObjectHeap *copy=MCObjectHeap_clone(heap);CHECK(copy);
    MCObjectRoot copyRoot={0};CHECK(MCObjectRoot_rebind(&copyRoot,copy,&root));
    Fixture *cloned=(Fixture *)MCObjectRoot_get(&copyRoot);
    CHECK(cloned!=f&&cloned->list!=f->list&&cloned->list->size==-1);
    CHECK(cloned->seen[0]==(MCObject *)cloned->query);
    CHECK(MCObjectHeap_canAdopt(heap,copy)&&MCObjectHeap_adopt(heap,copy));
    f=(Fixture *)MCObjectRoot_get(&root);CHECK(f==cloned&&f->list->size==-1);
    MCObjectHeap_free(copy);MCObjectRoot_drop(&root);
#endif
    MCObjectHeap_free(heap);
}
#ifndef NATIVE_LIST_REMOVE_PRIOR_RED
static void first_equal_and_null(void) {
    MCObjectHeap *heap=MCObjectHeap_new(65536);Fixture *f=fixture(heap,VALUE);
    Leaf *first=leaf(heap,7),*second=leaf(heap,7),*other=leaf(heap,2);
    CHECK(NativeReferenceList_add(f->list,(MCObject *)other));
    CHECK(NativeReferenceList_add(f->list,(MCObject *)first));
    CHECK(NativeReferenceList_add(f->list,(MCObject *)second));
    bool removed=false;uint32_t before=f->list->modCount;
    CHECK(remove_query(f,&removed)==NATIVE_ARRAY_OK&&removed);
    CHECK(f->calls==2&&f->seen[0]==(MCObject *)other&&f->seen[1]==(MCObject *)first);
    CHECK(f->list->size==2&&f->list->modCount==before+1);
    CHECK(f->list->storage->items[0]==(MCObject *)other&&f->list->storage->items[1]==(MCObject *)second);
    CHECK(f->list->storage->items[2]==NULL);
    CHECK(NativeReferenceList_clear(f->list));f->calls=0;f->action=REJECT;
    CHECK(NativeReferenceList_add(f->list,(MCObject *)f->query));
    CHECK(remove_query(f,&removed)==NATIVE_ARRAY_OK&&!removed&&f->calls==1);
    CHECK(f->list->size==1); /* identical refs still call query.equals */
    CHECK(NativeReferenceList_add(f->list,NULL)&&NativeReferenceList_add(f->list,NULL));
    CHECK(NativeReferenceList_removeObjectSource(f->list,NULL,NULL,NULL,&removed)==NATIVE_ARRAY_OK&&removed);
    CHECK(f->calls==1&&f->list->size==2&&f->list->storage->items[1]==NULL);
    CHECK(NativeReferenceList_clear(f->list));
    CHECK(NativeReferenceList_removeObjectSource(f->list,(MCObject *)f->query,NULL,NULL,&removed)==NATIVE_ARRAY_OK&&!removed);
    CHECK(!MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
}
static void live_reentrant_lists(void) {
    for(int action=CLEAR_FALSE;action<=BAD_COPY;++action) {
        MCObjectHeap *heap=MCObjectHeap_new(65536);Fixture *f=fixture(heap,(Action)action);
        CHECK(NativeReferenceList_add(f->list,(MCObject *)f->query));
        if(action==TRUNCATE)CHECK(NativeReferenceList_add(f->list,(MCObject *)f->query));
        uint32_t before=f->list->modCount;bool removed=false;
        NativeArrayResult result=remove_query(f,&removed);
        if(action==CLEAR_FALSE)CHECK(result==NATIVE_ARRAY_OK&&!removed&&f->list->size==0&&f->calls==1);
        if(action==GROW) {
            CHECK(result==NATIVE_ARRAY_OK&&removed&&f->calls==2&&f->list->size==8);
            CHECK(f->seen[1]==(MCObject *)f->added&&f->list->storage->capacity>=9);
            CHECK(f->list->storage->items[0]==(MCObject *)f->query);
            for(int32_t i=1;i<8;++i)CHECK(f->list->storage->items[i]==(MCObject *)f->added);
        }
        if(action==TRUNCATE)CHECK(result==NATIVE_ARRAY_OK&&removed&&f->calls==2&&f->list->size==0);
        if(action==NULL_STORAGE)CHECK(result==NATIVE_ARRAY_EXCEPTION&&!removed&&f->list->size==-1&&f->list->modCount==before+1);
        if(action==BAD_COPY)CHECK(result==NATIVE_ARRAY_EXCEPTION&&!removed&&f->list->size==f->list->storage->capacity+1&&f->list->modCount==before+1);
        CHECK(!MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
    }
}
static void callback_status_and_guards(void) {
    for(int action=THROW_PREFIX;action<=FOREIGN_STORAGE;++action) {
        MCObjectHeap *heap=MCObjectHeap_new(65536);Fixture *f=fixture(heap,(Action)action);
        f->foreign=MCObjectHeap_new(65536);CHECK(f->foreign);
        CHECK(NativeReferenceList_add(f->list,(MCObject *)f->query));
        uint32_t before=f->list->modCount;bool removed=true;
        NativeArrayResult result=remove_query(f,&removed);
        CHECK(result==(action==THROW_PREFIX?NATIVE_ARRAY_EXCEPTION:NATIVE_ARRAY_FAILURE));
        CHECK(removed&&f->list->size==1&&f->list->modCount==before);
        CHECK(MCObjectHeap_failed(heap)==(action!=THROW_PREFIX));
        if(action==THROW_PREFIX||action==FAIL_PREFIX)CHECK(f->list->storage->items[0]==(MCObject *)f->added);
        f->object.heap=heap;MCObjectHeap_free(f->foreign);MCObjectHeap_free(heap);
    }
    MCObjectHeap *heap=MCObjectHeap_new(65536);Fixture *f=fixture(heap,IDENTITY);
    bool removed=true;int32_t size=99;MCObject *sentinel=(MCObject *)f;
    CHECK(NativeReferenceList_sizeSource(NULL,&size)==NATIVE_ARRAY_EXCEPTION&&size==99);
    CHECK(NativeReferenceList_getSource(NULL,0,&sentinel)==NATIVE_ARRAY_EXCEPTION&&sentinel==(MCObject *)f);
    CHECK(NativeReferenceList_removeObjectSource(NULL,NULL,NULL,NULL,&removed)==NATIVE_ARRAY_EXCEPTION&&removed);
    CHECK(NativeReferenceList_add(f->list,(MCObject *)f->query));
    CHECK(NativeReferenceList_getSource(f->list,-1,&sentinel)==NATIVE_ARRAY_EXCEPTION&&sentinel==(MCObject *)f);
    CHECK(NativeReferenceList_getSource(f->list,1,&sentinel)==NATIVE_ARRAY_EXCEPTION&&sentinel==(MCObject *)f);
    CHECK(NativeReferenceList_getSource(f->list,0,&sentinel)==NATIVE_ARRAY_OK&&sentinel==(MCObject *)f->query);
    f->list->modCount=UINT32_MAX;
    CHECK(remove_query(f,&removed)==NATIVE_ARRAY_OK&&removed&&f->list->modCount==0);
    CHECK(!MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
    for(int guard=0;guard<8;++guard) {
        heap=MCObjectHeap_new(65536);f=fixture(heap,IDENTITY);
        MCObjectHeap *foreign=MCObjectHeap_new(65536);Leaf *foreignLeaf=leaf(foreign,1);
        CHECK(NativeReferenceList_add(f->list,(MCObject *)f->query));
        MCObject *query=(MCObject *)f->query,*context=(MCObject *)f;
        if(guard==0)query=(MCObject *)foreignLeaf;
        if(guard==1)context=(MCObject *)foreignLeaf;
        if(guard==2)f->list->storage->items[0]=(MCObject *)foreignLeaf;
        if(guard==3)f->list->storage=(NativeReferenceArray *)f->query;
        if(guard==4)f->list->storage=(NativeReferenceArray *)MCObjectHeap_alloc(heap,sizeof(MCObject),f->list->storage->object.klass);
        if(guard==5)f->list->storage->capacity=INT32_MAX;
        MCObject untracked={heap,&leafClass};
        if(guard==6)query=&untracked;
        if(guard==7)context=&untracked;
        removed=false;
        CHECK(NativeReferenceList_removeObjectSource(f->list,query,&methods,context,&removed)==NATIVE_ARRAY_FAILURE);
        CHECK(!removed&&MCObjectHeap_failed(heap)&&f->calls==0);
        MCObjectHeap_free(foreign);MCObjectHeap_free(heap);
    }
}
#endif
int main(void) {
    clear_during_equals();
#ifndef NATIVE_LIST_REMOVE_PRIOR_RED
    first_equal_and_null();live_reentrant_lists();callback_status_and_guards();
#endif
    printf("native list Source removal: %u checks\n",checks);return 0;
}
