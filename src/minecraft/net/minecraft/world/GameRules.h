#ifndef C919_SOURCE_GAME_RULES_H
#define C919_SOURCE_GAME_RULES_H
#include "util/NativeSortedStringMap.h"
#include "nbt/NBTTagCompound.h"
/* Immutable ordinal identity facts; inherited/generated Java enum APIs are
   not claimed. NULL type is retained just like the source constructor arg. */
typedef struct {int32_t ordinal;} GameRulesValueType;
extern const GameRulesValueType GameRules_ANY_VALUE,GameRules_BOOLEAN_VALUE,GameRules_NUMERICAL_VALUE;
typedef struct GameRulesValue {
    MCObject object;
    NBTString *valueString;
    bool valueBoolean;
    int32_t valueInteger;
    double valueDouble;
    const GameRulesValueType *type;
} GameRulesValue;
typedef struct GameRules {
    MCObject object;
    NativeSortedStringMap *theGameRules;
} GameRules;
typedef struct GameRulesStringArray {MCObject object;int32_t length;NBTString *items[];} GameRulesStringArray;
bool GameRules_isInstance(const MCObject *);
bool GameRulesValue_isInstance(const MCObject *);
GameRules *GameRules_nativeAllocate(MCObjectHeap *);
bool GameRules_construct(GameRules *);
GameRules *GameRules_new(MCObjectHeap *);
GameRulesValue *GameRulesValue_new(MCObjectHeap *,NBTString *,const GameRulesValueType *);
bool GameRulesValue_setValue(GameRulesValue *,NBTString *);
NBTString *GameRulesValue_getString(GameRulesValue *);
bool GameRulesValue_getBoolean(GameRulesValue *);
int32_t GameRulesValue_getInt(GameRulesValue *);
const GameRulesValueType *GameRulesValue_getType(GameRulesValue *);
bool GameRules_addGameRule(GameRules *,NBTString *,NBTString *,const GameRulesValueType *);
bool GameRules_setOrCreateGameRule(GameRules *,NBTString *,NBTString *);
NBTString *GameRules_getString(GameRules *,NBTString *);
bool GameRules_getBoolean(GameRules *,NBTString *);
int32_t GameRules_getInt(GameRules *,NBTString *);
NBTTagCompound *GameRules_writeToNBT(GameRules *);
bool GameRules_readFromNBT(GameRules *,NBTTagCompound *);
GameRulesStringArray *GameRules_getRules(GameRules *);
bool GameRules_hasRule(GameRules *,NBTString *);
bool GameRules_areSameType(GameRules *,NBTString *,const GameRulesValueType *);
#endif
