#ifndef C919_PACKET_BUFFER_H
#define C919_PACKET_BUFFER_H
#include "network/protocol.h"
#include "item/ItemStack.h"
#include "nbt/NBTTagByteArray.h"
#include "util/NativeTypedObjectArray.h"
/* Ephemeral native ByteBuf/IO allocation view. This ports original item/NBT,
   String and VarInt methods, not the complete delegated Netty ByteBuf class. The view
   and mc_buf remain with their native owner and must not cross heap adoption. */
typedef struct { mc_buf *buffer; MCObjectHeap *heap; } PacketBuffer;
bool PacketBuffer_init(PacketBuffer *,MCObjectHeap *,mc_buf *);
bool PacketBuffer_writeNBTTagCompoundToBuffer(PacketBuffer *,NBTTagCompound *);
bool PacketBuffer_readNBTTagCompoundFromBuffer(PacketBuffer *,NBTTagCompound **output);
bool PacketBuffer_writeItemStackToBuffer(PacketBuffer *,ItemStack *);
bool PacketBuffer_readItemStackFromBuffer(PacketBuffer *,ItemStack **output);
/* Source String methods over native UTF-16 String storage and the explicit
   JDK UTF-8 charset dependency. Malformed input uses Java replacement chars;
   unpaired UTF-16 surrogates encode as the JDK replacement byte. */
bool PacketBuffer_writeString(PacketBuffer *,const NBTString *);
bool PacketBuffer_readStringFromBuffer(PacketBuffer *,int32_t maxLength,NBTString **output);
/* Shared native JDK8 String.getBytes(UTF_8) dependency. Fresh managed byte[];
   no packet String length limit or length prefix. Native heap/Java array-size
   bounds apply. Input/output remain borrowed until retained by the caller. */
NBTByteArrayStorage *PacketBuffer_nativeEncodeUTF8(MCObjectHeap *,const NBTString *);
bool PacketBuffer_readVarIntFromBuffer(PacketBuffer *,int32_t *output);
bool PacketBuffer_writeVarIntToBuffer(PacketBuffer *,int32_t value);
/* Source byte[] bodies over the native bulk IO leaf. A healthy negative-array,
   NULL or truncated-input exception retains the VarInt/IO prefix. */
NativeArrayResult PacketBuffer_readByteArray(PacketBuffer *, NativeByteArray **out);
NativeArrayResult PacketBuffer_writeByteArray(PacketBuffer *, NativeByteArray *);
/* Read failures leave *output untouched, with the buffer/heap error set.
   Successful output is borrowed until retained/rooted by the caller. Negative
   short item IDs are null; nonnegative unknown IDs create a null-Item stack.
   Writer failures retain partially appended source-order bytes. Packet callers
   discard failed packets. NBT graph traversal uses original direct references;
   storage readFromNBT (including skull normalization) is never invoked. */
#endif
