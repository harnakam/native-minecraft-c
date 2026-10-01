#include "util/MCObjectHeap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node { MCObject object; struct Node *left, *right; int value; } Node;
static unsigned checks, destroyed;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"heap check %u failed at %d: %s\n",checks,__LINE__,#x); exit(1); } } while (0)
static void trace(MCObject *object, MCObjectVisitor visitor, void *context) {
    Node *node=(Node *)object;
    node->left=(Node *)visitor((MCObject *)node->left,context);
    node->right=(Node *)visitor((MCObject *)node->right,context);
}
static void destroy(MCObject *object) { (void)object; ++destroyed; }
static const MCObjectClass node_class={"test.Node",MCObjectHeap_plainClone,trace,destroy};
static Node *node(MCObjectHeap *heap,int value) {
    Node *result=(Node *)MCObjectHeap_alloc(heap,sizeof(Node),&node_class);
    CHECK(result!=NULL); result->value=value; return result;
}
static void aliases_and_cycles(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024); CHECK(heap!=NULL);
    Node *a=node(heap,1), *b=node(heap,2); a->left=b; a->right=b; b->left=a;
    MCObjectRoot first={0}, second={0}, empty={0};
    CHECK(MCObjectRoot_init(&first,heap,(MCObject *)a));
    CHECK(MCObjectRoot_init(&second,heap,(MCObject *)b));
    CHECK(MCObjectRoot_init(&empty,heap,NULL));
    (void)node(heap,99); CHECK(MCObjectHeap_collect(heap)); CHECK(MCObjectHeap_liveObjects(heap)==2);
    MCObjectHeap *working=MCObjectHeap_clone(heap); CHECK(working!=NULL);
    MCObjectRoot working_first={0}, working_second={0}, working_empty={0};
    CHECK(MCObjectRoot_rebind(&working_first,working,&first));
    CHECK(MCObjectRoot_rebind(&working_second,working,&second));
    CHECK(MCObjectRoot_rebind(&working_empty,working,&empty));
    Node *copy=(Node *)MCObjectRoot_get(&working_first), *other=(Node *)MCObjectRoot_get(&working_second);
    CHECK(copy!=a && other!=b && copy->left==other && copy->right==other && other->left==copy);
    CHECK(MCObjectHeap_identityHashCode((MCObject *)copy)==MCObjectHeap_identityHashCode((MCObject *)a));
    CHECK(MCObjectHeap_identityHashCode((MCObject *)other)==MCObjectHeap_identityHashCode((MCObject *)b));
    CHECK(MCObjectRoot_get(&working_empty)==NULL);
    copy->value=7; MCObjectHeap_touch(working); CHECK(a->value==1);
    CHECK(MCObjectHeap_adopt(heap,working));
    CHECK(MCObjectHeap_liveObjects(working)==0);
    CHECK(((Node *)MCObjectRoot_get(&first))->value==7);
    CHECK(MCObjectRoot_get(&second)==(MCObject *)other && other->object.heap==heap);
    CHECK(MCObjectRoot_rebind(&working_first,heap,&working_first));
    CHECK(MCObjectRoot_get(&working_first)==(MCObject *)copy);
    MCObjectHeap_free(working);
    MCObjectRoot_drop(&first); MCObjectRoot_drop(&second); MCObjectRoot_drop(&empty);
    CHECK(MCObjectHeap_collect(heap)); CHECK(MCObjectHeap_liveObjects(heap)==0);
    MCObjectHeap_free(heap);
}
static void scope_and_stale_snapshot(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024), *foreign=MCObjectHeap_new(1024*1024);
    CHECK(heap && foreign); MCObjectRoot root={0}; Node *a=node(heap,1);
    CHECK(MCObjectRoot_init(&root,heap,(MCObject *)a));
    MCObjectHeap *snapshot=MCObjectHeap_clone(heap); CHECK(snapshot!=NULL);
    MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,heap));
    CHECK(MCObjectRootScope_pin(&scope,(MCObject *)a));
    CHECK(!MCObjectHeap_collect(heap)); CHECK(!MCObjectHeap_adopt(heap,snapshot));
    a->value=3; MCObjectRootScope_end(&scope);
    CHECK(!MCObjectHeap_adopt(heap,snapshot)); CHECK(a->value==3);
    CHECK(!MCObjectHeap_failed(heap)); MCObjectHeap_free(snapshot);
    snapshot=MCObjectHeap_clone(heap); CHECK(snapshot!=NULL);
    CHECK(MCObjectRootScope_begin(&scope,snapshot)); CHECK(!MCObjectHeap_adopt(heap,snapshot));
    MCObjectRootScope_end(&scope); CHECK(MCObjectHeap_adopt(heap,snapshot)); MCObjectHeap_free(snapshot);
    MCObjectRoot outsider={0}; CHECK(MCObjectRoot_init(&outsider,foreign,(MCObject *)node(foreign,4)));
    CHECK(!MCObjectRoot_set(&root,MCObjectRoot_get(&outsider))); CHECK(MCObjectHeap_failed(heap));
    MCObjectHeap_free(foreign); MCObjectHeap_free(heap);
}
static void bounds_and_rollback(void) {
    MCObjectHeap *small=MCObjectHeap_new(1); CHECK(small!=NULL);
    CHECK(MCObjectHeap_alloc(small,sizeof(Node),&node_class)==NULL); CHECK(MCObjectHeap_failed(small));
    CHECK(MCObjectHeap_liveObjects(small)==0); MCObjectHeap_free(small);
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024); CHECK(heap!=NULL);
    MCObjectRoot root={0}; Node *a=node(heap,12); CHECK(MCObjectRoot_init(&root,heap,(MCObject *)a));
    MCObjectHeap *snapshot=MCObjectHeap_clone(heap); CHECK(snapshot!=NULL);
    MCObjectHeap_fail(snapshot); CHECK(!MCObjectHeap_adopt(heap,snapshot));
    CHECK(MCObjectRoot_get(&root)==(MCObject *)a && a->value==12); MCObjectHeap_free(snapshot);
    MCObjectRoot missing=root; MCObjectRoot_drop(&root); CHECK(MCObjectRoot_get(&missing)==NULL);
    CHECK(MCObjectHeap_collect(heap)); CHECK(MCObjectHeap_liveObjects(heap)==0); MCObjectHeap_free(heap);
}
static void branch_and_dropped_handles(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024); CHECK(heap!=NULL);
    MCObjectRoot root={0}; CHECK(MCObjectRoot_init(&root,heap,(MCObject *)node(heap,1)));
    MCObjectHeap *first=MCObjectHeap_clone(heap), *second=MCObjectHeap_clone(heap); CHECK(first && second);
    MCObjectRoot inherited_first={0}, inherited_second={0}, a={0}, b={0}, rebound={0};
    CHECK(MCObjectRoot_rebind(&inherited_first,first,&root));
    CHECK(MCObjectRoot_rebind(&inherited_second,second,&root));
    CHECK(MCObjectRoot_init(&a,first,(MCObject *)node(first,10)));
    CHECK(MCObjectRoot_init(&b,second,(MCObject *)node(second,20)));
    /* Each branch's new owner is absent in its sibling, even if their local
       allocation order is identical. Inherited owner identity is shared. */
    CHECK(!MCObjectRoot_rebind(&rebound,second,&a)); CHECK(MCObjectRoot_get(&rebound)==NULL);
    CHECK(!MCObjectRoot_rebind(&rebound,first,&b)); CHECK(MCObjectRoot_get(&rebound)==NULL);
    CHECK(MCObjectRoot_rebind(&rebound,second,&inherited_first));
    CHECK(((Node *)MCObjectRoot_get(&rebound))->value==1);
    MCObjectRoot stale=root; MCObjectRoot_drop(&root); CHECK(MCObjectRoot_get(&stale)==NULL);
    MCObjectRoot missing={0}; CHECK(!MCObjectRoot_rebind(&missing,first,&stale)); CHECK(MCObjectRoot_get(&missing)==NULL);
    MCObjectHeap_free(first); MCObjectHeap_free(second); MCObjectHeap_free(heap);
    /* Adoption must still permit live working-owner handles to be rebound
       before the empty working heap is released, including newly added roots. */
    heap=MCObjectHeap_new(1024*1024); CHECK(heap!=NULL);
    root=(MCObjectRoot){0}; CHECK(MCObjectRoot_init(&root,heap,(MCObject *)node(heap,2)));
    first=MCObjectHeap_clone(heap); CHECK(first!=NULL);
    a=(MCObjectRoot){0}; b=(MCObjectRoot){0};
    CHECK(MCObjectRoot_rebind(&a,first,&root)); CHECK(MCObjectRoot_init(&b,first,(MCObject *)node(first,30)));
    CHECK(MCObjectHeap_adopt(heap,first));
    CHECK(MCObjectRoot_rebind(&a,heap,&a)); CHECK(MCObjectRoot_rebind(&b,heap,&b));
    CHECK(((Node *)MCObjectRoot_get(&a))->value==2 && ((Node *)MCObjectRoot_get(&b))->value==30);
    MCObjectHeap_free(first); CHECK(((Node *)MCObjectRoot_get(&b))->value==30);
    MCObjectHeap_free(heap);
}
typedef struct OwnedNode {
    MCObject object;
    struct OwnedNode *left,*right;
    unsigned char *native;
    int value;
} OwnedNode;
static unsigned native_live;
static int fail_value,fail_after_duplicate;
static void owned_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    OwnedNode *node=(OwnedNode *)object;
    node->left=(OwnedNode *)visitor((MCObject *)node->left,context);
    node->right=(OwnedNode *)visitor((MCObject *)node->right,context);
}
static void owned_destroy(MCObject *object) {
    OwnedNode *node=(OwnedNode *)object;
    /* Managed children belong to the heap, including unmapped source edges
       in a failed clone. This destructor owns only its native buffer. */
    if (node->native) { CHECK(native_live>0); --native_live; free(node->native); }
}
static MCObject *owned_clone(MCObjectHeap *heap,const MCObject *object) {
    OwnedNode *copy=(OwnedNode *)MCObjectHeap_plainClone(heap,object);
    if (!copy) return NULL;
    copy->native=NULL;
    if (copy->value==fail_value && !fail_after_duplicate) { MCObjectHeap_fail(heap); return NULL; }
    copy->native=malloc(8);
    if (!copy->native) { MCObjectHeap_fail(heap); return NULL; }
    ++native_live; memcpy(copy->native,((const OwnedNode *)object)->native,8);
    if (copy->value==fail_value && fail_after_duplicate) { MCObjectHeap_fail(heap); return NULL; }
    return (MCObject *)copy;
}
static const MCObjectClass owned_class={"test.OwnedNode",owned_clone,owned_trace,owned_destroy};
static OwnedNode *owned_node(MCObjectHeap *heap,int value) {
    OwnedNode *result=(OwnedNode *)MCObjectHeap_alloc(heap,sizeof(*result),&owned_class); CHECK(result!=NULL);
    result->native=malloc(8); CHECK(result->native!=NULL); ++native_live;
    result->value=value; memset(result->native,value,8); return result;
}
static void native_buffer_failure_rollback(void) {
    CHECK(native_live==0); MCObjectHeap *heap=MCObjectHeap_new(1024*1024); CHECK(heap!=NULL);
    OwnedNode *a=owned_node(heap,1),*b=owned_node(heap,2),*c=owned_node(heap,3);
    a->left=b; a->right=c; b->left=a; c->right=b;
    MCObjectRoot first={0},second={0};
    CHECK(MCObjectRoot_init(&first,heap,(MCObject *)a)); CHECK(MCObjectRoot_init(&second,heap,(MCObject *)b));
    for (int stage=0;stage<2;stage++) for (int value=1;value<=3;value++) {
        fail_after_duplicate=stage; fail_value=value;
        CHECK(MCObjectHeap_clone(heap)==NULL); CHECK(!MCObjectHeap_failed(heap));
        CHECK(MCObjectHeap_liveObjects(heap)==3 && native_live==3);
        CHECK(MCObjectRoot_get(&first)==(MCObject *)a && MCObjectRoot_get(&second)==(MCObject *)b);
        CHECK(a->left==b && a->right==c && b->left==a && c->right==b);
        CHECK(a->native[0]==1 && b->native[0]==2 && c->native[0]==3);
    }
    fail_value=0; MCObjectHeap *working=MCObjectHeap_clone(heap); CHECK(working!=NULL); CHECK(native_live==6);
    MCObjectRoot copied={0}; CHECK(MCObjectRoot_rebind(&copied,working,&first));
    OwnedNode *copy=(OwnedNode *)MCObjectRoot_get(&copied);
    CHECK(copy!=a && copy->native!=a->native && copy->left->native!=b->native && copy->right->native!=c->native);
    CHECK(MCObjectHeap_identityHashCode((MCObject *)copy)==MCObjectHeap_identityHashCode((MCObject *)a));
    CHECK(copy->left->left==copy && copy->right->right==copy->left && copy->native[0]==1);
    MCObjectHeap_free(working); CHECK(native_live==3);
    MCObjectRoot_drop(&first); MCObjectRoot_drop(&second);
    CHECK(MCObjectHeap_collect(heap)); CHECK(MCObjectHeap_liveObjects(heap)==0 && native_live==0);
    MCObjectHeap_free(heap); CHECK(native_live==0);
}
static void deep_graph(void) {
    const unsigned depth=100000;
    MCObjectHeap *heap=MCObjectHeap_new(32*1024*1024); CHECK(heap!=NULL);
    Node *head=NULL;
    for (unsigned i=0;i<depth;i++) { Node *next=node(heap,(int)i); next->left=head; head=next; }
    head->right=head;
    MCObjectRoot root={0}; CHECK(MCObjectRoot_init(&root,heap,(MCObject *)head));
    CHECK(MCObjectHeap_collect(heap)); CHECK(MCObjectHeap_liveObjects(heap)==depth);
    MCObjectHeap *snapshot=MCObjectHeap_clone(heap); CHECK(snapshot!=NULL);
    MCObjectRoot copied={0}; CHECK(MCObjectRoot_rebind(&copied,snapshot,&root));
    Node *walk=(Node *)MCObjectRoot_get(&copied); CHECK(walk->right==walk);
    for (unsigned i=depth;i>0;i--) { CHECK(walk && walk->value==(int)(i-1)); walk=walk->left; }
    CHECK(walk==NULL); CHECK(MCObjectHeap_adopt(heap,snapshot)); MCObjectHeap_free(snapshot);
    MCObjectRoot_drop(&root); CHECK(MCObjectHeap_collect(heap)); CHECK(MCObjectHeap_liveObjects(heap)==0);
    MCObjectHeap_free(heap);
}
int main(void) {
    aliases_and_cycles(); scope_and_stale_snapshot(); bounds_and_rollback(); branch_and_dropped_handles(); native_buffer_failure_rollback(); deep_graph();
    CHECK(destroyed>200000); printf("object heap: %u checks passed\n",checks); return 0;
}
