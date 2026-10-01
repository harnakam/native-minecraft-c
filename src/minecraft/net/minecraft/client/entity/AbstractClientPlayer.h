#ifndef C919_SOURCE_ABSTRACT_CLIENT_PLAYER_H
#define C919_SOURCE_ABSTRACT_CLIENT_PLAYER_H
#include "entity/player/EntityPlayer.h"

/* Source inheritance has one authoritative first-member Player. NetworkPlayerInfo
   is a nullable managed dependency; its skin/network methods are separate ports. */
typedef struct AbstractClientPlayer {
    EntityPlayer player;
    MCObject *playerInfo;
} AbstractClientPlayer;
bool AbstractClientPlayer_isInstance(const MCObject *);
void AbstractClientPlayer_traceFields(AbstractClientPlayer *,MCObjectVisitor,void *context);
bool AbstractClientPlayer_construct(AbstractClientPlayer *,MCObject *world,NativeGameProfile *,
    const EntityPlayerDependencies *,const mc_crafting_dispatch *,MCObject *context,
    NativeJavaRandomRuntime *,NativeEntityIDRuntime *);
#endif
