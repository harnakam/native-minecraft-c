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
/* Include optional MapData in the same commit. Version-1 journals and the
   two-file APIs remain supported; NULL maps leave the existing map file alone. */
bool mc_transfer_prepare_all(const char *base, const char *uuid, const mc_nbt *player,
    const mc_nbt *items, const mc_nbt *maps, bool *committed, char *error, size_t error_size);
bool mc_transfer_commit_all(const char *base, const char *uuid, const mc_nbt *player,
    const mc_nbt *items, const mc_nbt *maps, bool *committed, char *error, size_t error_size);
/* Version 3 extends the same commit point to every affected player. UUIDs and
   snapshots are borrowed through the synchronous call; canonical UUIDs must
   be unique. Items are required, maps are optional, and each snapshot is at
   most 2MiB. Recovery validates all snapshots before writing any destination.
   Staging paths are derived from base and UUID; the journal accepts no paths.
   The same single-writer and committed-failure rules above apply. */
#define MC_TRANSFER_MAX_PLAYERS 64u
typedef struct { const char *uuid; const mc_nbt *snapshot; } mc_transfer_player;
bool mc_transfer_prepare_group(const char *base, const mc_transfer_player *players,
    size_t player_count, const mc_nbt *items, const mc_nbt *maps,
    bool *committed, char *error, size_t error_size);
bool mc_transfer_commit_group(const char *base, const mc_transfer_player *players,
    size_t player_count, const mc_nbt *items, const mc_nbt *maps,
    bool *committed, char *error, size_t error_size);
#endif
