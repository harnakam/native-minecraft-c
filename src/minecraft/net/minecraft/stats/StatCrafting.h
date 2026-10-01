#ifndef C919_SOURCE_STAT_CRAFTING_H
#define C919_SOURCE_STAT_CRAFTING_H
#include "stats/StatBase.h"
#include "item/ItemStack.h"
typedef struct { StatBase base; const Item *field_150960_a; } StatCrafting;
/* Explicit constructor identity adapter: original UTF16 prefix+suffix and
   Item field and actual base ObjectiveStat/criteria registrations. Chat
   components, formatter/static initialization and full original overloaded
   constructors remain separate dependencies. */
StatCrafting *StatCrafting_newIdentity(MCObjectHeap *,const NBTString *prefix,const NBTString *suffix,const Item *);
const Item *StatCrafting_func_150959_a(const StatCrafting *);
extern const MCObjectClass c919_statcrafting_class;
#endif
