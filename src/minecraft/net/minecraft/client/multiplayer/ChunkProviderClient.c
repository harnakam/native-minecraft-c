#include "client/multiplayer/ChunkProviderClient.h"
#include "world/ChunkCoordIntPair.h"
#include <stdio.h>
#include <string.h>
static bool fail(ChunkProviderClient *p){MCObjectHeap_fail(p?p->object.heap:NULL);return false;}
static void trace(MCObject *o,MCObjectVisitor v,void *ctx){
    if(MCObjectHeap_objectSize(o)<sizeof(ChunkProviderClient)){MCObjectHeap_fail(o->heap);return;}
    ChunkProviderClient *p=(ChunkProviderClient *)o;
    p->blankChunk=(Chunk *)v((MCObject *)p->blankChunk,ctx);
    p->chunkMapping=(LongHashMap *)v((MCObject *)p->chunkMapping,ctx);
    p->chunkListing=(NativeReferenceList *)v((MCObject *)p->chunkListing,ctx);
    p->worldObj=(World *)v((MCObject *)p->worldObj,ctx);
    p->dependencyContext=v(p->dependencyContext,ctx);
}
static const MCObjectClass klass={"net.minecraft.client.multiplayer.ChunkProviderClient",MCObjectHeap_plainClone,trace,NULL};
bool ChunkProviderClient_isInstance(const MCObject *o){return o&&o->klass==&klass&&MCObjectHeap_objectSize(o)>=sizeof(ChunkProviderClient);}
static bool valid(ChunkProviderClient *p){
    return ChunkProviderClient_isInstance((MCObject *)p)&&!MCObjectHeap_failed(p->object.heap)&&
        (!p->dependencyContext||p->dependencyContext->heap==p->object.heap)?true:fail(p);
}
static bool pin(ChunkProviderClient *p,MCObjectRootScope *s,MCObject *o){
    return (!o||o->heap==p->object.heap)&&MCObjectRootScope_pin(s,o)&&!MCObjectHeap_failed(p->object.heap)?true:fail(p);
}
static bool begin(ChunkProviderClient *p,MCObjectRootScope *s){
    if(!valid(p)||!MCObjectRootScope_begin(s,p->object.heap))return fail(p);
    if(pin(p,s,(MCObject *)p)&&pin(p,s,p->dependencyContext))return true;
    MCObjectRootScope_end(s);return false;
}
static bool end(ChunkProviderClient *p,MCObjectRootScope *s,bool ok){
    ok=ok&&valid(p);if(!ok)fail(p);MCObjectRootScope_end(s);return ok;
}
static bool chunk(ChunkProviderClient *p,MCObjectRootScope *s,Chunk *c,bool nullable){
    return pin(p,s,(MCObject *)c)&&((nullable&&!c)||Chunk_isInstance((MCObject *)c))?true:fail(p);
}
static bool mapping(ChunkProviderClient *p,MCObjectRootScope *s,LongHashMap *m){
    return pin(p,s,(MCObject *)m)&&LongHashMap_isInstance((MCObject *)m)?true:fail(p);
}
static bool listing(ChunkProviderClient *p,MCObjectRootScope *s,NativeReferenceList *l){
    return pin(p,s,(MCObject *)l)&&NativeReferenceList_isInstance((MCObject *)l)?true:fail(p);
}
ChunkProviderClient *ChunkProviderClient_nativeAllocate(MCObjectHeap *h,const ChunkProviderClientDependencies *d,MCObject *c){
    if(c&&c->heap!=h){MCObjectHeap_fail(h);return NULL;}
    ChunkProviderClient *p=(ChunkProviderClient *)MCObjectHeap_alloc(h,sizeof(*p),&klass);
    if(p){p->dependencies=d;p->dependencyContext=c;}return p;
}
bool ChunkProviderClient_construct(ChunkProviderClient *p,World *w){
    MCObjectRootScope s={0};if(!begin(p,&s))return false;
    bool ok=false;MCObjectHeap *h=p->object.heap;
    if(!pin(p,&s,(MCObject *)w)||(w&&!World_isInstance((MCObject *)w)))goto done;
    const ChunkProviderClientDependencies *d=p->dependencies;
    if(d&&d->newChunkMapping&&!pin(p,&s,p->dependencyContext))goto done;
    LongHashMap *map=d&&d->newChunkMapping?d->newChunkMapping(p->dependencyContext,p):LongHashMap_new(h);
    if(!mapping(p,&s,map))goto done;
    p->chunkMapping=map;MCObjectHeap_touch(h);
    d=p->dependencies;
    if(d&&d->newChunkListing&&!pin(p,&s,p->dependencyContext))goto done;
    NativeReferenceList *list=d&&d->newChunkListing?d->newChunkListing(p->dependencyContext,p):NativeReferenceList_new(h);
    if(!listing(p,&s,list))goto done;
    p->chunkListing=list;MCObjectHeap_touch(h);
    d=p->dependencies;
    if(d&&d->newBlankChunk&&!pin(p,&s,p->dependencyContext))goto done;
    Chunk *blank=d&&d->newBlankChunk?d->newBlankChunk(p->dependencyContext,w,0,0):(Chunk *)EmptyChunk_new(h,w,0,0,NULL);
    if(!chunk(p,&s,blank,false))goto done;
    p->blankChunk=blank;p->worldObj=w;MCObjectHeap_touch(h);ok=true;
done:return end(p,&s,ok);
}
ChunkProviderClient *ChunkProviderClient_new(MCObjectHeap *h,World *w,const ChunkProviderClientDependencies *d,MCObject *c){
    ChunkProviderClient *p=ChunkProviderClient_nativeAllocate(h,d,c);return p&&ChunkProviderClient_construct(p,w)?p:NULL;
}
bool ChunkProviderClient_chunkExists(ChunkProviderClient *p,int32_t x,int32_t z){(void)x;(void)z;return valid(p);}
Chunk *ChunkProviderClient_provideChunk_base(ChunkProviderClient *p,int32_t x,int32_t z){
    MCObjectRootScope s={0};if(!begin(p,&s))return NULL;
    LongHashMap *map=p->chunkMapping;Chunk *out=NULL;bool ok=false;
    if(!mapping(p,&s,map))goto done;
    Chunk *found=(Chunk *)LongHashMap_getValueByKey(map,ChunkCoordIntPair_chunkXZ2Int(x,z));
    if(!chunk(p,&s,found,true))goto done;
    out=found?found:p->blankChunk;ok=chunk(p,&s,out,true);
done:return end(p,&s,ok)?out:NULL;
}
Chunk *ChunkProviderClient_provideChunk(ChunkProviderClient *p,int32_t x,int32_t z){
    MCObjectRootScope s={0};if(!begin(p,&s))return NULL;
    const ChunkProviderClientDependencies *d=p->dependencies;
    if(d&&d->provideChunk&&!pin(p,&s,p->dependencyContext)){end(p,&s,false);return NULL;}
    Chunk *out=d&&d->provideChunk?d->provideChunk(p->dependencyContext,p,x,z):ChunkProviderClient_provideChunk_base(p,x,z);
    bool ok=chunk(p,&s,out,true);return end(p,&s,ok)?out:NULL;
}
Chunk *ChunkProviderClient_loadChunk(ChunkProviderClient *p,int32_t x,int32_t z){
    MCObjectRootScope s={0};if(!begin(p,&s))return NULL;
    bool ok=false;Chunk *out=NULL;const ChunkProviderClientDependencies *d=p->dependencies;World *world=p->worldObj;
    if(!pin(p,&s,(MCObject *)world)||(world&&!World_isInstance((MCObject *)world)))goto done;
    if(d&&d->newChunk&&!pin(p,&s,p->dependencyContext))goto done;
    out=d&&d->newChunk?d->newChunk(p->dependencyContext,world,x,z):Chunk_new(p->object.heap,world,x,z,NULL);
    if(!chunk(p,&s,out,false))goto done;
    LongHashMap *map=p->chunkMapping;
    if(!mapping(p,&s,map)||!LongHashMap_add(map,ChunkCoordIntPair_chunkXZ2Int(x,z),(MCObject *)out))goto done;
    NativeReferenceList *list=p->chunkListing;
    if(!listing(p,&s,list)||!NativeReferenceList_add(list,(MCObject *)out))goto done;
    d=p->dependencies;
    if(d&&d->setChunkLoaded&&!pin(p,&s,p->dependencyContext))goto done;
    ok=d&&d->setChunkLoaded?d->setChunkLoaded(p->dependencyContext,out,true):Chunk_setChunkLoaded(out,true);
done:if(!end(p,&s,ok))return NULL;return out;
}
static bool remove_chunk(ChunkProviderClient *p,MCObjectRootScope *s,NativeReferenceList *list,Chunk *c){
    if(!listing(p,s,list))return false;
    for(int32_t i=0;i<NativeReferenceList_size(list);i++){
        MCObject *value=NativeReferenceList_get(list,i);if(MCObjectHeap_failed(p->object.heap)||!pin(p,s,value))return false;
        bool equal;const ChunkProviderClientDependencies *d=p->dependencies;
        if(d&&d->chunkEquals){if(!pin(p,s,p->dependencyContext)||!d->chunkEquals(p->dependencyContext,c,value,&equal)||MCObjectHeap_failed(p->object.heap))return fail(p);}
        else equal=(MCObject *)c==value;
        if(equal){NativeReferenceList_remove(list,i);return !MCObjectHeap_failed(p->object.heap);}
    }
    return !MCObjectHeap_failed(p->object.heap);
}
bool ChunkProviderClient_unloadChunk(ChunkProviderClient *p,int32_t x,int32_t z){
    MCObjectRootScope s={0};if(!begin(p,&s))return false;
    bool ok=false,empty=false;Chunk *c=ChunkProviderClient_provideChunk(p,x,z);
    if(!chunk(p,&s,c,false))goto done;
    const ChunkProviderClientDependencies *d=p->dependencies;
    if(d&&d->chunkIsEmpty){if(!pin(p,&s,p->dependencyContext)||!d->chunkIsEmpty(p->dependencyContext,c,&empty)||MCObjectHeap_failed(p->object.heap))goto done;}
    else empty=Chunk_isEmpty(c);
    if(MCObjectHeap_failed(p->object.heap))goto done;
    if(!empty){
        d=p->dependencies;
        if(d&&d->onChunkUnload&&!pin(p,&s,p->dependencyContext))goto done;
        bool unloaded=d&&d->onChunkUnload?d->onChunkUnload(p->dependencyContext,c):Chunk_onChunkUnload(c);
        if(!unloaded||MCObjectHeap_failed(p->object.heap))goto done;
    }
    LongHashMap *map=p->chunkMapping;if(!mapping(p,&s,map))goto done;
    LongHashMap_remove(map,ChunkCoordIntPair_chunkXZ2Int(x,z));if(MCObjectHeap_failed(p->object.heap))goto done;
    ok=remove_chunk(p,&s,p->chunkListing,c);
done:return end(p,&s,ok);
}
static int32_t sar4(int32_t value){return value<0?-1-(-(value+1))/16:value/16;}
Chunk *ChunkProviderClient_provideChunkAt(ChunkProviderClient *p,BlockPos *pos){
    MCObjectRootScope s={0};if(!begin(p,&s))return NULL;
    bool ok=false;Chunk *out=NULL;int32_t x,z;
    if(!pin(p,&s,(MCObject *)pos)||!BlockPos_isInstance((MCObject *)pos))goto done;
    const ChunkProviderClientDependencies *d=p->dependencies;
    if(d&&d->positionGetX){if(!pin(p,&s,p->dependencyContext)||!d->positionGetX(p->dependencyContext,pos,&x)||MCObjectHeap_failed(p->object.heap))goto done;}else if(Vec3i_getX(&pos->vec3i,&x)!=NATIVE_ARRAY_OK)goto done;
    d=p->dependencies;
    if(d&&d->positionGetZ){if(!pin(p,&s,p->dependencyContext)||!d->positionGetZ(p->dependencyContext,pos,&z)||MCObjectHeap_failed(p->object.heap))goto done;}else if(Vec3i_getZ(&pos->vec3i,&z)!=NATIVE_ARRAY_OK)goto done;
    out=ChunkProviderClient_provideChunk(p,sar4(x),sar4(z));ok=chunk(p,&s,out,true);
done:return end(p,&s,ok)?out:NULL;
}
int32_t ChunkProviderClient_getLoadedChunkCount(ChunkProviderClient *p){
    MCObjectRootScope s={0};if(!begin(p,&s))return 0;NativeReferenceList *list=p->chunkListing;
    bool ok=listing(p,&s,list);int32_t out=ok?NativeReferenceList_size(list):0;return end(p,&s,ok)?out:0;
}
NBTString *ChunkProviderClient_makeString(ChunkProviderClient *p){
    MCObjectRootScope s={0};if(!begin(p,&s))return NULL;NBTString *out=NULL;bool ok=false;
    LongHashMap *map=p->chunkMapping;if(!mapping(p,&s,map))goto done;
    int32_t mapped=LongHashMap_getNumHashElements(map);if(MCObjectHeap_failed(p->object.heap))goto done;
    NativeReferenceList *list=p->chunkListing;if(!listing(p,&s,list))goto done;
    int32_t listed=NativeReferenceList_size(list);if(MCObjectHeap_failed(p->object.heap))goto done;
    /* Native ASCII signed-int decimal/StringBuilder dependency. */
    char text[80];int n=snprintf(text,sizeof(text),"MultiplayerChunkCache: %d, %d",(int)mapped,(int)listed);
    if(n<0||(size_t)n>=sizeof(text))goto done;
    out=NBTString_fromASCII(p->object.heap,text);ok=pin(p,&s,(MCObject *)out)&&out;
done:return end(p,&s,ok)?out:NULL;
}
/* These empty/constant methods are the actual original bodies. Their bool
   completion adapters do not stand in for unload/tick lifecycle methods. */
