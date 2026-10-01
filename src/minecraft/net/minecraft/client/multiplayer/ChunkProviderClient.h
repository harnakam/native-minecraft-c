#ifndef C919_SOURCE_CHUNK_PROVIDER_CLIENT_H
#define C919_SOURCE_CHUNK_PROVIDER_CLIENT_H
#include "world/chunk/Chunk.h"
#include "world/chunk/EmptyChunk.h"
#include "util/LongHashMap.h"
#include "util/NativeReferenceList.h"
typedef struct ChunkProviderClient ChunkProviderClient;
typedef struct ChunkProviderClientDependencies {
    LongHashMap *(*newChunkMapping)(MCObject *,ChunkProviderClient *);
    NativeReferenceList *(*newChunkListing)(MCObject *,ChunkProviderClient *);
    Chunk *(*newBlankChunk)(MCObject *,World *,int32_t,int32_t);
    Chunk *(*newChunk)(MCObject *,World *,int32_t,int32_t);
    Chunk *(*provideChunk)(MCObject *,ChunkProviderClient *,int32_t,int32_t);
    bool (*chunkIsEmpty)(MCObject *,Chunk *,bool *);
    /* NULL entries inherit the translated Chunk.onChunkUnload body. Any
       remaining World/entity/tile dependency failure is propagated. */
    bool (*onChunkUnload)(MCObject *,Chunk *);
    bool (*setChunkLoaded)(MCObject *,Chunk *,bool);
    bool (*chunkEquals)(MCObject *,Chunk *query,MCObject *stored,bool *);
    bool (*positionGetX)(MCObject *,BlockPos *,int32_t *);
    bool (*positionGetZ)(MCObject *,BlockPos *,int32_t *);
} ChunkProviderClientDependencies;
struct ChunkProviderClient {
    MCObject object;
    /* All four original declared instance fields, in declaration order. */
    Chunk *blankChunk;
    LongHashMap *chunkMapping;
    NativeReferenceList *chunkListing;
    World *worldObj;
    const ChunkProviderClientDependencies *dependencies;
    MCObject *dependencyContext;
};
/* Allocation-only native concrete receiver, followed by the actual constructor.
   Guava/JDK list storage is the existing managed ordered-reference adapter.
   Source Chunk inherits Object identity equality; overrides require chunkEquals.
   IChunkProvider interface/class-init logger and unloadQueuedChunks clock/tick/
   logging closure remain unported, rather than fabricated healthy work. */
ChunkProviderClient *ChunkProviderClient_nativeAllocate(MCObjectHeap *,const ChunkProviderClientDependencies *,MCObject *);
bool ChunkProviderClient_construct(ChunkProviderClient *,World *);
ChunkProviderClient *ChunkProviderClient_new(MCObjectHeap *,World *,const ChunkProviderClientDependencies *,MCObject *);
bool ChunkProviderClient_isInstance(const MCObject *);
bool ChunkProviderClient_chunkExists(ChunkProviderClient *,int32_t,int32_t);
Chunk *ChunkProviderClient_loadChunk(ChunkProviderClient *,int32_t,int32_t);
Chunk *ChunkProviderClient_provideChunk(ChunkProviderClient *,int32_t,int32_t);
Chunk *ChunkProviderClient_provideChunk_base(ChunkProviderClient *,int32_t,int32_t);
Chunk *ChunkProviderClient_provideChunkAt(ChunkProviderClient *,BlockPos *);
bool ChunkProviderClient_unloadChunk(ChunkProviderClient *,int32_t,int32_t);
int32_t ChunkProviderClient_getLoadedChunkCount(ChunkProviderClient *);
NBTString *ChunkProviderClient_makeString(ChunkProviderClient *);
bool ChunkProviderClient_saveChunks(ChunkProviderClient *,bool,MCObject *progress);
bool ChunkProviderClient_saveExtraData(ChunkProviderClient *);
bool ChunkProviderClient_canSave(ChunkProviderClient *);
bool ChunkProviderClient_populate(ChunkProviderClient *,MCObject *provider,int32_t,int32_t);
bool ChunkProviderClient_populateChunk(ChunkProviderClient *,MCObject *provider,Chunk *,int32_t,int32_t);
NativeReferenceList *ChunkProviderClient_getPossibleCreatures(ChunkProviderClient *,MCObject *creatureType,BlockPos *);
BlockPos *ChunkProviderClient_getStrongholdGen(ChunkProviderClient *,World *,NBTString *,BlockPos *);
bool ChunkProviderClient_recreateStructures(ChunkProviderClient *,Chunk *,int32_t,int32_t);
#endif
