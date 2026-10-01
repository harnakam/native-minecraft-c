#include "client/multiplayer/ChunkProviderClient.h"
#include "world/ChunkCoordIntPair.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{checks++;if(!(v)){fprintf(stderr,"ChunkProvider check%u line%d: %s\n",checks,__LINE__,#v);exit(1);}}while(0)
static void source_constructor(void){
 MCObjectHeap *h=MCObjectHeap_new(32u*1024u*1024u);CHECK(h);World *w=World_nativeAllocate(h,NULL,NULL);CHECK(w);
 ChunkProviderClient *p=ChunkProviderClient_nativeAllocate(h,NULL,NULL);CHECK(p);
 CHECK(ChunkProviderClient_construct(p,w));CHECK(p->chunkMapping&&p->chunkListing&&p->blankChunk);
 CHECK(p->worldObj==w&&p->blankChunk->worldObj==w&&EmptyChunk_isInstance((MCObject *)p->blankChunk));
 CHECK(p->blankChunk->xPosition==0&&p->blankChunk->zPosition==0);
 CHECK(!MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
typedef struct Witness {
 MCObject object;ChunkProviderClient *p;World *world[2];Chunk *retained,*provided;
 LongHashMap *oldMap,*alternateMap;NativeReferenceList *oldList,*alternateList;
 unsigned calls,failAt,change;char events[96];bool equalAll,empty;
 int32_t x,z;World *capturedWorld;
 MCObject *replacementContext;
} Witness;
static void witness_trace(MCObject *o,MCObjectVisitor v,void *c){
 Witness *w=(Witness *)o;w->p=(ChunkProviderClient *)v((MCObject *)w->p,c);
 for(unsigned i=0;i<2;i++)w->world[i]=(World *)v((MCObject *)w->world[i],c);
 w->retained=(Chunk *)v((MCObject *)w->retained,c);w->provided=(Chunk *)v((MCObject *)w->provided,c);
 w->oldMap=(LongHashMap *)v((MCObject *)w->oldMap,c);w->alternateMap=(LongHashMap *)v((MCObject *)w->alternateMap,c);
 w->oldList=(NativeReferenceList *)v((MCObject *)w->oldList,c);w->alternateList=(NativeReferenceList *)v((MCObject *)w->alternateList,c);
 w->capturedWorld=(World *)v((MCObject *)w->capturedWorld,c);
 w->replacementContext=v(w->replacementContext,c);
}
static const MCObjectClass witnessClass={"test.ChunkProviderWitness",MCObjectHeap_plainClone,witness_trace,NULL};
static bool event(Witness *w,char c){
 CHECK(MCObjectHeap_hasBorrowers(w->object.heap));CHECK(w->calls+1<sizeof(w->events));w->events[w->calls++]=c;w->events[w->calls]=0;
 if(w->calls==w->failAt){w->p->worldObj=w->world[1];MCObjectHeap_fail(w->object.heap);return false;}return true;
}
static LongHashMap *new_map(MCObject *o,ChunkProviderClient *p){Witness *w=(Witness *)o;CHECK(w->p==p);return event(w,'m')?LongHashMap_new(o->heap):NULL;}
static NativeReferenceList *new_list(MCObject *o,ChunkProviderClient *p){Witness *w=(Witness *)o;CHECK(w->p==p);return event(w,'a')?NativeReferenceList_new(o->heap):NULL;}
static Chunk *new_blank(MCObject *o,World *world,int32_t x,int32_t z){Witness *w=(Witness *)o;CHECK(x==0&&z==0);w->capturedWorld=world;return event(w,'b')?(Chunk *)EmptyChunk_new(o->heap,world,x,z,NULL):NULL;}
static Chunk *new_chunk(MCObject *o,World *world,int32_t x,int32_t z){Witness *w=(Witness *)o;w->capturedWorld=world;w->x=x;w->z=z;
 if(!event(w,'n'))return NULL;
 if(w->change==3)w->p->worldObj=w->world[1];
 w->retained=Chunk_new(o->heap,world,x,z,NULL);return w->retained;
}
static bool set_loaded(MCObject *o,Chunk *c,bool value){Witness *w=(Witness *)o;CHECK(value&&c==w->retained);
 if(!event(w,'l'))return false;
 CHECK(NativeReferenceList_get(w->p->chunkListing,NativeReferenceList_size(w->p->chunkListing)-1)==(MCObject *)c);
 if(w->change==2){w->oldList=w->p->chunkListing;w->p->chunkListing=w->alternateList;}
 if(w->change==4)w->p->dependencyContext=w->replacementContext;
 return Chunk_setChunkLoaded(c,value);
}
static bool is_empty(MCObject *o,Chunk *c,bool *out){Witness *w=(Witness *)o;CHECK(c);if(!event(w,'i'))return false;*out=w->empty||Chunk_isEmpty(c);if(w->change==5)w->p->dependencyContext=w->replacementContext;return !MCObjectHeap_failed(o->heap);}
static bool unload(MCObject *o,Chunk *c){Witness *w=(Witness *)o;if(!event(w,'u'))return false;
 if(w->change==1){w->oldMap=w->p->chunkMapping;w->oldList=w->p->chunkListing;w->p->chunkMapping=w->alternateMap;w->p->chunkListing=w->alternateList;}
 return Chunk_setChunkLoaded(c,false);
}
static bool equality(MCObject *o,Chunk *query,MCObject *stored,bool *out){Witness *w=(Witness *)o;if(!event(w,'e'))return false;*out=w->equalAll||(MCObject *)query==stored;return true;}
static Chunk *provided(MCObject *o,ChunkProviderClient *p,int32_t x,int32_t z){Witness *w=(Witness *)o;CHECK(p==w->p);w->x=x;w->z=z;return event(w,'p')?w->provided:NULL;}
static bool position_x(MCObject *o,BlockPos *p,int32_t *out){Witness *w=(Witness *)o;CHECK(p);if(!event(w,'x'))return false;*out=p->x;if(w->change==6)w->p->dependencyContext=w->replacementContext;return true;}
static bool position_z(MCObject *o,BlockPos *p,int32_t *out){Witness *w=(Witness *)o;CHECK(p);if(!event(w,'z'))return false;*out=p->z;return true;}
static const ChunkProviderClientDependencies hooks={.newChunkMapping=new_map,.newChunkListing=new_list,.newBlankChunk=new_blank,.newChunk=new_chunk,.chunkIsEmpty=is_empty,.onChunkUnload=unload,.setChunkLoaded=set_loaded};
static Witness *witness(MCObjectHeap *h,bool construct){
 Witness *w=(Witness *)MCObjectHeap_alloc(h,sizeof(*w),&witnessClass);CHECK(w);
 for(unsigned i=0;i<2;i++){w->world[i]=World_nativeAllocate(h,NULL,NULL);CHECK(w->world[i]);}
 w->p=ChunkProviderClient_nativeAllocate(h,&hooks,(MCObject *)w);CHECK(w->p);
 w->alternateMap=LongHashMap_new(h);w->alternateList=NativeReferenceList_new(h);CHECK(w->alternateMap&&w->alternateList);
 if(construct){CHECK(ChunkProviderClient_construct(w->p,w->world[0]));CHECK(!strcmp(w->events,"mab"));w->calls=0;w->events[0]=0;}
 return w;
}
static void duplicate_and_blank_semantics(void){
 MCObjectHeap *h=MCObjectHeap_new(32u*1024u*1024u);CHECK(h);Witness *w=witness(h,true);ChunkProviderClient *p=w->p;
 CHECK(ChunkProviderClient_chunkExists(p,INT32_MIN,INT32_MAX));CHECK(ChunkProviderClient_provideChunk(p,31,-20)==p->blankChunk);
 CHECK(ChunkProviderClient_getLoadedChunkCount(p)==0);CHECK(ChunkProviderClient_unloadChunk(p,31,-20));CHECK(!strcmp(w->events,"i"));
 w->calls=0;w->events[0]=0;Chunk *first=ChunkProviderClient_loadChunk(p,-1,INT32_MIN);CHECK(first&&first->isChunkLoaded);
 Chunk *second=ChunkProviderClient_loadChunk(p,-1,INT32_MIN);CHECK(second&&second!=first&&second->isChunkLoaded);
 CHECK(ChunkProviderClient_provideChunk(p,-1,INT32_MIN)==second);CHECK(ChunkProviderClient_getLoadedChunkCount(p)==2);
 CHECK(LongHashMap_getNumHashElements(p->chunkMapping)==1);CHECK(NativeReferenceList_get(p->chunkListing,0)==(MCObject *)first);
 CHECK(NBTString_equalsASCII(ChunkProviderClient_makeString(p),"MultiplayerChunkCache: 1, 2"));CHECK(!strcmp(w->events,"nlnl"));
 w->calls=0;w->events[0]=0;CHECK(ChunkProviderClient_unloadChunk(p,-1,INT32_MIN));CHECK(!strcmp(w->events,"iu"));
 CHECK(!second->isChunkLoaded&&first->isChunkLoaded);CHECK(ChunkProviderClient_getLoadedChunkCount(p)==1);CHECK(NativeReferenceList_get(p->chunkListing,0)==(MCObject *)first);
 CHECK(ChunkProviderClient_provideChunk(p,-1,INT32_MIN)==p->blankChunk);CHECK(NBTString_equalsASCII(ChunkProviderClient_makeString(p),"MultiplayerChunkCache: 0, 1"));
 CHECK(!MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
static void captured_arguments_and_live_fields(void){
 MCObjectHeap *h=MCObjectHeap_new(32u*1024u*1024u);CHECK(h);Witness *w=witness(h,true);w->change=3;
 Chunk *c=ChunkProviderClient_loadChunk(w->p,12,-22);CHECK(c&&c->worldObj==w->world[0]);CHECK(w->p->worldObj==w->world[1]&&w->capturedWorld==w->world[0]);
 w->change=1;CHECK(LongHashMap_add(w->alternateMap,ChunkCoordIntPair_chunkXZ2Int(12,-22),(MCObject *)c));CHECK(NativeReferenceList_add(w->alternateList,(MCObject *)c));
 w->calls=0;w->events[0]=0;CHECK(ChunkProviderClient_unloadChunk(w->p,12,-22));CHECK(!strcmp(w->events,"iu"));
 CHECK(w->p->chunkMapping==w->alternateMap&&w->p->chunkListing==w->alternateList);
 CHECK(!LongHashMap_getValueByKey(w->alternateMap,ChunkCoordIntPair_chunkXZ2Int(12,-22)));CHECK(NativeReferenceList_size(w->alternateList)==0);
 CHECK(LongHashMap_getValueByKey(w->oldMap,ChunkCoordIntPair_chunkXZ2Int(12,-22))==(MCObject *)c);CHECK(NativeReferenceList_size(w->oldList)==1);
 MCObjectHeap_free(h);
 h=MCObjectHeap_new(32u*1024u*1024u);CHECK(h);w=witness(h,true);w->change=2;c=ChunkProviderClient_loadChunk(w->p,4,8);
 CHECK(c&&c->isChunkLoaded);CHECK(w->p->chunkListing==w->alternateList&&NativeReferenceList_size(w->oldList)==1);CHECK(ChunkProviderClient_getLoadedChunkCount(w->p)==0);MCObjectHeap_free(h);
}
static void constructor_and_method_failure_prefixes(void){
 for(unsigned failed=1;failed<=3;failed++){
  MCObjectHeap *h=MCObjectHeap_new(32u*1024u*1024u);CHECK(h);Witness *w=witness(h,false);w->failAt=failed;
  CHECK(!ChunkProviderClient_construct(w->p,w->world[0]));CHECK(MCObjectHeap_failed(h)&&w->calls==failed);
  CHECK((w->p->chunkMapping!=NULL)==(failed>1));CHECK((w->p->chunkListing!=NULL)==(failed>2));CHECK(!w->p->blankChunk);
  CHECK(w->p->worldObj==w->world[1]);CHECK(!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);
 }
 for(unsigned failed=1;failed<=2;failed++){
  MCObjectHeap *h=MCObjectHeap_new(32u*1024u*1024u);CHECK(h);Witness *w=witness(h,true);w->failAt=failed;
  CHECK(!ChunkProviderClient_loadChunk(w->p,3,9));CHECK(MCObjectHeap_failed(h)&&w->calls==failed);
  CHECK(w->p->chunkMapping->numHashElements==(failed==2?1:0));CHECK(w->p->chunkListing->size==(failed==2?1:0));
  CHECK(!w->retained||!w->retained->isChunkLoaded);CHECK(!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);
 }
 for(unsigned failed=1;failed<=2;failed++){
  MCObjectHeap *h=MCObjectHeap_new(32u*1024u*1024u);CHECK(h);Witness *w=witness(h,true);Chunk *c=ChunkProviderClient_loadChunk(w->p,3,9);CHECK(c);
  w->calls=0;w->events[0]=0;w->failAt=failed;CHECK(!ChunkProviderClient_unloadChunk(w->p,3,9));
  CHECK(MCObjectHeap_failed(h)&&w->p->chunkMapping->numHashElements==1&&w->p->chunkListing->size==1);CHECK(c->isChunkLoaded);CHECK(!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);
 }
 MCObjectHeap *h=MCObjectHeap_new(32u*1024u*1024u);CHECK(h);ChunkProviderClient *p=ChunkProviderClient_new(h,NULL,NULL,NULL);CHECK(p);
 Chunk *c=ChunkProviderClient_loadChunk(p,1,2);CHECK(c);CHECK(!ChunkProviderClient_unloadChunk(p,1,2));CHECK(MCObjectHeap_failed(h));
 CHECK(p->chunkMapping->numHashElements==1&&p->chunkListing->size==1&&!c->isChunkLoaded);MCObjectHeap_free(h);
}
static void default_source_unload_world_closure(void){
 MCObjectHeap *h=MCObjectHeap_new(32u*1024u*1024u);CHECK(h);World *w=World_nativeAllocate(h,NULL,NULL);CHECK(w);
 WorldInfo *i=WorldInfo_nativeAllocate(h,NULL,NULL);CHECK(i&&WorldInfo_construct(i));
 WorldProvider *provider=WorldProvider_nativeAllocate(h,NULL,NULL);CHECK(provider&&WorldProvider_construct(provider));
 CHECK(World_construct(w,NULL,i,provider,NULL,true));ChunkProviderClient *p=ChunkProviderClient_new(h,w,NULL,NULL);CHECK(p);
 Chunk *c=ChunkProviderClient_loadChunk(p,7,-8);CHECK(c&&c->isChunkLoaded);CHECK(w->unloadedEntityList&&w->tileEntitiesToBeRemoved);
 CHECK(ChunkProviderClient_unloadChunk(p,7,-8));CHECK(!c->isChunkLoaded&&p->chunkMapping->numHashElements==0&&p->chunkListing->size==0);
 CHECK(NativeReferenceList_size(w->unloadedEntityList)==0&&NativeReferenceList_size(w->tileEntitiesToBeRemoved)==0);
 CHECK(ChunkProviderClient_provideChunk(p,7,-8)==p->blankChunk);CHECK(!MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
static void virtual_equals_and_position(void){
 MCObjectHeap *h=MCObjectHeap_new(32u*1024u*1024u);CHECK(h);Witness *w=witness(h,true);ChunkProviderClientDependencies deps=hooks;deps.chunkEquals=equality;w->p->dependencies=&deps;
 Chunk *a=ChunkProviderClient_loadChunk(w->p,2,3),*b=ChunkProviderClient_loadChunk(w->p,2,3);CHECK(a&&b);w->equalAll=true;w->calls=0;w->events[0]=0;
 CHECK(ChunkProviderClient_unloadChunk(w->p,2,3));CHECK(!strcmp(w->events,"iue"));CHECK(NativeReferenceList_get(w->p->chunkListing,0)==(MCObject *)b);MCObjectHeap_free(h);
 static const int32_t coordinates[]={INT32_MIN,-17,-16,-1,0,15,16,INT32_MAX};
 for(unsigned n=0;n<sizeof(coordinates)/sizeof(coordinates[0]);n++){
  h=MCObjectHeap_new(32u*1024u*1024u);CHECK(h);w=witness(h,true);deps=hooks;deps.provideChunk=provided;deps.positionGetX=position_x;deps.positionGetZ=position_z;w->p->dependencies=&deps;w->provided=w->p->blankChunk;
  int32_t v=coordinates[n];BlockPos *pos=DataWatcher_blockPos(h,v,17,-1);CHECK(pos);
  CHECK(ChunkProviderClient_provideChunkAt(w->p,pos)==w->provided);CHECK(!strcmp(w->events,"xzp"));
  CHECK(w->x==(v<0?-1-(-(v+1))/16:v/16)&&w->z==-1);CHECK(!MCObjectHeap_failed(h));MCObjectHeap_free(h);
 }
 h=MCObjectHeap_new(32u*1024u*1024u);CHECK(h);w=witness(h,true);deps=hooks;deps.provideChunk=provided;w->p->dependencies=&deps;
 CHECK(!ChunkProviderClient_provideChunk(w->p,1,2)&&!MCObjectHeap_failed(h));CHECK(!ChunkProviderClient_unloadChunk(w->p,1,2)&&MCObjectHeap_failed(h));CHECK(!strcmp(w->events,"pp"));MCObjectHeap_free(h);
}
static void source_simple_methods_and_native_guards(void){
 MCObjectHeap *h=MCObjectHeap_new(32u*1024u*1024u);CHECK(h);ChunkProviderClient *p=ChunkProviderClient_new(h,NULL,NULL,NULL);CHECK(p);
 CHECK(ChunkProviderClient_saveChunks(p,false,NULL)&&ChunkProviderClient_saveChunks(p,true,NULL));CHECK(ChunkProviderClient_saveExtraData(p));CHECK(!ChunkProviderClient_canSave(p));
 CHECK(ChunkProviderClient_populate(p,NULL,INT32_MIN,INT32_MAX));CHECK(!ChunkProviderClient_populateChunk(p,NULL,NULL,0,0));
 CHECK(!ChunkProviderClient_getPossibleCreatures(p,NULL,NULL));CHECK(!ChunkProviderClient_getStrongholdGen(p,NULL,NULL,NULL));CHECK(ChunkProviderClient_recreateStructures(p,NULL,0,0));
 CHECK(!MCObjectHeap_failed(h)&&p->worldObj==NULL&&p->blankChunk->worldObj==NULL);
 MCObject *tiny=MCObjectHeap_alloc(h,sizeof(MCObject),p->object.klass);CHECK(tiny);CHECK(!ChunkProviderClient_isInstance(tiny));CHECK(!ChunkProviderClient_chunkExists((ChunkProviderClient *)tiny,0,0)&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
 h=MCObjectHeap_new(32u*1024u*1024u);MCObjectHeap *foreign=MCObjectHeap_new(32u*1024u*1024u);CHECK(h&&foreign);p=ChunkProviderClient_new(h,NULL,NULL,NULL);CHECK(p);p->dependencyContext=(MCObject *)World_nativeAllocate(foreign,NULL,NULL);CHECK(p->dependencyContext);
 CHECK(!ChunkProviderClient_chunkExists(p,0,0)&&MCObjectHeap_failed(h)&&!MCObjectHeap_failed(foreign));MCObjectHeap_free(h);MCObjectHeap_free(foreign);
 h=MCObjectHeap_new(1);CHECK(h);CHECK(!ChunkProviderClient_new(h,NULL,NULL,NULL)&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
static void graph_clone_aliases_and_adoption(void){
 MCObjectHeap *h=MCObjectHeap_new(32u*1024u*1024u);CHECK(h);Witness *w=witness(h,true);Chunk *a=ChunkProviderClient_loadChunk(w->p,5,7),*b=ChunkProviderClient_loadChunk(w->p,5,7);CHECK(a&&b);
 MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)w->p));CHECK(MCObjectHeap_collect(h));
 MCObjectHeap *copy=MCObjectHeap_clone(h);CHECK(copy);MCObjectRoot cr={0};CHECK(MCObjectRoot_rebind(&cr,copy,&root));
 ChunkProviderClient *p=(ChunkProviderClient *)MCObjectRoot_get(&cr);Witness *cw=(Witness *)p->dependencyContext;CHECK(p!=w->p&&cw!=w&&cw->p==p);
 Chunk *mapped=ChunkProviderClient_provideChunk(p,5,7);CHECK(mapped&&mapped!=b&&mapped==cw->retained);CHECK(NativeReferenceList_get(p->chunkListing,1)==(MCObject *)mapped);
 CHECK(mapped->worldObj==p->worldObj&&p->worldObj==cw->world[0]&&p->blankChunk->worldObj==p->worldObj);
 cw->calls=0;cw->events[0]=0;CHECK(ChunkProviderClient_unloadChunk(p,5,7));CHECK(ChunkProviderClient_getLoadedChunkCount(p)==1);CHECK(w->p->chunkListing->size==2);
 CHECK(MCObjectHeap_canAdopt(h,copy));CHECK(MCObjectHeap_adopt(h,copy));MCObjectHeap_free(copy);p=(ChunkProviderClient *)MCObjectRoot_get(&root);CHECK(MCObjectHeap_collect(h));
 CHECK(ChunkProviderClient_getLoadedChunkCount(p)==1);CHECK(ChunkProviderClient_provideChunk(p,5,7)==p->blankChunk);CHECK(!MCObjectHeap_failed(h));MCObjectRoot_drop(&root);MCObjectHeap_free(h);
}
static void mutable_context_and_collection_failure_guards(void){
 for(unsigned mode=4;mode<=6;mode++){
  MCObjectHeap *h=MCObjectHeap_new(32u*1024u*1024u),*foreign=MCObjectHeap_new(32u*1024u*1024u);CHECK(h&&foreign);
  Witness *w=witness(h,true);w->replacementContext=(MCObject *)World_nativeAllocate(foreign,NULL,NULL);CHECK(w->replacementContext);
  Chunk *c=NULL;if(mode==5){c=ChunkProviderClient_loadChunk(w->p,2,3);CHECK(c);}
  w->calls=0;w->events[0]=0;w->change=mode;
  if(mode==4){CHECK(!ChunkProviderClient_loadChunk(w->p,2,3));CHECK(!strcmp(w->events,"nl"));c=w->retained;CHECK(c&&c->isChunkLoaded);}
  else if(mode==5){CHECK(!ChunkProviderClient_unloadChunk(w->p,2,3));CHECK(!strcmp(w->events,"i"));CHECK(c->isChunkLoaded);}
  else {ChunkProviderClientDependencies d=hooks;d.positionGetX=position_x;d.positionGetZ=position_z;w->p->dependencies=&d;
   BlockPos *pos=DataWatcher_blockPos(h,32,0,48);CHECK(pos);CHECK(!ChunkProviderClient_provideChunkAt(w->p,pos));CHECK(!strcmp(w->events,"x"));}
  CHECK(MCObjectHeap_failed(h)&&!MCObjectHeap_failed(foreign));CHECK(!MCObjectHeap_hasBorrowers(h));
  CHECK(w->p->chunkMapping->numHashElements==(mode==6?0:1)&&w->p->chunkListing->size==(mode==6?0:1));
  MCObjectHeap_free(h);MCObjectHeap_free(foreign);
 }
 for(unsigned missing=0;missing<2;missing++){
  MCObjectHeap *h=MCObjectHeap_new(32u*1024u*1024u);CHECK(h);Witness *w=witness(h,true);
  LongHashMap *map=w->p->chunkMapping;if(missing)w->p->chunkListing=NULL;else w->p->chunkMapping=NULL;
  CHECK(!ChunkProviderClient_loadChunk(w->p,2,3));CHECK(!strcmp(w->events,"n"));CHECK(w->retained&&!w->retained->isChunkLoaded);
  CHECK(map->numHashElements==(missing?1:0));CHECK(MCObjectHeap_failed(h)&&!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);
 }
}
int main(void){source_constructor();duplicate_and_blank_semantics();captured_arguments_and_live_fields();constructor_and_method_failure_prefixes();default_source_unload_world_closure();virtual_equals_and_position();source_simple_methods_and_native_guards();graph_clone_aliases_and_adoption();mutable_context_and_collection_failure_guards();printf("Source ChunkProviderClient checks%u\n",checks);return 0;}
