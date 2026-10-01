#ifndef C919_NATIVE_CONCURRENT_QUEUE_H
#define C919_NATIVE_CONCURRENT_QUEUE_H
#include "util/MCObjectHeap.h"
/* Managed single-writer dependency for the Source Chunk's actual linked
   position queue. Head/tail and dummy/self-linked node identity follow the
   reached JDK8 queue operations. This does not implement Java CAS, concurrent
   iterators or the complete ConcurrentLinkedQueue class. */
typedef struct NativeConcurrentQueueNode NativeConcurrentQueueNode;
struct NativeConcurrentQueueNode {MCObject object;MCObject *item;NativeConcurrentQueueNode *next;};
typedef struct NativeConcurrentQueue {MCObject object;NativeConcurrentQueueNode *head,*tail;} NativeConcurrentQueue;
NativeConcurrentQueue *NativeConcurrentQueue_new(MCObjectHeap *);
bool NativeConcurrentQueue_isInstance(const MCObject *);
bool NativeConcurrentQueueNode_isInstance(const MCObject *);
bool NativeConcurrentQueue_offer(NativeConcurrentQueue *,MCObject *);
MCObject *NativeConcurrentQueue_poll(NativeConcurrentQueue *);
MCObject *NativeConcurrentQueue_peek(NativeConcurrentQueue *);
bool NativeConcurrentQueue_isEmpty(NativeConcurrentQueue *);
#endif
