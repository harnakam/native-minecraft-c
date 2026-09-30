#ifndef C919_NBT_H
#define C919_NBT_H
#include "network/protocol.h"

#define MC_NBT_MAX_DEPTH 64
#define MC_NBT_MAX_BYTES MC_MAX_PACKET
/* An owned, validated, named NBT root. Empty metadata uses data=NULL,size=0. */
typedef struct { uint8_t *data; size_t size; } mc_nbt;
/* A borrowed payload view; valid while its owning mc_nbt remains alive. */
typedef struct { uint8_t type; const uint8_t *data; size_t size; } mc_nbt_view;
void mc_nbt_init(mc_nbt *nbt);
void mc_nbt_free(mc_nbt *nbt);
bool mc_nbt_copy(mc_nbt *destination, const mc_nbt *source);
bool mc_nbt_equal(const mc_nbt *a, const mc_nbt *b);
bool mc_nbt_read(mc_buf *input, mc_nbt *output);
bool mc_nbt_write(mc_buf *output, const mc_nbt *nbt);
bool mc_nbt_validate(const void *data, size_t size);
bool mc_nbt_root(const mc_nbt *nbt, mc_nbt_view *view);
bool mc_nbt_find(const mc_nbt_view *compound, const char *name, mc_nbt_view *view);
bool mc_nbt_list_get(const mc_nbt_view *list, size_t index, mc_nbt_view *view);
bool mc_nbt_get_integer(const mc_nbt_view *view, int64_t *value);
bool mc_nbt_get_number(const mc_nbt_view *view, double *value);
/* Converts Java modified UTF-8, including paired UTF-16 surrogates, to UTF-8.
   Embedded NUL or an unpaired surrogate fails with output unchanged; its raw
   NBT bytes are still preserved by the codec. */
bool mc_nbt_get_string(const mc_nbt_view *view, char *output, size_t capacity);
bool mc_nbt_load_gzip(mc_nbt *output, const char *path, char *error, size_t error_size);
bool mc_nbt_save_gzip(const mc_nbt *nbt, const char *path, char *error, size_t error_size);
#endif
