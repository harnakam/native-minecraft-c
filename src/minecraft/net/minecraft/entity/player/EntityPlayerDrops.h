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
/* Original body; sleeping and the inherited sneaking flag are native field
   bindings until the complete EntityPlayer/DataWatcher hierarchy is ported. */
float EntityPlayer_getEyeHeight(const MCGameplayPlayer *);
/* The rest of EntityPlayer and inherited Entity remain separate ports. */
#endif
