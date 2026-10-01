#ifndef C919_SOURCE_COMMAND_RESULT_STATS_H
#define C919_SOURCE_COMMAND_RESULT_STATS_H
#include "nbt/NBTString.h"
/* Constructor-only source subset. Both fields initially retain the same
   class-static empty String[5]. Command execution/NBT methods are unported. */
typedef struct CommandResultStringArray { MCObject object; NBTString *items[5]; } CommandResultStringArray;
typedef struct CommandResultStats { MCObject object; CommandResultStringArray *entitiesID,*objectives; } CommandResultStats;
CommandResultStats *CommandResultStats_new(MCObjectHeap *);
bool CommandResultStats_isInstance(const MCObject *);
bool CommandResultStringArray_isInstance(const MCObject *);
#endif
