#include "util/NativeJavaUUID.h"
#include "nbt/NBTInternal.h"
#include <limits.h>

static const MCObjectClass uuidClass={"native.JavaUUID",MCObjectHeap_plainClone,NULL,NULL};
NativeJavaUUID *NativeJavaUUID_new(MCObjectHeap *heap,int64_t most,int64_t least) {
    NativeJavaUUID *uuid=(NativeJavaUUID*)MCObjectHeap_alloc(heap,sizeof(*uuid),&uuidClass);
    if (uuid) {
        uuid->mostSignificantBits=most;
        uuid->leastSignificantBits=least;
    }
    return uuid;
}
bool NativeJavaUUID_isInstance(const MCObject *object) {
    return object && object->klass==&uuidClass && MCObjectHeap_objectSize(object)>=sizeof(NativeJavaUUID);
}
/* UTF-16 char digit facts observed against Java 8 Character.digit(c,16).
   Surrogate pairs are not combined: Long's character parsing uses char units. */
static int hexadecimal(uint16_t unit) {
    static const uint16_t decimals[]={0x0030,0x0660,0x06f0,0x07c0,0x0966,0x09e6,
        0x0a66,0x0ae6,0x0b66,0x0be6,0x0c66,0x0ce6,0x0d66,0x0e50,0x0ed0,0x0f20,
        0x1040,0x1090,0x17e0,0x1810,0x1946,0x19d0,0x1a80,0x1a90,0x1b50,0x1bb0,
        0x1c40,0x1c50,0xa620,0xa8d0,0xa900,0xa9d0,0xaa50,0xabf0,0xff10};
    if (unit>='A' && unit<='F') return unit-'A'+10;
    if (unit>='a' && unit<='f') return unit-'a'+10;
    if (unit>=0xff21 && unit<=0xff26) return unit-0xff21+10;
    if (unit>=0xff41 && unit<=0xff46) return unit-0xff41+10;
    for (size_t i=0;i<sizeof(decimals)/sizeof(*decimals);i++)
        if (unit>=decimals[i] && (unsigned)(unit-decimals[i])<10) return unit-decimals[i];
    return -1;
}
static bool component(const uint16_t *units,size_t begin,size_t end,uint64_t *value) {
    if (begin==end) return false;
    uint64_t result=0;
    for (size_t i=begin;i<end;i++) {
        int digit=hexadecimal(units[i]);
        if (digit<0 || result>((uint64_t)INT64_MAX-(unsigned)digit)/16u) return false;
        result=result*16u+(unsigned)digit;
    }
    *value=result; return true;
}
static int64_t signed_bits(uint64_t value) {
    return value<=INT64_MAX ? (int64_t)value : -1-(int64_t)(UINT64_MAX-value);
}
NativeJavaUUID *NativeJavaUUID_fromString(MCObjectHeap *heap,const NBTString *text) {
    if (!heap || !NBTString_isInstance((const MCObject*)text) || text->object.heap!=heap ||
        MCObjectHeap_objectSize((const MCObject*)text)<sizeof(*text)) {
        MCObjectHeap_fail(heap); return NULL;
    }
    MCObjectRootScope scope={0};
    if (!MCObjectRootScope_begin(&scope,heap) || !MCObjectRootScope_pin(&scope,(MCObject*)text)) {
        MCObjectHeap_fail(heap); MCObjectRootScope_end(&scope); return NULL;
    }
    NativeJavaUUID *uuid=NULL;
    size_t size=MCObjectHeap_objectSize((const MCObject*)text);
    size_t length=NBTString_length(text);
    if (length>(size-sizeof(*text))/sizeof(uint16_t)) goto invalid;
    const uint16_t *units=NBTString_units(text);
    /* Java String.split("-") discards trailing empty components. It does not
       require canonical widths; each component is a positive signed long. */
    while (length && units[length-1]=='-') --length;
    uint64_t fields[5]; size_t start=0,count=0;
    for (size_t end=0;end<=length;end++) {
        if (end!=length && units[end]!='-') continue;
        if (count==5 || !component(units,start,end,&fields[count])) goto invalid;
        ++count; start=end+1;
    }
    if (count!=5) goto invalid;
    uint64_t most=((fields[0]<<16)|fields[1])<<16;
    most|=fields[2];
    uint64_t least=(fields[3]<<48)|fields[4];
    uuid=NativeJavaUUID_new(heap,signed_bits(most),signed_bits(least));
    MCObjectRootScope_end(&scope); return uuid;
invalid:
    MCObjectHeap_fail(heap); MCObjectRootScope_end(&scope); return NULL;
}
