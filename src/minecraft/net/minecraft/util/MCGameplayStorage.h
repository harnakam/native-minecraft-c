#ifndef C919_NATIVE_GAMEPLAY_STORAGE_H
#define C919_NATIVE_GAMEPLAY_STORAGE_H
#include "util/MCGameplayPlayer.h"
#include "entity/item/EntityItem.h"

/* Native journal/file envelope, not a complete translation of Entity or
   EntityPlayer persistence. Original InventoryPlayer/ItemStack/EntityItem NBT
   methods operate directly on the canonical graph. Encoded mc_nbt values exist
   only at this storage boundary, with the existing 2 MiB per-file limit.
   StatFileWriter counters belong to the separate original StatisticsFile JSON
   dependency; these NBT encoders do not invent a player-NBT statistics format. */
const MCGameplayEncoders *MCGameplayStorage_encoders(void);
bool MCGameplayStorage_encodePlayer(const MCGameplayObjects *,size_t index,mc_nbt *,void *context);
bool MCGameplayStorage_encodeItems(const MCGameplayObjects *,mc_nbt *,void *context);
bool MCGameplayStorage_encodeMaps(const MCGameplayObjects *,mc_nbt *,void *context);
/* Load only into a caller's disposable working graph. Original NBT reads and
   callbacks preserve source-order partial mutations on failure; discard the
   entire working graph and never adopt a partially loaded state. All methods
   establish their own RootScope. File/journal recovery is the caller's job.
   A saved C919Workbench creates a recovery-only ContainerWorkbench with NULL
   position. Its original close must run before any play or durable adoption.
   Save/read reconstruct each occurrence; no persisted alias IDs are invented.
   Unsupported ItemStack profile normalization fails explicitly.
   The native item envelope restores finite Pos/Motion, source |Motion|>10
   clamping and the required inherited setPosition dependency before/after the
   source subclass NBT read. Complete Entity.readFromNBT remains unported;
   unknown inherited fields are retained unchanged as envelope metadata. */
bool MCGameplayStorage_loadPlayer(MCGameplayPlayer *,const mc_nbt *,const mc_crafting_dispatch *);
bool MCGameplayStorage_loadItems(MCGameplayWorld *,const mc_nbt *,MCObject *entityContext,
    const EntityItemDependencies *,const EntityItemConstructorDependencies *);
bool MCGameplayStorage_loadMaps(MCGameplayWorld *,const mc_nbt *);
#endif
