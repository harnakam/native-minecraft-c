#include "world/ChunkCoordIntPair.h"
#include <string.h>
static const MCObjectClass klass={"net.minecraft.world.ChunkCoordIntPair",MCObjectHeap_plainClone,NULL,NULL};
static int32_t signed_bits(uint32_t bits){int32_t value;memcpy(&value,&bits,sizeof(value));return value;}
bool ChunkCoordIntPair_isInstance(const MCObject *object){return object&&object->klass==&klass&&MCObjectHeap_objectSize(object)>=sizeof(ChunkCoordIntPair);}
static bool valid(ChunkCoordIntPair *pair) {
    if(ChunkCoordIntPair_isInstance((MCObject *)pair)&&!MCObjectHeap_failed(pair->object.heap))return true;
    MCObjectHeap_fail(pair?pair->object.heap:NULL);return false;
}
ChunkCoordIntPair *ChunkCoordIntPair_new(MCObjectHeap *heap,int32_t x,int32_t z) {
    ChunkCoordIntPair *pair=(ChunkCoordIntPair *)MCObjectHeap_alloc(heap,sizeof(*pair),&klass);
    if(pair){pair->chunkXPos=x;pair->chunkZPos=z;}return pair;
}
int64_t ChunkCoordIntPair_chunkXZ2Int(int32_t x,int32_t z) {
    uint64_t bits=(uint32_t)x|((uint64_t)(uint32_t)z<<32);int64_t value;memcpy(&value,&bits,sizeof(value));return value;
}
int32_t ChunkCoordIntPair_hashCode(ChunkCoordIntPair *pair) {
    if(!valid(pair))return 0;
    uint32_t x=1664525u*(uint32_t)pair->chunkXPos+1013904223u;
    uint32_t z=1664525u*((uint32_t)pair->chunkZPos^(uint32_t)-559038737)+1013904223u;
    return signed_bits(x^z);
}
bool ChunkCoordIntPair_equals(ChunkCoordIntPair *pair,MCObject *other) {
    if(!valid(pair))return false;
    if((MCObject *)pair==other)return true;
    if(!ChunkCoordIntPair_isInstance(other))return false;
    ChunkCoordIntPair *right=(ChunkCoordIntPair *)other;
    return pair->chunkXPos==right->chunkXPos&&pair->chunkZPos==right->chunkZPos;
}
int32_t ChunkCoordIntPair_getXStart(ChunkCoordIntPair *pair){return valid(pair)?signed_bits((uint32_t)pair->chunkXPos<<4):0;}
int32_t ChunkCoordIntPair_getZStart(ChunkCoordIntPair *pair){return valid(pair)?signed_bits((uint32_t)pair->chunkZPos<<4):0;}
int32_t ChunkCoordIntPair_getCenterXPos(ChunkCoordIntPair *pair){return valid(pair)?signed_bits(((uint32_t)pair->chunkXPos<<4)+8u):0;}
int32_t ChunkCoordIntPair_getCenterZPosition(ChunkCoordIntPair *pair){return valid(pair)?signed_bits(((uint32_t)pair->chunkZPos<<4)+8u):0;}
int32_t ChunkCoordIntPair_getXEnd(ChunkCoordIntPair *pair){return valid(pair)?signed_bits(((uint32_t)pair->chunkXPos<<4)+15u):0;}
int32_t ChunkCoordIntPair_getZEnd(ChunkCoordIntPair *pair){return valid(pair)?signed_bits(((uint32_t)pair->chunkZPos<<4)+15u):0;}
BlockPos *ChunkCoordIntPair_getBlock(ChunkCoordIntPair *pair,int32_t x,int32_t y,int32_t z) {
    return valid(pair)?BlockPos_newInt(pair->object.heap,signed_bits(((uint32_t)pair->chunkXPos<<4)+(uint32_t)x),y,
        signed_bits(((uint32_t)pair->chunkZPos<<4)+(uint32_t)z)):NULL;
}
BlockPos *ChunkCoordIntPair_getCenterBlock(ChunkCoordIntPair *pair,int32_t y) {
    if(!valid(pair))return NULL;
    int32_t x=ChunkCoordIntPair_getCenterXPos(pair),z=ChunkCoordIntPair_getCenterZPosition(pair);
    return BlockPos_newInt(pair->object.heap,x,y,z);
}
