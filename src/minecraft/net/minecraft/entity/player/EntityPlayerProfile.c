#include "entity/player/EntityPlayer.h"
#include "util/MCGameplayPlayer.h"
#include "network/PacketBuffer.h"
#include "nbt/NBTInternal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int64_t signed_bits(uint64_t bits) {
    return bits<=INT64_MAX?(int64_t)bits:-1-(int64_t)(UINT64_MAX-bits);
}
NativeJavaUUID *EntityPlayer_getOfflineUUID(MCObjectHeap *heap,NBTString *username) {
    MCObjectRootScope scope={0};NativeJavaUUID *uuid=NULL;
    if (!MCObjectRootScope_begin(&scope,heap)) return NULL;
    if (username) {
        size_t size=MCObjectHeap_objectSize((MCObject *)username);
        if (!NBTString_isInstance((MCObject *)username) || username->object.heap!=heap || size<sizeof(*username) ||
            username->length>(size-sizeof(*username))/sizeof(uint16_t) || !MCObjectRootScope_pin(&scope,(MCObject *)username)) goto failed;
    }
    /* Source String concatenation preserves all UTF-16 units. Java appends
       the literal "null" for a null name, before String.getBytes(UTF_8). */
    static const char prefix[]="OfflinePlayer:";
    size_t nameLength=username?NBTString_length(username):4u;
    size_t prefixLength=sizeof(prefix)-1;
    if (nameLength>(size_t)INT32_MAX-prefixLength || nameLength>SIZE_MAX/sizeof(uint16_t)-prefixLength) goto failed;
    size_t length=prefixLength+nameLength;
    uint16_t *units=(uint16_t *)malloc(length*sizeof(*units));
    if (!units) goto failed;
    for (size_t i=0;i<prefixLength;i++) units[i]=(uint8_t)prefix[i];
    if (username) memcpy(units+prefixLength,NBTString_units(username),nameLength*sizeof(*units));
    else for (size_t i=0;i<nameLength;i++) units[prefixLength+i]=(uint8_t)"null"[i];
    NBTString *concatenated=NBTString_fromUTF16(heap,units,length);free(units);
    if (!concatenated) goto failed;
    NBTByteArrayStorage *bytes=PacketBuffer_nativeEncodeUTF8(heap,concatenated);
    if (!bytes) goto failed;
    uint8_t digest[16];
    if (!mc_name_uuid_from_bytes((const uint8_t *)NBTByteArrayStorage_data(bytes),(size_t)NBTByteArrayStorage_length(bytes),digest)) goto failed;
    uint64_t most=0,least=0;
    for (size_t i=0;i<8;i++) most=(most<<8)|digest[i];
    for (size_t i=8;i<16;i++) least=(least<<8)|digest[i];
    uuid=NativeJavaUUID_new(heap,signed_bits(most),signed_bits(least));
    MCObjectRootScope_end(&scope);return uuid;
failed:
    MCObjectHeap_fail(heap);MCObjectRootScope_end(&scope);return NULL;
}
NativeJavaUUID *EntityPlayer_getUUID(NativeGameProfile *profile) {
    MCObjectHeap *heap=profile?profile->object.heap:NULL;MCObjectRootScope scope={0};
    if (!MCObjectRootScope_begin(&scope,heap)) return NULL;
    if (!NativeGameProfile_isInstance((MCObject *)profile) || !MCObjectRootScope_pin(&scope,(MCObject *)profile)) {
        MCObjectHeap_fail(heap);MCObjectRootScope_end(&scope);return NULL;
    }
    NativeJavaUUID *uuid=NativeGameProfile_getId(profile);
    if (!uuid && !MCObjectHeap_failed(heap)) {
        NBTString *name=NativeGameProfile_getName(profile);
        if (!MCObjectHeap_failed(heap)) uuid=EntityPlayer_getOfflineUUID(heap,name);
    }
    MCObjectRootScope_end(&scope);return uuid;
}
NBTString *EntityPlayer_getName(MCGameplayPlayer *player) {
    MCObject *object=(MCObject *)player;
    if (!MCGameplayPlayer_isInstance(object)) {MCObjectHeap_fail(object?object->heap:NULL);return NULL;}
    NativeGameProfile *profile=player->gameProfile;
    if (MCObjectHeap_failed(object->heap) || !NativeGameProfile_isInstance((MCObject *)profile) ||
        profile->object.heap!=object->heap) {MCObjectHeap_fail(object->heap);return NULL;}
    return NativeGameProfile_getName(profile);
}
