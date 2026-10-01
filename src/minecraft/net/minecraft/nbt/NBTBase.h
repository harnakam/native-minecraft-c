#ifndef C919_NATIVE_NBT_BASE_H
#define C919_NATIVE_NBT_BASE_H
#include "util/MCObjectHeap.h"
#include "network/protocol.h"
#include "nbt/NBTSizeTracker.h"
#include "nbt/NBTString.h"
typedef struct NBTBase { MCObject object; int8_t type; } NBTBase;
typedef struct NBTTagCompound NBTTagCompound;
extern const char *const NBTBase_NBT_TYPES[12];
NBTBase *NBTBase_createNewByType(MCObjectHeap *heap,int8_t id);
int8_t NBTBase_getId(const NBTBase *tag);
bool NBTBase_hasNoTags(const NBTBase *tag);
/* Source copy allocates separately for each edge. Heap clone is the distinct,
   cycle-aware platform snapshot operation. Borrow results under RootScope. */
NBTBase *NBTBase_copy(MCObjectHeap *heap,const NBTBase *tag);
bool NBTBase_equals(const NBTBase *a,const NBTBase *b);
int32_t NBTBase_hashCode(const NBTBase *tag);
bool NBTBase_read(NBTBase *tag,mc_buf *input,int32_t depth,NBTSizeTracker *tracker);
bool NBTBase_write(NBTBase *tag,mc_buf *output);
/* Root/packet boundary: NULL compound is the End marker. Root names are not
   stored in native tag objects. A failed decode never replaces *output. */
bool NBTWire_decodeCompound(MCObjectHeap *heap,mc_buf *input,NBTSizeTracker *tracker,NBTTagCompound **output);
bool NBTWire_encodeCompound(mc_buf *output,NBTTagCompound *tag);
#endif
