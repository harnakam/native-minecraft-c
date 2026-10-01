#ifndef C919_SOURCE_S1C_PACKET_ENTITY_METADATA_H
#define C919_SOURCE_S1C_PACKET_ENTITY_METADATA_H
#include "entity/DataWatcher.h"
#include "network/play/INetHandlerPlayClient.h"
struct S1CPacketEntityMetadata {
    MCObject object;
    int32_t entityId;
    WatchableObjectList *field_149378_b;
};
S1CPacketEntityMetadata *S1CPacketEntityMetadata_new_empty(MCObjectHeap *);
S1CPacketEntityMetadata *S1CPacketEntityMetadata_new(MCObjectHeap *, int32_t, DataWatcher *,
                                                     bool all);
bool S1CPacketEntityMetadata_readPacketData(S1CPacketEntityMetadata *, PacketBuffer *);
bool S1CPacketEntityMetadata_writePacketData(S1CPacketEntityMetadata *, PacketBuffer *);
bool S1CPacketEntityMetadata_processPacket(S1CPacketEntityMetadata *, INetHandlerPlayClient);
WatchableObjectList *S1CPacketEntityMetadata_func_149376_c(const S1CPacketEntityMetadata *);
int32_t S1CPacketEntityMetadata_getEntityId(const S1CPacketEntityMetadata *);
#endif
