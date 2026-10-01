#ifndef C919_SOURCE_ITEM_IN_WORLD_MANAGER_H
#define C919_SOURCE_ITEM_IN_WORLD_MANAGER_H
#include "util/BlockPos.h"
#include "world/WorldSettingsGameType.h"

typedef struct EntityPlayerMP EntityPlayerMP;
typedef struct ItemInWorldManager {
    MCObject object;
    MCObject *theWorld;
    EntityPlayerMP *thisPlayerMP;
    const WorldSettingsGameType *gameType;
    bool isDestroyingBlock;
    int32_t initialDamage;
    BlockPos *field_180240_f;
    int32_t curblockDamage;
    bool receivedFinishDiggingPacket;
    BlockPos *field_180241_i;
    int32_t initialBlockDamage,durabilityRemainingOnBlock;
} ItemInWorldManager;
/* Allocation-only native seam. The source constructor below performs all
   original declaration initializers and the nullable World assignment. */
ItemInWorldManager *ItemInWorldManager_nativeAllocate(MCObjectHeap *);
bool ItemInWorldManager_construct(ItemInWorldManager *,MCObject *world);
ItemInWorldManager *ItemInWorldManager_new(MCObjectHeap *,MCObject *world);
bool ItemInWorldManager_isInstance(const MCObject *);
void ItemInWorldManager_traceFields(ItemInWorldManager *,MCObjectVisitor,void *context);
const WorldSettingsGameType *ItemInWorldManager_getGameType(ItemInWorldManager *);
bool ItemInWorldManager_isCreative(ItemInWorldManager *);
bool ItemInWorldManager_survivalOrAdventure(ItemInWorldManager *);
/* Source body scope: the complete constructor and these three getters only.
   World is a managed native provider reference. GameType identities are the
   existing immutable enum adapter. setGameType/initializeGameType, block
   destruction, use/harvest and their broadcast/physics dependencies are not
   declared as implemented. This is not the complete manager class. */
#endif
