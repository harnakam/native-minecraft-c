#ifndef C919_SOURCE_I_NET_HANDLER_PLAY_CLIENT_H
#define C919_SOURCE_I_NET_HANDLER_PLAY_CLIENT_H
#include "util/MCObjectHeap.h"
typedef struct S2EPacketCloseWindow S2EPacketCloseWindow;
typedef struct S2FPacketSetSlot S2FPacketSetSlot;
typedef struct S30PacketWindowItems S30PacketWindowItems;
typedef struct S32PacketConfirmTransaction S32PacketConfirmTransaction;
typedef struct S1CPacketEntityMetadata S1CPacketEntityMetadata;
typedef struct S39PacketPlayerAbilities S39PacketPlayerAbilities;
/* Native dispatch for this subset of the original client interface. */
typedef struct {
    bool (*handleCloseWindow)(MCObject *,S2EPacketCloseWindow *);
    bool (*handleSetSlot)(MCObject *,S2FPacketSetSlot *);
    bool (*handleWindowItems)(MCObject *,S30PacketWindowItems *);
    bool (*handleConfirmTransaction)(MCObject *,S32PacketConfirmTransaction *);
    bool (*handleEntityMetadata)(MCObject *,S1CPacketEntityMetadata *);
    bool (*handlePlayerAbilities)(MCObject *,S39PacketPlayerAbilities *);
} INetHandlerPlayClientMethods;
typedef struct {MCObject *instance;const INetHandlerPlayClientMethods *methods;} INetHandlerPlayClient;
#endif
