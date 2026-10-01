#ifndef C919_MC_GAMEPLAY_H
#define C919_MC_GAMEPLAY_H
#include "util/MCObjectHeap.h"
#include "util/transfer.h"

#define MC_GAMEPLAY_MAX_ITEMS 1024u
/* Native owner of the translated object graph. Actor/entity objects retain
   their own source class and trace their InventoryPlayer/Container/ItemStack
   fields. These arrays store references, never another owning slot value. */
typedef struct MCGameplayObjects {
    MCObject object;
    /* Native transient commit fence. Only MCGameplay_commit advances it in an
       adopted graph; it is not an alias ID or a serialized Minecraft field. */
    uint64_t commitSerial;
    MCObject *world;
    MCObject *players[MC_TRANSFER_MAX_PLAYERS];
    char uuids[MC_TRANSFER_MAX_PLAYERS][37];
    MCObject *items[MC_GAMEPLAY_MAX_ITEMS];
    size_t itemCount;
} MCGameplayObjects;
/* Initialize native handles with {0}. They own the heap and must not be copied. */
typedef struct {
    MCObjectHeap *heap;
    MCObjectRoot root;
    bool fatal,snapshot;
} MCGameplay;
typedef struct {
    MCGameplay *parent;
    MCGameplay working;
    bool active;
} MCGameplayTransaction;
bool MCGameplay_init(MCGameplay *,size_t byteBudget);
bool MCGameplay_free(MCGameplay *);
/* Returned objects are borrowed. Callers hold a RootScope while invoking
   translated methods or touching fields, then release it before snapshotting. */
MCGameplayObjects *MCGameplay_get(MCGameplay *);
bool MCGameplay_setWorld(MCGameplay *,MCObject *world);
bool MCGameplay_setPlayer(MCGameplay *,size_t index,const char *uuid,MCObject *player);
bool MCGameplay_addItem(MCGameplay *,MCObject *item);
bool MCGameplay_removeItem(MCGameplay *,size_t index);
bool MCGameplay_begin(MCGameplay *,MCGameplayTransaction *);
/* Working handles cannot start another durable transaction. Commit operates
   only against the authoritative owner; nested disk commits are invalid. */
bool MCGameplay_abort(MCGameplayTransaction *);

/* Encoders serialize the working graph directly. They may allocate within that
   heap, but cannot modify the parent graph or retain borrowed references. Native
   map/world snapshots belong to context and must obey the same commit point.
   Players are encoded together even when only another actor picked up an alias.
   Required encoders have no default or empty-success implementation. */
typedef struct {
    bool (*player)(const MCGameplayObjects *,size_t index,mc_nbt *,void *context);
    bool (*items)(const MCGameplayObjects *,mc_nbt *,void *context);
    bool (*maps)(const MCGameplayObjects *,mc_nbt *,void *context); /* Optional. */
} MCGameplayEncoders;
typedef enum {
    MC_GAMEPLAY_NOT_COMMITTED,
    MC_GAMEPLAY_COMMITTED,
    MC_GAMEPLAY_COMMITTED_NEEDS_RECOVERY
} MCGameplayCommit;
/* Synchronous single-writer operation. Any durable commitment adopts the full
   working heap, including aliases in all registered roots. A checkpoint error
   then marks the owner fatal; restart recovery must precede further mutations.
   Release every working RootScope before commit/abort/free. An active working
   borrow refuses the operation and retains the transaction; otherwise commit
   consumes it on every result. */
MCGameplayCommit MCGameplay_commit(MCGameplayTransaction *,const char *savePath,
    const MCGameplayEncoders *,void *context,char *error,size_t errorSize);
#endif
