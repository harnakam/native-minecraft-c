#include "util/MCGameplay.h"
#include "util/MCGameplayWorld.h"
#include <stdio.h>
#include <string.h>
static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    MCGameplayObjects *objects=(MCGameplayObjects *)object;
    objects->world=visitor(objects->world,context);
    for (size_t i=0;i<MC_TRANSFER_MAX_PLAYERS;i++) objects->players[i]=visitor(objects->players[i],context);
    for (size_t i=0;i<objects->itemCount;i++) objects->items[i]=visitor(objects->items[i],context);
}
static const MCObjectClass objects_class={"C919.native.GameplayOwners",MCObjectHeap_plainClone,trace,NULL};
static void message(char *error,size_t size,const char *text) { if (error && size) snprintf(error,size,"%s",text); }
static bool uuid_valid(const char *uuid) {
    if (!uuid || strlen(uuid)!=36) return false;
    for (size_t i=0;i<36;i++) {
        char c=uuid[i];
        if (i==8 || i==13 || i==18 || i==23) { if (c!='-') return false; }
        else if (!((c>='0'&&c<='9')||(c>='a'&&c<='f'))) return false;
    }
    return true;
}
bool MCGameplay_init(MCGameplay *game,size_t budget) {
    if (!game || game->heap || game->root.heap) return false;
    MCObjectHeap *heap=MCObjectHeap_new(budget);
    if (!heap) return false;
    MCGameplayObjects *objects=(MCGameplayObjects *)MCObjectHeap_alloc(heap,sizeof(*objects),&objects_class);
    MCObjectRoot root={0};
    if (!objects || !MCObjectRoot_init(&root,heap,(MCObject *)objects)) { MCObjectHeap_free(heap); return false; }
    *game=(MCGameplay){heap,root,false,false}; return true;
}
bool MCGameplay_free(MCGameplay *game) {
    if (!game || MCObjectHeap_hasBorrowers(game->heap)) return false;
    MCObjectHeap_free(game->heap); *game=(MCGameplay){0}; return true;
}
MCGameplayObjects *MCGameplay_get(MCGameplay *game) {
    return game && !game->fatal ? (MCGameplayObjects *)MCObjectRoot_get(&game->root) : NULL;
}
static bool accepts(MCGameplay *game,const MCObject *object) {
    return game && !game->fatal && !MCObjectHeap_failed(game->heap) && (!object || object->heap==game->heap);
}
bool MCGameplay_setWorld(MCGameplay *game,MCObject *world) {
    MCGameplayObjects *objects=MCGameplay_get(game);
    if (!objects || !accepts(game,world)) return false;
    objects->world=world; MCObjectHeap_touch(game->heap); return true;
}
bool MCGameplay_setPlayer(MCGameplay *game,size_t index,const char *uuid,MCObject *player) {
    MCGameplayObjects *objects=MCGameplay_get(game);
    if (!objects || index>=MC_TRANSFER_MAX_PLAYERS || !accepts(game,player) ||
        (player ? !uuid_valid(uuid) : uuid!=NULL)) return false;
    if (player) for (size_t i=0;i<MC_TRANSFER_MAX_PLAYERS;i++)
        if (i!=index && objects->players[i] && (!strcmp(objects->uuids[i],uuid) || objects->players[i]==player)) return false;
    char copiedUuid[37]={0};
    if (player) memcpy(copiedUuid,uuid,sizeof(copiedUuid));
    objects->players[index]=player;
    memset(objects->uuids[index],0,sizeof(objects->uuids[index]));
    if (player) memcpy(objects->uuids[index],copiedUuid,sizeof(copiedUuid));
    MCObjectHeap_touch(game->heap); return true;
}
bool MCGameplay_addItem(MCGameplay *game,MCObject *item) {
    MCGameplayObjects *objects=MCGameplay_get(game);
    if (!objects || !item || !accepts(game,item) || objects->itemCount>=MC_GAMEPLAY_MAX_ITEMS) return false;
    for (size_t i=0;i<objects->itemCount;i++) if (objects->items[i]==item) return false;
    objects->items[objects->itemCount++]=item; MCObjectHeap_touch(game->heap); return true;
}
bool MCGameplay_removeItem(MCGameplay *game,size_t index) {
    MCGameplayObjects *objects=MCGameplay_get(game);
    if (!objects || !accepts(game,NULL) || index>=objects->itemCount) return false;
    memmove(objects->items+index,objects->items+index+1,(objects->itemCount-index-1)*sizeof(*objects->items));
    objects->items[--objects->itemCount]=NULL; MCObjectHeap_touch(game->heap); return true;
}
bool MCGameplay_begin(MCGameplay *game,MCGameplayTransaction *transaction) {
    if (!game || game->snapshot || !transaction || transaction->active || transaction->working.heap || !accepts(game,NULL)) return false;
    MCObjectHeap *copy=MCObjectHeap_clone(game->heap);
    if (!copy) return false;
    MCObjectRoot root={0};
    if (!MCObjectRoot_rebind(&root,copy,&game->root)) { MCObjectHeap_free(copy); return false; }
    *transaction=(MCGameplayTransaction){game,{copy,root,false,true},true}; return true;
}
bool MCGameplay_abort(MCGameplayTransaction *transaction) {
    if (!transaction || !MCGameplay_free(&transaction->working)) return false;
    *transaction=(MCGameplayTransaction){0}; return true;
}
static bool remote_world(MCGameplayObjects *objects) {
    return objects && MCGameplayWorld_isInstance(objects->world) &&
        ((MCGameplayWorld *)objects->world)->owners == objects &&
        ((MCGameplayWorld *)objects->world)->remote;
}
bool MCGameplay_acceptClientFrame(MCGameplayTransaction *transaction,
    MCGameplayClientValidator validate, void *context, char *error, size_t size) {
    if (transaction && MCObjectHeap_hasBorrowers(transaction->working.heap)) {
        message(error,size,"Release working gameplay pointers before accepting a client frame");
        return false;
    }
    if (!transaction || !transaction->active || !transaction->parent || !validate) {
        message(error,size,"Client frame requires an active transaction and validator");
        MCGameplay_abort(transaction); return false;
    }
    MCGameplay *parent=transaction->parent,*working=&transaction->working;
    MCGameplayObjects *previous=MCGameplay_get(parent),*objects=MCGameplay_get(working);
    bool ok=!parent->fatal && !parent->snapshot && !working->fatal && working->snapshot &&
        MCObjectHeap_canAdopt(parent->heap,working->heap) && remote_world(previous) &&
        remote_world(objects) && previous->commitSerial!=UINT64_MAX &&
        objects->commitSerial==previous->commitSerial &&
        objects->durableSerial==previous->durableSerial;
    MCObjectRootScope scope={0}; MCObjectReadScope parentScope={0};
    if (ok) ok=MCObjectReadScope_begin(&parentScope,parent->heap);
    if (ok) ok=MCObjectRootScope_begin(&scope,working->heap);
    if (ok) ok=validate(objects,context);
    ok=ok && !working->fatal && !MCObjectHeap_failed(working->heap) &&
        remote_world(objects) && objects->commitSerial==previous->commitSerial &&
        objects->durableSerial==previous->durableSerial;
    if (ok) { ++objects->commitSerial; MCObjectHeap_touch(working->heap); }
    MCObjectRootScope_end(&scope);
    MCObjectReadScope_end(&parentScope);
    ok=ok && !parent->fatal && !working->fatal &&
        MCObjectHeap_canAdopt(parent->heap,working->heap);
    if (ok) ok=MCObjectHeap_adopt(parent->heap,working->heap);
    if (!ok) message(error,size,"Client frame validation failed or its object graph is stale");
    MCGameplay_abort(transaction);
    return ok;
}
MCGameplayCommit MCGameplay_commit(MCGameplayTransaction *transaction,const char *path,
    const MCGameplayEncoders *encoders,void *context,char *error,size_t size) {
    if (transaction && MCObjectHeap_hasBorrowers(transaction->working.heap)) {
        message(error,size,"Release working gameplay pointers before committing");
        return MC_GAMEPLAY_NOT_COMMITTED;
    }
    if (!transaction || !transaction->active || !transaction->parent || !path || !encoders ||
        !encoders->player || !encoders->items) {
        message(error,size,"Gameplay commit requires an active transaction and encoders");
        MCGameplay_abort(transaction); return MC_GAMEPLAY_NOT_COMMITTED;
    }
    MCGameplay *parent=transaction->parent,*working=&transaction->working;
    if (parent->fatal || parent->snapshot || working->fatal || !working->snapshot ||
        !MCObjectHeap_canAdopt(parent->heap,working->heap)) {
        message(error,size,"Gameplay snapshot is stale or has live borrowed pointers");
        MCGameplay_abort(transaction); return MC_GAMEPLAY_NOT_COMMITTED;
    }
    mc_nbt players[MC_TRANSFER_MAX_PLAYERS]={{0}},items={0},maps={0};
    mc_transfer_player group[MC_TRANSFER_MAX_PLAYERS]; size_t count=0;
    MCObjectRootScope scope={0}; MCObjectReadScope parentScope={0};
    bool encoded=MCObjectReadScope_begin(&parentScope,parent->heap) &&
        MCObjectRootScope_begin(&scope,working->heap);
    MCGameplayObjects *objects=MCGameplay_get(working);
    MCGameplayObjects *previous=MCGameplay_get(parent);
    encoded=encoded && objects!=NULL && previous!=NULL &&
        previous->commitSerial!=UINT64_MAX && objects->commitSerial==previous->commitSerial &&
        objects->durableSerial==previous->durableSerial;
    for (size_t i=0;i<MC_TRANSFER_MAX_PLAYERS && encoded;i++) if (objects->players[i]) {
        encoded=encoders->player(objects,i,&players[count],context);
        group[count]=(mc_transfer_player){objects->uuids[i],&players[count]}; ++count;
    }
    if (encoded) encoded=encoders->items(objects,&items,context);
    if (encoded && encoders->maps) encoded=encoders->maps(objects,&maps,context);
    encoded=encoded && !working->fatal && !MCObjectHeap_failed(working->heap) &&
        objects->commitSerial==previous->commitSerial &&
        objects->durableSerial==previous->durableSerial;
    if (encoded) {
        ++objects->commitSerial; objects->durableSerial=objects->commitSerial;
        MCObjectHeap_touch(working->heap);
    }
    MCObjectRootScope_end(&scope);
    MCObjectReadScope_end(&parentScope);
    bool committed=false;
    bool ok=false;
    if (!encoded) message(error,size,"Could not encode the complete gameplay object graph");
    else if (parent->fatal || working->fatal || !MCObjectHeap_canAdopt(parent->heap,working->heap))
        message(error,size,"Gameplay graph changed during snapshot encoding");
    else ok=mc_transfer_commit_group(path,group,count,&items,encoders->maps ? &maps : NULL,&committed,error,size);
    if (committed) {
        if (!MCObjectHeap_adopt(parent->heap,working->heap)) {
            ok=false; message(error,size,"Durable gameplay commit could not be adopted; restart recovery is required");
        }
        if (!ok) parent->fatal=true;
    }
    for (size_t i=0;i<MC_TRANSFER_MAX_PLAYERS;i++) mc_nbt_free(&players[i]);
    mc_nbt_free(&items); mc_nbt_free(&maps); MCGameplay_abort(transaction);
    return !committed ? MC_GAMEPLAY_NOT_COMMITTED : ok ? MC_GAMEPLAY_COMMITTED : MC_GAMEPLAY_COMMITTED_NEEDS_RECOVERY;
}
