#ifndef C919_FOOD_STATS_H
#define C919_FOOD_STATS_H
#include "item/ItemStack.h"
typedef struct FoodStats {
    MCObject object;
    int32_t foodLevel;
    float foodSaturationLevel,foodExhaustionLevel;
    int32_t foodTimer,prevFoodLevel;
} FoodStats;
/* Required source dependencies. Each bool reports native completion; an
   ordinary false shouldHeal/rule/attack result is returned through its output.
   Difficulty IDs are the canonical PEACEFUL=0/EASY=1/NORMAL=2/HARD=3 facts;
   -1 represents a source NULL enum reference (all identity comparisons false).
   getWorld models the source field read and is reevaluated at each source site.
   The callbacks/context are borrowed only for this invocation. */
typedef struct {
    MCObject *(*getWorld)(MCObject *,MCObject *player);
    bool (*getDifficulty)(MCObject *,MCObject *world,int32_t *);
    MCObject *(*getGameRules)(MCObject *,MCObject *world);
    bool (*getBoolean)(MCObject *,MCObject *rules,const char *key,bool *);
    bool (*shouldHeal)(MCObject *,MCObject *player,bool *);
    bool (*heal)(MCObject *,MCObject *player,float);
    bool (*getHealth)(MCObject *,MCObject *player,float *);
    bool (*attackStarve)(MCObject *,MCObject *player,float,bool *accepted);
} FoodStatsPlayerDependencies;
typedef struct {
    bool (*getHealAmount)(MCObject *,const Item *,ItemStack *,int32_t *);
    bool (*getSaturationModifier)(MCObject *,const Item *,ItemStack *,float *);
} FoodStatsItemDependencies;
FoodStats *FoodStats_new(MCObjectHeap *);
bool FoodStats_isInstance(const MCObject *);
bool FoodStats_addStats(FoodStats *,int32_t,float);
bool FoodStats_addStats_item(FoodStats *,const Item *,ItemStack *,const FoodStatsItemDependencies *,MCObject *);
bool FoodStats_onUpdate(FoodStats *,MCObject *player,const FoodStatsPlayerDependencies *,MCObject *context);
bool FoodStats_readNBT(FoodStats *,NBTTagCompound *);
bool FoodStats_writeNBT(FoodStats *,NBTTagCompound *);
int32_t FoodStats_getFoodLevel(const FoodStats *);
int32_t FoodStats_getPrevFoodLevel(const FoodStats *);
bool FoodStats_needFood(const FoodStats *);
bool FoodStats_addExhaustion(FoodStats *,float);
float FoodStats_getSaturationLevel(const FoodStats *);
void FoodStats_setFoodLevel(FoodStats *,int32_t);
void FoodStats_setFoodSaturationLevel(FoodStats *,float);
/* Complete source bodies; ItemFood/EntityPlayer/World/GameRules/DamageSource
   and Java exception classes remain explicit native dispatch dependencies. */
#endif
