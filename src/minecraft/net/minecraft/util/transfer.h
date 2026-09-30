#ifndef C919_TRANSFER_H
#define C919_TRANSFER_H
#include "nbt/nbt.h"
/* A recoverable transfer between one player file and the world's item file.
   base is the world save path. uuid is a canonical offline UUID, or NULL for
   an item-only update. All snapshots are borrowed, validated named roots.
   prepare durably records the intended replacements before either target is
   changed. recover finishes a recorded transfer; it is also safe at startup.
   Only one transfer may be pending per world. */
bool mc_transfer_prepare(const char *base, const char *uuid, const mc_nbt *player,
                         const mc_nbt *items, bool *committed, char *error, size_t error_size);
bool mc_transfer_recover(const char *base, char *error, size_t error_size);
/* committed distinguishes preparation failure from a checkpoint failure after
   the journal's commit point. A committed failure must stop further mutations
   and retain the journal for recovery; it must never be rolled back in memory. */
bool mc_transfer_commit(const char *base, const char *uuid, const mc_nbt *player,
                        const mc_nbt *items, bool *committed, char *error, size_t error_size);
#endif
