#ifndef C919_SOURCE_STAT_CRAFTING_H
#define C919_SOURCE_STAT_CRAFTING_H
#include "stats/StatBase.h"
#include "item/ItemStack.h"
typedef struct { StatBase base; const Item *field_150960_a; } StatCrafting;
/* Explicit constructor identity adapter: original UTF16 prefix+suffix and
   Item field. Chat components, ObjectiveStat and criteria registrations are
   undeclared dependencies, not manufactured display/scoreboard successes. */
StatCrafting *StatCrafting_newIdentity(MCObjectHeap *,const NBTString *prefix,const NBTString *suffix,const Item *);
const Item *StatCrafting_func_150959_a(const StatCrafting *);
extern const MCObjectClass c919_statcrafting_class;
#endif
