#ifndef C919_SOURCE_S34_PACKET_MAPS_H
#define C919_SOURCE_S34_PACKET_MAPS_H
#include "network/PacketBuffer.h"
#include "network/play/INetHandlerPlayClient.h"
#include "util/NativeCollectionTyped.h"

typedef struct MapData MapData;
struct S34PacketMaps {
    MCObject object;
    int32_t mapId;
    int8_t mapScale;
    NativeTypedObjectArray *mapVisiblePlayersVec4b;
    int32_t mapMinX, mapMinY, mapMaxX, mapMaxY;
    NativeByteArray *mapDataBytes;
    /* Native virtual Collection boundary; no additional Source state. */
    const NativeCollectionTypedMethods *nativeVisiblePlayersMethods;
    MCObject *nativeVisiblePlayersContext;
};
bool S34PacketMaps_isInstance(const MCObject *);
S34PacketMaps *S34PacketMaps_nativeAllocate(MCObjectHeap *);
S34PacketMaps *S34PacketMaps_new_empty(MCObjectHeap *);
NativeArrayResult S34PacketMaps_construct(S34PacketMaps *, int32_t mapId, int8_t scale,
                                          MCObject *visiblePlayers, NativeByteArray *colors,
                                          int32_t minX, int32_t minY, int32_t maxX, int32_t maxY);
NativeArrayResult S34PacketMaps_readPacketData(S34PacketMaps *, PacketBuffer *);
NativeArrayResult S34PacketMaps_writePacketData(S34PacketMaps *, PacketBuffer *);
NativeArrayResult S34PacketMaps_setMapdataTo(S34PacketMaps *, MapData *);
bool S34PacketMaps_processPacket(S34PacketMaps *, INetHandlerPlayClient);
int32_t S34PacketMaps_getMapId(const S34PacketMaps *);
/* Healthy Source exceptions preserve completed field/array/IO effects.
   Unknown native class, foreign ownership and OOM are explicit FAILURE.
   Concrete Vec4b/ItemStack dependencies and synchronous native IO are bounded;
   this is not a Java Throwable hierarchy or the complete client handler. */
#endif
