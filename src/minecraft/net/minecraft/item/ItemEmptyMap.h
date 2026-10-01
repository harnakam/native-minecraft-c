#ifndef C919_SOURCE_ITEM_EMPTY_MAP_H
#define C919_SOURCE_ITEM_EMPTY_MAP_H
#include "util/MCGameplayPlayer.h"
#include "entity/item/EntityItem.h"
#include "stats/StatBase.h"

/* ItemEmptyMap.onItemRightClick over the canonical native World/player owners.
   self is the immutable Item registry identity adapter, not a translated Item
   subclass instance. The creative-tab constructor and ItemMapBase inheritance
   remain separate dependencies. This override has no remote/creative branch. */
typedef struct ItemEmptyMapDependencies {
    StatBase *(*objectUseStat)(MCObject *context,const Item *self);
    bool (*triggerAchievement)(MCObject *context,MCGameplayPlayer *,StatBase *);
    /* true with *result==NULL preserves the source's nullable ignored return.
       false means native effect failure and fails the working heap. */
    bool (*dropPlayerItemWithRandomChoice)(MCObject *context,MCGameplayPlayer *,
        ItemStack *exactStack,bool unused,EntityItem **result);
} ItemEmptyMapDependencies;
ItemStack *ItemEmptyMap_onItemRightClick(const Item *self,ItemStack *,MCGameplayWorld *,
    MCGameplayPlayer *,const ItemEmptyMapDependencies *,MCObject *context);
/* Required virtual/stat dependencies are invoked only on their original
   branch. NULL stat is a valid original lookup, not a missing dependency.
   Failures retain original partial mutation order; discard the entire working
   graph. The returned reference is borrowed until retained by its real owner. */
#endif
