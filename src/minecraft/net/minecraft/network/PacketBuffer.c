#include "network/PacketBuffer.h"
#include "nbt/NBTBase.h"
#include "nbt/NBTSizeTracker.h"
#include "nbt/NBTString.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

bool PacketBuffer_init(PacketBuffer *view,MCObjectHeap *heap,mc_buf *buffer) {
    if (!view || !heap || !buffer) return false;
    *view=(PacketBuffer){buffer,heap}; return true;
}
static bool begin(PacketBuffer *view,MCObjectRootScope *scope) {
    if (!view || !view->buffer || !view->heap) return false;
    if (view->buffer->failed || MCObjectHeap_failed(view->heap) || !MCObjectRootScope_begin(scope,view->heap)) {
        view->buffer->failed=true; return false;
    }
    return true;
}
static bool finish(PacketBuffer *view,MCObjectRootScope *scope,bool ok) {
    if (!ok || MCObjectHeap_failed(view->heap)) view->buffer->failed=true;
    MCObjectRootScope_end(scope); return !view->buffer->failed;
}
bool PacketBuffer_readVarIntFromBuffer(PacketBuffer *p, int32_t *output) {
    MCObjectRootScope scope = {0};
    if (!begin(p, &scope))
        return false;
    if (!output) {
        MCObjectHeap_fail(p->heap);
        return finish(p, &scope, false);
    }
    uint32_t value = 0;
    unsigned j = 0;
    bool ok = true;
    for (;;) {
        uint8_t byte = mc_get_u8(p->buffer);
        if (p->buffer->failed) {
            ok = false;
            break;
        }
        /* Original Java int shift/wrap, then the >5 check: byte six has
           already been consumed when the method raises VarInt-too-big. */
        value |= (uint32_t)(byte & 127u) << ((j++ * 7u) & 31u);
        if (j > 5) {
            ok = false;
            break;
        }
        if (!(byte & 128u))
            break;
    }
    ok = finish(p, &scope, ok);
    if (ok)
        memcpy(output, &value, sizeof(value));
    return ok;
}
bool PacketBuffer_writeVarIntToBuffer(PacketBuffer *p, int32_t number) {
    MCObjectRootScope scope = {0};
    if (!begin(p, &scope))
        return false;
    uint32_t value = (uint32_t)number;
    while ((value & ~127u) != 0 && !p->buffer->failed) {
        mc_put_u8(p->buffer, (uint8_t)((value & 127u) | 128u));
        value >>= 7;
    }
    if (!p->buffer->failed)
        mc_put_u8(p->buffer, (uint8_t)value);
    return finish(p, &scope, !p->buffer->failed);
}
bool PacketBuffer_writeNBTTagCompoundToBuffer(PacketBuffer *view, NBTTagCompound *tag) {
    MCObjectRootScope scope = {0};
    if (!begin(view, &scope))
        return false;
    bool ok =
        MCObjectRootScope_pin(&scope, (MCObject *)tag) && NBTWire_encodeCompound(view->buffer, tag);
    return finish(view, &scope, ok);
}
bool PacketBuffer_readNBTTagCompoundFromBuffer(PacketBuffer *view,NBTTagCompound **output) {
    MCObjectRootScope scope={0}; if (!begin(view,&scope)) return false;
    if (!output) { MCObjectHeap_fail(view->heap); return finish(view,&scope,false); }
    size_t reader_index=view->buffer->pos;
    uint8_t first=mc_get_u8(view->buffer); NBTTagCompound *decoded=NULL;
    bool ok=!view->buffer->failed;
    if (ok && first!=0) {
        view->buffer->pos=reader_index;
        NBTSizeTracker tracker; NBTSizeTracker_init(&tracker,INT64_C(2097152));
        ok=NBTWire_decodeCompound(view->heap,view->buffer,&tracker,&decoded);
    }
    ok=finish(view,&scope,ok);
    if (ok) *output=decoded;
    return ok;
}
/* Immutable registered Item facts: every 1.8.9 registry item inherits the
   original base getShareTag=true; no registered subclass overrides it. This
   is not a port/stub of arbitrary future Item subclasses. */
