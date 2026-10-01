#ifndef C919_SOURCE_ENTITY_PLAYER_DROPS_H
#define C919_SOURCE_ENTITY_PLAYER_DROPS_H
#include "util/MCGameplayPlayer.h"
#include "entity/item/EntityItem.h"

/* Source drop methods on the native actor owner. These required dependencies
   are real virtual method/Random/World/stat/JDK math calls, never default
   empty-success hooks. Context is managed and borrowed for the whole call.
   joinEntityItemWithWorld completes even if World ordinarily rejects spawn;
   false means dependency failure, distinct from that ignored source bool. */
typedef struct {
    const EntityItemDependencies *entity;
    const EntityItemConstructorDependencies *constructor;
    float (*getEyeHeight)(MCObject *context,MCGameplayPlayer *);
    float (*nextFloat)(MCObject *context,MCGameplayPlayer *);
    NBTString *(*getName)(MCObject *context,MCGameplayPlayer *);
    bool (*joinEntityItemWithWorld)(MCObject *context,MCGameplayPlayer *,EntityItem *);
    bool (*triggerDropStat)(MCObject *context,MCGameplayPlayer *);
    double (*mathSin)(MCObject *context,double value);
    double (*mathCos)(MCObject *context,double value);
} EntityPlayerDropsDependencies;
EntityItem *EntityPlayer_dropPlayerItemWithRandomChoice(MCGameplayPlayer *,ItemStack *,bool unused,
    const EntityPlayerDropsDependencies *,MCObject *context);
EntityItem *EntityPlayer_dropItem(MCGameplayPlayer *,ItemStack *,bool dropAround,bool traceItem,
    const EntityPlayerDropsDependencies *,MCObject *context);
EntityItem *EntityPlayer_dropOneItem(MCGameplayPlayer *,bool dropAll,
    const EntityPlayerDropsDependencies *,MCObject *context);
/* Original getEyeHeight body invokes these virtual predicates in source order.
   False callback completion models an exception; the bool output is the actual
   predicate result. Context is a traced caller-owned same-heap reference (or
   NULL for stateless dependencies), borrowed with the exact captured receiver. */
typedef struct {
    bool (*isPlayerSleeping)(MCObject *context,MCGameplayPlayer *,bool *out);
    bool (*isSneaking)(MCObject *context,MCGameplayPlayer *,bool *out);
} EntityPlayerEyeHeightDependencies;
float EntityPlayer_getEyeHeightWithDispatch(MCGameplayPlayer *,MCObject *context,
    const EntityPlayerEyeHeightDependencies *);
/* Native concrete-base receiver adapter: Player.sleeping getter and inherited
   Entity.isSneaking. A subtype such as SP must supply its actual override to
   WithDispatch; this convenience function does not perform generic dispatch. */
float EntityPlayer_getEyeHeight(const MCGameplayPlayer *);
/* Other EntityPlayer methods remain separate ports. */
#endif
