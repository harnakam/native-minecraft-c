#include "util/MCGameplay.h"
#include "util/MCGameplayWorld.h"
#include "util/MCGameplayPlayer.h"
#include "entity/item/EntityItem.h"
#include <stdio.h>
#include <string.h>
static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    MCGameplayObjects *objects=(MCGameplayObjects *)object;
    if(MCObjectHeap_objectSize(object)<sizeof(*objects)||objects->itemCount>MC_GAMEPLAY_MAX_ITEMS) {
        MCObjectHeap_fail(object->heap);return;
    }
    objects->world=visitor(objects->world,context);
    for (size_t i=0;i<MC_TRANSFER_MAX_PLAYERS;i++) objects->players[i]=visitor(objects->players[i],context);
    for (size_t i=0;i<objects->itemCount;i++) objects->items[i]=visitor(objects->items[i],context);
    objects->nativeEntityIndex=visitor(objects->nativeEntityIndex,context);
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
/* Native admission bookkeeping. The actual references and current IDs belong
   to Source World lists/entitiesById and Entity; no ID mirror is retained. */
typedef struct {
    MCObject object;
    World *world;
    NativeReferenceList *registered;
} NativeEntityIndex;
static bool index_fail(MCObjectHeap *heap){MCObjectHeap_fail(heap);return false;}
static void index_trace(MCObject *o,MCObjectVisitor v,void *context) {
    NativeEntityIndex *index=(NativeEntityIndex *)o;
    if(MCObjectHeap_objectSize(o)<sizeof(*index)){index_fail(o->heap);return;}
    index->world=(World *)v((MCObject *)index->world,context);
    index->registered=(NativeReferenceList *)v((MCObject *)index->registered,context);
}
static const MCObjectClass index_class={"native.WorldEntityRegistration",MCObjectHeap_plainClone,index_trace,NULL};
static bool owners_valid(const MCGameplayObjects *o) {
    MCObjectHeap *heap=o?o->object.heap:NULL;
    if(!o||o->object.klass!=&objects_class||MCObjectHeap_objectSize((const MCObject *)o)<sizeof(*o)||
       MCObjectHeap_failed(heap)||o->itemCount>MC_GAMEPLAY_MAX_ITEMS)return index_fail(heap);
    for(size_t i=0;i<MC_TRANSFER_MAX_PLAYERS;i++)if(o->players[i]&&o->players[i]->heap!=heap)return index_fail(heap);
    for(size_t i=0;i<o->itemCount;i++)if(!o->items[i]||o->items[i]->heap!=heap)return index_fail(heap);
    return !o->world||o->world->heap==heap||index_fail(heap);
}
static bool list_valid(NativeReferenceList *list,MCObjectHeap *heap) {
    return (NativeReferenceList_isInstance((MCObject *)list)&&list->object.heap==heap&&
        NativeReferenceList_size(list)>=0)||index_fail(heap);
}
static bool list_has(NativeReferenceList *list,MCObject *object) {
    int32_t size=NativeReferenceList_size(list);
    for(int32_t i=0;i<size;i++)if(NativeReferenceList_get(list,i)==object)return true;
    return false;
}
static bool list_remove_ref(NativeReferenceList *list,MCObject *object) {
    for(int32_t i=0;i<NativeReferenceList_size(list);) {
        if(NativeReferenceList_get(list,i)==object)NativeReferenceList_remove(list,i);else ++i;
        if(MCObjectHeap_failed(list->object.heap))return false;
    }
    return !MCObjectHeap_failed(list->object.heap);
}
static bool entity_in_world(MCObject *entity,World *world) {
    return (Entity_isInstance(entity)&&entity->heap==world->object.heap&&
        ((Entity *)entity)->worldObj==(MCObject *)world)||index_fail(world->object.heap);
}
static bool map_shape(IntHashMap *map,MCObjectHeap *heap) {
    if(!IntHashMap_isInstance((MCObject *)map)||map->object.heap!=heap||
       !map->slots||map->slots->object.heap!=heap||MCObjectHeap_objectSize((MCObject *)map->slots)<sizeof(*map->slots)||
       map->slots->length<=0||(size_t)map->slots->length>
           (MCObjectHeap_objectSize((MCObject *)map->slots)-sizeof(*map->slots))/sizeof(*map->slots->values)||
       map->count<0||(size_t)map->count>MCObjectHeap_liveObjects(heap))return index_fail(heap);
    /* Reuse the source map's private slots-class validation before traversing. */
    IntHashMap_lookupEntry(map,0);
    if(MCObjectHeap_failed(heap))return false;
    int64_t visited=0;
    for(int32_t i=0;i<map->slots->length;i++)for(IntHashMapEntry *e=map->slots->values[i];e;e=e->nextEntry) {
        if(++visited>map->count||!IntHashMapEntry_isInstance((MCObject *)e)||e->object.heap!=heap||
           !e->valueEntry||e->valueEntry->heap!=heap||
           e->slotHash!=IntHashMap_computeHash(e->hashEntry)||
           IntHashMap_getSlotIndex(e->slotHash,map->slots->length)!=i||
           IntHashMap_lookupEntry(map,e->hashEntry)!=e)return index_fail(heap);
    }
    return visited==map->count||index_fail(heap);
}
static bool world_indexes_ready(World *world,MCGameplayObjects *owners) {
    MCObjectHeap *heap=owners->object.heap;
    return (World_isInstance((MCObject *)world)&&world->object.heap==heap&&world->owners==owners&&
        list_valid(world->loadedEntityList,heap)&&list_valid(world->playerEntities,heap)&&
        map_shape(world->entitiesById,heap))||index_fail(heap);
}
static NativeEntityIndex *index_state(MCGameplayObjects *o,bool create) {
    NativeEntityIndex *index=(NativeEntityIndex *)o->nativeEntityIndex;
    if(index) {
        if(index->object.klass!=&index_class||index->object.heap!=o->object.heap||
           MCObjectHeap_objectSize((MCObject *)index)<sizeof(*index)||
           !list_valid(index->registered,o->object.heap)||
           (index->world&&(!World_isInstance((MCObject *)index->world)||index->world->object.heap!=o->object.heap))) {
            index_fail(o->object.heap);return NULL;
        }
    } else if(create) {
        index=(NativeEntityIndex *)MCObjectHeap_alloc(o->object.heap,sizeof(*index),&index_class);
        if(index)index->registered=NativeReferenceList_new(o->object.heap);
        if(!index||!index->registered)return NULL;
        o->nativeEntityIndex=(MCObject *)index;MCObjectHeap_touch(o->object.heap);
    }
    return index;
}
static bool currently_registered(const MCGameplayObjects *o,MCObject *object) {
    for(size_t i=0;i<MC_TRANSFER_MAX_PLAYERS;i++)if(o->players[i]==object)return true;
    for(size_t i=0;i<o->itemCount;i++)if(o->items[i]==object)return true;
    return false;
}
static bool detach(World *world,MCObject *object) {
    if(!list_remove_ref(world->loadedEntityList,object)||!list_remove_ref(world->playerEntities,object))return false;
    IntHashMap *map=world->entitiesById;
    for(int32_t i=0;i<map->slots->length;i++)for(IntHashMapEntry *e=map->slots->values[i],*next;e;e=next) {
        next=e->nextEntry;
        if(e->valueEntry==object)IntHashMap_removeEntry(map,e->hashEntry);
        if(MCObjectHeap_failed(world->object.heap))return false;
    }
    return true;
}
static bool ensure_entry(World *world,MCObject *object) {
    if(!entity_in_world(object,world))return false;
    IntHashMap *map=world->entitiesById;int32_t id=((Entity *)object)->entityId;
    MCObject *existing=IntHashMap_lookup(map,id);
    if(MCObjectHeap_failed(world->object.heap))return false;
    if(existing)return existing==object||index_fail(world->object.heap);
    return IntHashMap_addKey(map,id,object);
}
static bool rekey(World *world) {
    IntHashMap *map=world->entitiesById;NativeReferenceList *changed=NULL;
    /* Remove every stale key before adding current keys, including ID swaps. */
    for(int32_t i=0;i<map->slots->length;i++)for(IntHashMapEntry *e=map->slots->values[i],*next;e;e=next) {
        next=e->nextEntry;
        if(!entity_in_world(e->valueEntry,world))return false;
        if(e->hashEntry!=((Entity *)e->valueEntry)->entityId) {
            if(!changed)changed=NativeReferenceList_new(world->object.heap);
            if(!changed||!NativeReferenceList_add(changed,e->valueEntry))return false;
            IntHashMap_removeEntry(map,e->hashEntry);
        }
    }
    for(int32_t i=0;i<(changed?NativeReferenceList_size(changed):0);i++)
        if(!ensure_entry(world,NativeReferenceList_get(changed,i)))return false;
    for(int32_t i=0;i<NativeReferenceList_size(world->loadedEntityList);i++)
        if(!ensure_entry(world,NativeReferenceList_get(world->loadedEntityList,i)))return false;
    for(int32_t i=0;i<NativeReferenceList_size(world->playerEntities);i++) {
        MCObject *player=NativeReferenceList_get(world->playerEntities,i);
        if(!MCGameplayPlayer_isInstance(player)||!ensure_entry(world,player))return index_fail(world->object.heap);
    }
    return !MCObjectHeap_failed(world->object.heap);
}
bool MCGameplay_validateWorldIndexes(const MCGameplayObjects *objects) {
    if(!owners_valid(objects))return false;
    MCGameplayObjects *o=(MCGameplayObjects *)objects;
    NativeEntityIndex *index=index_state(o,false);
    if(MCObjectHeap_failed(o->object.heap))return false;
    if(!World_isInstance(o->world))return !index||!index->world||index_fail(o->object.heap);
    World *world=(World *)o->world;
    if(!world_indexes_ready(world,o)||!index||index->world!=world)return index_fail(o->object.heap);
    for(int32_t i=0;i<world->entitiesById->slots->length;i++)
        for(IntHashMapEntry *e=world->entitiesById->slots->values[i];e;e=e->nextEntry)
            if(!entity_in_world(e->valueEntry,world)||e->hashEntry!=((Entity *)e->valueEntry)->entityId)return index_fail(o->object.heap);
    NativeReferenceList *lists[]={world->loadedEntityList,world->playerEntities};
    for(size_t l=0;l<2;l++)for(int32_t i=0;i<NativeReferenceList_size(lists[l]);i++) {
        MCObject *e=NativeReferenceList_get(lists[l],i);
        if(!entity_in_world(e,world)||(l==1&&!MCGameplayPlayer_isInstance(e))||
           IntHashMap_lookup(world->entitiesById,((Entity *)e)->entityId)!=e)return index_fail(o->object.heap);
    }
    for(int32_t i=0;i<NativeReferenceList_size(index->registered);i++)
        if(!currently_registered(o,NativeReferenceList_get(index->registered,i)))return index_fail(o->object.heap);
    for(size_t i=0;i<MC_TRANSFER_MAX_PLAYERS+o->itemCount;i++) {
        bool player=i<MC_TRANSFER_MAX_PLAYERS;MCObject *e=player?o->players[i]:o->items[i-MC_TRANSFER_MAX_PLAYERS];
        if(!e)continue;
        if(!(player?MCGameplayPlayer_isInstance(e):EntityItem_isInstance(e))||!entity_in_world(e,world)||
           !list_has(index->registered,e)||!list_has(world->loadedEntityList,e)||
           (player&&!list_has(world->playerEntities,e)))return index_fail(o->object.heap);
    }
    return !MCObjectHeap_failed(o->object.heap);
}
bool MCGameplay_reindexWorld(MCGameplayObjects *o) {
    if(!owners_valid(o))return false;
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,o->object.heap))return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)o);
    World *world=World_isInstance(o->world)?(World *)o->world:NULL;
    NativeEntityIndex *index=ok?index_state(o,world!=NULL):NULL;
    ok=ok&&!MCObjectHeap_failed(o->object.heap);
    if(ok&&index&&index->world) {
        World *previous=index->world;
        ok=world_indexes_ready(previous,o);
        for(int32_t i=0;ok&&i<NativeReferenceList_size(index->registered);) {
            MCObject *e=NativeReferenceList_get(index->registered,i);
            if(previous!=world||!currently_registered(o,e)) {
                ok=detach(previous,e);
                if(ok)NativeReferenceList_remove(index->registered,i);
            } else ++i;
        }
    }
    if(ok&&index&&index->world!=world){index->world=world;MCObjectHeap_touch(o->object.heap);}
    if(ok&&world) {
        ok=world_indexes_ready(world,o);
        for(size_t i=0;ok&&i<MC_TRANSFER_MAX_PLAYERS+o->itemCount;i++) {
            bool player=i<MC_TRANSFER_MAX_PLAYERS;MCObject *e=player?o->players[i]:o->items[i-MC_TRANSFER_MAX_PLAYERS];
            if(!e)continue;
            ok=(player?MCGameplayPlayer_isInstance(e):EntityItem_isInstance(e))&&entity_in_world(e,world);
            if(!ok){index_fail(o->object.heap);break;}
            if(!list_has(world->loadedEntityList,e))ok=NativeReferenceList_add(world->loadedEntityList,e);
            if(ok&&player&&!list_has(world->playerEntities,e))ok=NativeReferenceList_add(world->playerEntities,e);
            if(ok&&!list_has(index->registered,e))ok=NativeReferenceList_add(index->registered,e);
        }
        if(ok)ok=rekey(world);
    }
    if(ok)ok=MCGameplay_validateWorldIndexes(o);
    if(!ok)index_fail(o->object.heap);
    MCObjectRootScope_end(&scope);return ok;
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
    objects->world=world; MCObjectHeap_touch(game->heap); return MCGameplay_reindexWorld(objects);
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
    MCObjectHeap_touch(game->heap); return MCGameplay_reindexWorld(objects);
}
bool MCGameplay_addItem(MCGameplay *game,MCObject *item) {
    MCGameplayObjects *objects=MCGameplay_get(game);
    if (!objects || !item || !accepts(game,item) || objects->itemCount>=MC_GAMEPLAY_MAX_ITEMS) return false;
    for (size_t i=0;i<objects->itemCount;i++) if (objects->items[i]==item) return false;
    objects->items[objects->itemCount++]=item; MCObjectHeap_touch(game->heap); return MCGameplay_reindexWorld(objects);
}
bool MCGameplay_removeItem(MCGameplay *game,size_t index) {
    MCGameplayObjects *objects=MCGameplay_get(game);
    if (!objects || !accepts(game,NULL) || index>=objects->itemCount) return false;
    memmove(objects->items+index,objects->items+index+1,(objects->itemCount-index-1)*sizeof(*objects->items));
    objects->items[--objects->itemCount]=NULL; MCObjectHeap_touch(game->heap); return MCGameplay_reindexWorld(objects);
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
        ((MCGameplayWorld *)objects->world)->isRemote;
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
    if (ok) ok=MCGameplay_reindexWorld(objects);
    if (ok) ok=validate(objects,context);
    ok=ok && !working->fatal && !MCObjectHeap_failed(working->heap) &&
        MCGameplay_validateWorldIndexes(objects) &&
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
    if(encoded)encoded=MCGameplay_reindexWorld(objects);
    for (size_t i=0;i<MC_TRANSFER_MAX_PLAYERS && encoded;i++) if (objects->players[i]) {
        encoded=encoders->player(objects,i,&players[count],context);
        group[count]=(mc_transfer_player){objects->uuids[i],&players[count]}; ++count;
    }
    if (encoded) encoded=encoders->items(objects,&items,context);
    if (encoded && encoders->maps) encoded=encoders->maps(objects,&maps,context);
    encoded=encoded && !working->fatal && !MCObjectHeap_failed(working->heap) &&
        MCGameplay_validateWorldIndexes(objects) &&
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