static bool registry_share_tag(const Item *item) { return item!=NULL; }
static int16_t narrow_short(int32_t value) { uint16_t bits=(uint16_t)value; int16_t out; memcpy(&out,&bits,sizeof(out)); return out; }
bool PacketBuffer_writeItemStackToBuffer(PacketBuffer *view,ItemStack *stack) {
    MCObjectRootScope scope={0}; if (!begin(view,&scope)) return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)stack);
    if (ok && !stack) mc_put_i16(view->buffer,-1);
    else if (ok) {
        mc_put_i16(view->buffer,narrow_short(ItemStack_registryId(ItemStack_getItem(stack))));
        mc_put_u8(view->buffer,(uint8_t)stack->stackSize);
        mc_put_i16(view->buffer,narrow_short(ItemStack_getMetadata(stack)));
        if (!view->buffer->failed) {
            if (!ItemStack_getItem(stack)) { MCObjectHeap_fail(view->heap); ok=false; }
            else {
                NBTTagCompound *tag=NULL;
                bool damageable=ItemStack_getMaxDamage(stack)>0 && !ItemStack_getHasSubtypes(stack);
                if (damageable || registry_share_tag(ItemStack_getItem(stack))) tag=ItemStack_getTagCompound(stack);
                ok=PacketBuffer_writeNBTTagCompoundToBuffer(view,tag);
            }
        }
    }
    return finish(view,&scope,ok);
}
bool PacketBuffer_readItemStackFromBuffer(PacketBuffer *view,ItemStack **output) {
    MCObjectRootScope scope={0}; if (!begin(view,&scope)) return false;
    if (!output) { MCObjectHeap_fail(view->heap); return finish(view,&scope,false); }
    ItemStack *stack=NULL; int16_t id=mc_get_i16(view->buffer); bool ok=!view->buffer->failed;
    if (ok && id>=0) {
        uint8_t bits=mc_get_u8(view->buffer); int8_t count; memcpy(&count,&bits,sizeof(count));
        int16_t metadata=mc_get_i16(view->buffer); ok=!view->buffer->failed;
        if (ok) {
            stack=ItemStack_new(view->heap,ItemStack_registryItem(id),count,metadata);
            NBTTagCompound *tag=NULL;
            ok=stack && PacketBuffer_readNBTTagCompoundFromBuffer(view,&tag) && ItemStack_setTagCompound(stack,tag);
        }
    }
    ok=finish(view,&scope,ok);
    if (ok) *output=stack;
    return ok;
}
/* Explicit Java8 UTF-8 charset adapter. Unpaired UTF-16 units encode as the
   CharsetEncoder replacement '?'; malformed byte sequences decode as U+FFFD,
   with the decoder's maximal valid prefix consumed before the bad byte. */
