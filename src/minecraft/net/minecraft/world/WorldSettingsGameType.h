#ifndef C919_SOURCE_WORLD_SETTINGS_GAME_TYPE_H
#define C919_SOURCE_WORLD_SETTINGS_GAME_TYPE_H
#include "entity/player/PlayerCapabilities.h"

/* Immutable native canonical enum identities. The supplied GameType method
   bodies below are translated; WorldSettings, java.lang.Enum, generated
   values/valueOf and Java enum construction are separate dependencies.
   No per-call GameType allocation or independently authoritative creative
   flag is introduced. Native callers retain these process-lifetime pointers. */
typedef struct WorldSettingsGameType {
    int32_t id;
    const char *name;
} WorldSettingsGameType;
extern const WorldSettingsGameType WorldSettingsGameType_NOT_SET;
extern const WorldSettingsGameType WorldSettingsGameType_SURVIVAL;
extern const WorldSettingsGameType WorldSettingsGameType_CREATIVE;
extern const WorldSettingsGameType WorldSettingsGameType_ADVENTURE;
extern const WorldSettingsGameType WorldSettingsGameType_SPECTATOR;
bool WorldSettingsGameType_isCanonical(const WorldSettingsGameType *);
/* Getter/predicate receivers must be canonical. Native invalid receivers have
   no heap to fail and return -1/false; validate before accepting native input.
   getName/configure instead fail their explicit/owner heap on invalid input. */
int32_t WorldSettingsGameType_getID(const WorldSettingsGameType *);
NBTString *WorldSettingsGameType_getName(MCObjectHeap *, const WorldSettingsGameType *);
bool WorldSettingsGameType_isAdventure(const WorldSettingsGameType *);
bool WorldSettingsGameType_isCreative(const WorldSettingsGameType *);
bool WorldSettingsGameType_isSurvivalOrAdventure(const WorldSettingsGameType *);
const WorldSettingsGameType *WorldSettingsGameType_getByID(int32_t);
/* The original name.equals argument permits NULL and falls back to SURVIVAL.
   Matching is exact UTF-16 equality, with no case folding or UTF-8 conversion. */
const WorldSettingsGameType *WorldSettingsGameType_getByName(const NBTString *);
bool WorldSettingsGameType_configurePlayerCapabilities(const WorldSettingsGameType *,
                                                     PlayerCapabilities *);
#endif
