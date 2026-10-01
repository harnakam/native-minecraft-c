#include "util/NativeConcurrentQueue.h"
static bool fail(MCObjectHeap *h){MCObjectHeap_fail(h);return false;}
static void node_trace(MCObject *o,MCObjectVisitor v,void *ctx){
    if(MCObjectHeap_objectSize(o)<sizeof(NativeConcurrentQueueNode)){fail(o->heap);return;}
    NativeConcurrentQueueNode *n=(NativeConcurrentQueueNode *)o;n->item=v(n->item,ctx);n->next=(NativeConcurrentQueueNode *)v((MCObject *)n->next,ctx);
}
static void trace(MCObject *o,MCObjectVisitor v,void *ctx){
    if(MCObjectHeap_objectSize(o)<sizeof(NativeConcurrentQueue)){fail(o->heap);return;}
    NativeConcurrentQueue *q=(NativeConcurrentQueue *)o;q->head=(NativeConcurrentQueueNode *)v((MCObject *)q->head,ctx);
    q->tail=(NativeConcurrentQueueNode *)v((MCObject *)q->tail,ctx);
}
static const MCObjectClass nodeClass={"native.ConcurrentLinkedQueue.Node",MCObjectHeap_plainClone,node_trace,NULL};
static const MCObjectClass queueClass={"native.ConcurrentLinkedQueue",MCObjectHeap_plainClone,trace,NULL};
bool NativeConcurrentQueue_isInstance(const MCObject *o){return o&&o->klass==&queueClass&&MCObjectHeap_objectSize(o)>=sizeof(NativeConcurrentQueue);}
bool NativeConcurrentQueueNode_isInstance(const MCObject *o){return o&&o->klass==&nodeClass&&MCObjectHeap_objectSize(o)>=sizeof(NativeConcurrentQueueNode);}
static bool node_valid(NativeConcurrentQueue *q,NativeConcurrentQueueNode *n){
    return NativeConcurrentQueueNode_isInstance((MCObject *)n)&&n->object.heap==q->object.heap?true:fail(q->object.heap);
}
static bool begin(NativeConcurrentQueue *q,MCObjectRootScope *scope){
    MCObjectHeap *h=q?q->object.heap:NULL;
    if(!NativeConcurrentQueue_isInstance((MCObject *)q)||MCObjectHeap_failed(h)||!MCObjectRootScope_begin(scope,h))return fail(h);
    if(MCObjectRootScope_pin(scope,(MCObject *)q)&&node_valid(q,q->head)&&node_valid(q,q->tail))return true;
    MCObjectRootScope_end(scope);return fail(h);
}
NativeConcurrentQueue *NativeConcurrentQueue_new(MCObjectHeap *h){
    NativeConcurrentQueue *q=(NativeConcurrentQueue *)MCObjectHeap_alloc(h,sizeof(*q),&queueClass);if(!q)return NULL;
    NativeConcurrentQueueNode *dummy=(NativeConcurrentQueueNode *)MCObjectHeap_alloc(h,sizeof(*dummy),&nodeClass);
    if(!dummy)return NULL;
    q->head=q->tail=dummy;return q;
}
static void update_head(NativeConcurrentQueue *q,NativeConcurrentQueueNode *old,NativeConcurrentQueueNode *next){
    if(old!=next){q->head=next;old->next=old;MCObjectHeap_touch(q->object.heap);}
}
bool NativeConcurrentQueue_offer(NativeConcurrentQueue *q,MCObject *item){
    MCObjectRootScope scope={0};if(!begin(q,&scope))return false;
    bool ok=false;if(!item||!MCObjectRootScope_pin(&scope,item)){fail(q->object.heap);goto done;}
    NativeConcurrentQueueNode *node=(NativeConcurrentQueueNode *)MCObjectHeap_alloc(q->object.heap,sizeof(*node),&nodeClass);
    if(!node)goto done;
    node->item=item;
    NativeConcurrentQueueNode *tail=q->tail,*p=tail;
    size_t steps=0,limit=MCObjectHeap_liveObjects(q->object.heap);
    while(steps++<=limit){
        if(!node_valid(q,p))goto done;
        NativeConcurrentQueueNode *next=p->next;
        if(!next){p->next=node;if(p!=tail)q->tail=node;MCObjectHeap_touch(q->object.heap);ok=true;goto done;}
        if(!node_valid(q,next))goto done;
        if(p==next)p=tail!=q->tail?(tail=q->tail):q->head;
        else p=p!=tail&&tail!=q->tail?(tail=q->tail):next;
    }
    fail(q->object.heap);
done:MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(q->object.heap);
}
static MCObject *read_first(NativeConcurrentQueue *q,bool remove){
    MCObjectRootScope scope={0};if(!begin(q,&scope))return NULL;
    MCObject *out=NULL;NativeConcurrentQueueNode *head=q->head,*p=head;
    size_t steps=0,limit=MCObjectHeap_liveObjects(q->object.heap);
    while(steps++<=limit){
        if(!node_valid(q,p))goto done;
        MCObject *item=p->item;
        if(item&&item->heap!=q->object.heap){fail(q->object.heap);goto done;}
        if(item){
            if(remove){p->item=NULL;
                if(p!=head){NativeConcurrentQueueNode *next=p->next;
                    if(next&&!node_valid(q,next))goto done;
                    update_head(q,head,next?next:p);
                }
                MCObjectHeap_touch(q->object.heap);
            }else update_head(q,head,p);
            out=item;goto done;
        }
        NativeConcurrentQueueNode *next=p->next;
        if(!next){update_head(q,head,p);goto done;}
        if(!node_valid(q,next))goto done;
        if(p==next){head=q->head;p=head;}else p=next;
    }
    fail(q->object.heap);
done:MCObjectRootScope_end(&scope);return MCObjectHeap_failed(q->object.heap)?NULL:out;
}
MCObject *NativeConcurrentQueue_poll(NativeConcurrentQueue *q){return read_first(q,true);}
MCObject *NativeConcurrentQueue_peek(NativeConcurrentQueue *q){return read_first(q,false);}
bool NativeConcurrentQueue_isEmpty(NativeConcurrentQueue *q){
    MCObject *item=NativeConcurrentQueue_peek(q);return NativeConcurrentQueue_isInstance((MCObject *)q)&&!MCObjectHeap_failed(q->object.heap)&&!item;
}
