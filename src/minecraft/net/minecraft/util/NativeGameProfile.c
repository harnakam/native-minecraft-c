#include "util/NativeGameProfile.h"
#include "nbt/NBTInternal.h"

static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    NativeGameProfile *profile=(NativeGameProfile *)object;
    profile->id=(NativeJavaUUID *)visitor((MCObject *)profile->id,context);
    profile->name=(NBTString *)visitor((MCObject *)profile->name,context);
}
static const MCObjectClass profileClass={"native.authlib.GameProfile",MCObjectHeap_plainClone,trace,NULL};
bool NativeGameProfile_isInstance(const MCObject *object) {
    return object && object->klass==&profileClass && MCObjectHeap_objectSize(object)>=sizeof(NativeGameProfile);
}
/* Constructor-reachable StringUtils.isBlank char facts for the target Java 8.
   This is a native authlib view, not its PropertyMap/authentication constructor. */
static bool whitespace(uint16_t unit) {
    return (unit>=9 && unit<=13) || (unit>=0x1c && unit<=0x20) || unit==0x1680 ||
        unit==0x180e || (unit>=0x2000 && unit<=0x2006) || (unit>=0x2008 && unit<=0x200a) ||
        unit==0x2028 || unit==0x2029 || unit==0x205f || unit==0x3000;
}
static bool valid_string(MCObjectHeap *heap,const NBTString *text) {
    if (!text) return true;
    size_t size=MCObjectHeap_objectSize((const MCObject *)text);
    if (!NBTString_isInstance((const MCObject *)text) || text->object.heap!=heap ||
        size<sizeof(*text) || NBTString_length(text)>(size-sizeof(*text))/sizeof(uint16_t)) {
        MCObjectHeap_fail(heap);return false;
    }
    return true;
}
NativeGameProfile *NativeGameProfile_new(MCObjectHeap *heap,NativeJavaUUID *id,NBTString *name) {
    MCObjectRootScope scope={0};NativeGameProfile *profile=NULL;
    if (!MCObjectRootScope_begin(&scope,heap)) return NULL;
    if ((id && (!NativeJavaUUID_isInstance((MCObject *)id) || id->object.heap!=heap)) ||
        !valid_string(heap,name) || !MCObjectRootScope_pin(&scope,(MCObject *)id) ||
        !MCObjectRootScope_pin(&scope,(MCObject *)name)) goto failed;
    if (!id) {
        bool blank=true;
        const uint16_t *units=NBTString_units(name);
        for (size_t i=0;i<NBTString_length(name);i++) if (!whitespace(units[i])) {blank=false;break;}
        if (blank) goto failed;
    }
    profile=(NativeGameProfile *)MCObjectHeap_alloc(heap,sizeof(*profile),&profileClass);
    if (profile) {profile->id=id;profile->name=name;MCObjectHeap_touch(heap);}
    MCObjectRootScope_end(&scope);return profile;
failed:
    MCObjectHeap_fail(heap);MCObjectRootScope_end(&scope);return NULL;
}
NativeJavaUUID *NativeGameProfile_getId(NativeGameProfile *profile) {
    if (!NativeGameProfile_isInstance((MCObject *)profile)) {MCObjectHeap_fail(profile?profile->object.heap:NULL);return NULL;}
    NativeJavaUUID *id=profile->id;
    if (MCObjectHeap_failed(profile->object.heap) || (id &&
        (!NativeJavaUUID_isInstance((MCObject *)id) || id->object.heap!=profile->object.heap))) {
        MCObjectHeap_fail(profile->object.heap);return NULL;
    }
    return id;
}
NBTString *NativeGameProfile_getName(NativeGameProfile *profile) {
    if (!NativeGameProfile_isInstance((MCObject *)profile)) {MCObjectHeap_fail(profile?profile->object.heap:NULL);return NULL;}
    if (MCObjectHeap_failed(profile->object.heap) || !valid_string(profile->object.heap,profile->name)) return NULL;
    return profile->name;
}