static uint32_t string_scalar(const uint16_t *units, size_t length, size_t *i) {
    uint32_t c = units[(*i)++];
    if (c >= 0xd800 && c <= 0xdbff) {
        if (*i < length && units[*i] >= 0xdc00 && units[*i] <= 0xdfff) {
            c = 0x10000 + ((c - 0xd800) << 10) + (units[(*i)++] - 0xdc00);
        } else
            c = '?';
    } else if (c >= 0xdc00 && c <= 0xdfff)
        c = '?';
    return c;
}
bool PacketBuffer_writeString(PacketBuffer *p, const NBTString *text) {
    MCObjectRootScope scope = {0};
    if (!begin(p, &scope))
        return false;
    bool ok = NBTString_isInstance((const MCObject *)text) &&
              MCObjectRootScope_pin(&scope, (MCObject *)text);
    if (!ok) {
        MCObjectHeap_fail(p->heap);
        return finish(p, &scope, false);
    }
    const uint16_t *units = NBTString_units(text);
    size_t length = NBTString_length(text), bytes = 0, i = 0;
    while (i < length) {
        uint32_t c = string_scalar(units, length, &i);
        bytes += c < 128 ? 1u : c < 2048 ? 2u : c < 65536 ? 3u : 4u;
        if (bytes > 32767)
            return finish(p, &scope, false);
    }
    if (!PacketBuffer_writeVarIntToBuffer(p, (int32_t)bytes))
        return finish(p, &scope, false);
    i = 0;
    while (i < length && !p->buffer->failed) {
        uint32_t c = string_scalar(units, length, &i);
        if (c < 128)
            mc_put_u8(p->buffer, (uint8_t)c);
        else if (c < 2048) {
            mc_put_u8(p->buffer, (uint8_t)(0xc0 | (c >> 6)));
            mc_put_u8(p->buffer, (uint8_t)(0x80 | (c & 63)));
        } else if (c < 65536) {
            mc_put_u8(p->buffer, (uint8_t)(0xe0 | (c >> 12)));
            mc_put_u8(p->buffer, (uint8_t)(0x80 | ((c >> 6) & 63)));
            mc_put_u8(p->buffer, (uint8_t)(0x80 | (c & 63)));
        } else {
            mc_put_u8(p->buffer, (uint8_t)(0xf0 | (c >> 18)));
            mc_put_u8(p->buffer, (uint8_t)(0x80 | ((c >> 12) & 63)));
            mc_put_u8(p->buffer, (uint8_t)(0x80 | ((c >> 6) & 63)));
            mc_put_u8(p->buffer, (uint8_t)(0x80 | (c & 63)));
        }
    }
    return finish(p, &scope, !p->buffer->failed);
}
static bool continuation(uint8_t b) { return (b & 0xc0u) == 0x80u; }
static uint32_t decode_utf8(const uint8_t *bytes, size_t size, size_t *i) {
    size_t start = *i;
    uint32_t a = bytes[(*i)++];
    if (a < 128)
        return a;
    if (a >= 0xc2 && a <= 0xdf) {
        if (*i < size && continuation(bytes[*i])) {
            uint32_t b = bytes[(*i)++];
            return ((a & 31) << 6) | (b & 63);
        }
        return 0xfffd;
    }
    if (a >= 0xe0 && a <= 0xef) {
        if (*i == size)
            return 0xfffd;
        uint32_t b = bytes[*i];
        if (!continuation((uint8_t)b) || (a == 0xe0 && b < 0xa0))
            return 0xfffd;
        (*i)++;
        if (*i == size || !continuation(bytes[*i]))
            return 0xfffd;
        uint32_t c = bytes[(*i)++];
        uint32_t value = ((a & 15) << 12) | ((b & 63) << 6) | (c & 63);
        return value >= 0xd800 && value <= 0xdfff ? 0xfffd : value;
    }
    if (a >= 0xf0 && a <= 0xf4) {
        if (*i == size)
            return 0xfffd;
        uint32_t b = bytes[*i];
        if (!continuation((uint8_t)b) || (a == 0xf0 && b < 0x90) || (a == 0xf4 && b > 0x8f))
            return 0xfffd;
        (*i)++;
        if (*i == size || !continuation(bytes[*i]))
            return 0xfffd;
        uint32_t c = bytes[(*i)++];
        if (*i == size || !continuation(bytes[*i]))
            return 0xfffd;
        uint32_t d = bytes[(*i)++];
        return ((a & 7) << 18) | ((b & 63) << 12) | ((c & 63) << 6) | (d & 63);
    }
    *i = start + 1;
    return 0xfffd;
}
bool PacketBuffer_readStringFromBuffer(PacketBuffer *p, int32_t maxLength, NBTString **output) {
    MCObjectRootScope scope = {0};
    if (!begin(p, &scope))
        return false;
    if (!output) {
        MCObjectHeap_fail(p->heap);
        return finish(p, &scope, false);
    }
    int32_t encoded = 0;
    if (!PacketBuffer_readVarIntFromBuffer(p, &encoded))
        return finish(p, &scope, false);
    uint32_t limitBits = (uint32_t)maxLength * 4u;
    int32_t limit;
    memcpy(&limit, &limitBits, sizeof(limit));
    if (p->buffer->failed || encoded > limit || encoded < 0)
        return finish(p, &scope, false);
    size_t n = (size_t)encoded;
    if (n > MC_MAX_PACKET || n > SIZE_MAX / sizeof(uint16_t))
        return finish(p, &scope, false);
    uint8_t *bytes = n ? malloc(n) : NULL;
    uint16_t *units = n ? malloc(n * sizeof(*units)) : NULL;
    if (n && (!bytes || !units)) {
        free(bytes);
        free(units);
        MCObjectHeap_fail(p->heap);
        return finish(p, &scope, false);
    }
    bool ok = mc_get_bytes(p->buffer, bytes, n);
    size_t i = 0, length = 0;
    while (ok && i < n) {
        uint32_t c = decode_utf8(bytes, n, &i);
        if (c < 65536)
            units[length++] = (uint16_t)c;
        else {
            c -= 65536;
            units[length++] = (uint16_t)(0xd800 + (c >> 10));
            units[length++] = (uint16_t)(0xdc00 + (c & 1023));
        }
    }
    NBTString *decoded = NULL;
    if (ok) {
        if (maxLength < 0 || length > (size_t)maxLength)
            ok = false;
        else {
            decoded = NBTString_fromUTF16(p->heap, units, length);
            ok = decoded != NULL;
        }
    }
    free(bytes);
    free(units);
    ok = finish(p, &scope, ok);
    if (ok)
        *output = decoded;
    return ok;
}