bool ChunkProviderClient_saveChunks(ChunkProviderClient *p,bool all,MCObject *progress){(void)all;(void)progress;return valid(p);}
bool ChunkProviderClient_saveExtraData(ChunkProviderClient *p){return valid(p);}
bool ChunkProviderClient_canSave(ChunkProviderClient *p){valid(p);return false;}
bool ChunkProviderClient_populate(ChunkProviderClient *p,MCObject *provider,int32_t x,int32_t z){(void)provider;(void)x;(void)z;return valid(p);}
bool ChunkProviderClient_populateChunk(ChunkProviderClient *p,MCObject *provider,Chunk *c,int32_t x,int32_t z){(void)provider;(void)c;(void)x;(void)z;valid(p);return false;}
NativeReferenceList *ChunkProviderClient_getPossibleCreatures(ChunkProviderClient *p,MCObject *type,BlockPos *pos){(void)type;(void)pos;valid(p);return NULL;}
BlockPos *ChunkProviderClient_getStrongholdGen(ChunkProviderClient *p,World *w,NBTString *name,BlockPos *pos){(void)w;(void)name;(void)pos;valid(p);return NULL;}
bool ChunkProviderClient_recreateStructures(ChunkProviderClient *p,Chunk *c,int32_t x,int32_t z){(void)c;(void)x;(void)z;return valid(p);}
