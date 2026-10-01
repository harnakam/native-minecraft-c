#include "util/NativeConcurrentQueue.h"
#include <stdio.h>
#include <stdlib.h>
static int checks;
#define CHECK(test) do{++checks;if(!(test)){fprintf(stderr,"Queue check%d line%d: %s\n",checks,__LINE__,#test);exit(1);}}while(0)
static const MCObjectClass tokenClass={"fixture.QueueValue",MCObjectHeap_plainClone,NULL,NULL};
static void values_and_nodes(void){
    MCObjectHeap *h=MCObjectHeap_new(1000000);CHECK(h);NativeConcurrentQueue *q=NativeConcurrentQueue_new(h);CHECK(q);
    CHECK(q->head==q->tail&&!q->head->item&&!q->head->next);NativeConcurrentQueueNode *old=q->head;
    CHECK(NativeConcurrentQueue_isEmpty(q));CHECK(!NativeConcurrentQueue_poll(q)&&!MCObjectHeap_failed(h));
    MCObject *a=MCObjectHeap_alloc(h,sizeof(MCObject),&tokenClass),*b=MCObjectHeap_alloc(h,sizeof(MCObject),&tokenClass);CHECK(a&&b);
    CHECK(NativeConcurrentQueue_offer(q,a));CHECK(q->tail==old&&old->next&&old->next->item==a);
    CHECK(NativeConcurrentQueue_offer(q,b));CHECK(q->tail->item==b&&q->tail!=old);
    CHECK(NativeConcurrentQueue_peek(q)==a);CHECK(q->head!=old&&old->next==old);
    CHECK(old->next==old&&q->head->item==a);NativeConcurrentQueueNode *first=q->head;
    CHECK(NativeConcurrentQueue_poll(q)==a);CHECK(!first->item&&q->head==first);
    CHECK(NativeConcurrentQueue_poll(q)==b);CHECK(first->next==first&&q->head==q->tail&&!q->head->item);
    CHECK(NativeConcurrentQueue_isEmpty(q));CHECK(NativeConcurrentQueue_offer(q,a));CHECK(NativeConcurrentQueue_poll(q)==a);
    CHECK(!NativeConcurrentQueue_poll(q)&&!MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
static void graph(void){
    MCObjectHeap *h=MCObjectHeap_new(1000000);CHECK(h);NativeConcurrentQueue *q=NativeConcurrentQueue_new(h);CHECK(q);
    MCObject *value=MCObjectHeap_alloc(h,sizeof(MCObject),&tokenClass);CHECK(value);
    CHECK(NativeConcurrentQueue_offer(q,value));CHECK(NativeConcurrentQueue_offer(q,value));
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)q));CHECK(MCObjectHeap_collect(h));
    MCObjectHeap *copy=MCObjectHeap_clone(h);CHECK(copy);MCObjectRoot cr={0};CHECK(MCObjectRoot_rebind(&cr,copy,&root));
    NativeConcurrentQueue *cq=(NativeConcurrentQueue *)MCObjectRoot_get(&cr);CHECK(cq!=q);
    MCObject *v=NativeConcurrentQueue_peek(cq);CHECK(v&&v!=value);CHECK(NativeConcurrentQueue_poll(cq)==v);
    CHECK(NativeConcurrentQueue_poll(cq)==v);CHECK(NativeConcurrentQueue_isEmpty(cq));
    CHECK(MCObjectHeap_adopt(h,copy));MCObjectHeap_free(copy);q=(NativeConcurrentQueue *)MCObjectRoot_get(&root);
    CHECK(NativeConcurrentQueue_isEmpty(q));CHECK(MCObjectHeap_collect(h));MCObjectRoot_drop(&root);
    CHECK(MCObjectHeap_collect(h)&&MCObjectHeap_liveObjects(h)==0);MCObjectHeap_free(h);
}
static void rejected(void){
    MCObjectHeap *h=MCObjectHeap_new(1000000);CHECK(h);NativeConcurrentQueue *q=NativeConcurrentQueue_new(h);CHECK(q);
    CHECK(!NativeConcurrentQueue_offer(q,NULL)&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
    h=MCObjectHeap_new(1000000);MCObjectHeap *other=MCObjectHeap_new(1000000);CHECK(h&&other);q=NativeConcurrentQueue_new(h);CHECK(q);
    MCObject *foreign=MCObjectHeap_alloc(other,sizeof(MCObject),&tokenClass);CHECK(foreign);
    CHECK(!NativeConcurrentQueue_offer(q,foreign)&&MCObjectHeap_failed(h)&&!MCObjectHeap_failed(other));
    MCObjectHeap_free(h);MCObjectHeap_free(other);
}
int main(void){values_and_nodes();graph();rejected();printf("Native concurrent queue dependency: %d checks\n",checks);return 0;}
