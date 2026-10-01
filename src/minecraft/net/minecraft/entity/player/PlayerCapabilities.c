#include "entity/player/PlayerCapabilities.h"
static const MCObjectClass klass={"net.minecraft.entity.player.PlayerCapabilities",
                                 MCObjectHeap_plainClone,NULL,NULL};
bool PlayerCapabilities_isInstance(const MCObject *object) {
    return object&&object->klass==&klass&&MCObjectHeap_objectSize(object)>=sizeof(PlayerCapabilities);
}
static bool valid(const PlayerCapabilities *caps) {
    MCObjectHeap *heap=caps?caps->object.heap:NULL;
    if (!PlayerCapabilities_isInstance((const MCObject *)caps)||MCObjectHeap_failed(heap)) {
        MCObjectHeap_fail(heap);return false;
    }
    return true;
}
PlayerCapabilities *PlayerCapabilities_new(MCObjectHeap *heap) {
    PlayerCapabilities *caps=(PlayerCapabilities *)MCObjectHeap_alloc(heap,sizeof *caps,&klass);
    if (caps) {
        caps->disableDamage=false;caps->isFlying=false;caps->allowFlying=false;
        caps->isCreativeMode=false;caps->allowEdit=true;
        caps->flySpeed=0.05f;caps->walkSpeed=0.1f;
    }
    return caps;
}
float PlayerCapabilities_getFlySpeed(const PlayerCapabilities *caps) {return valid(caps)?caps->flySpeed:0;}
float PlayerCapabilities_getWalkSpeed(const PlayerCapabilities *caps) {return valid(caps)?caps->walkSpeed:0;}
bool PlayerCapabilities_setFlySpeed(PlayerCapabilities *caps,float speed) {
    if (!valid(caps)) return false;
    caps->flySpeed=speed;MCObjectHeap_touch(caps->object.heap);return true;
}
bool PlayerCapabilities_setPlayerWalkSpeed(PlayerCapabilities *caps,float speed) {
    if (!valid(caps)) return false;
    caps->walkSpeed=speed;MCObjectHeap_touch(caps->object.heap);return true;
}
static bool begin(PlayerCapabilities *caps,NBTTagCompound *tag,MCObjectRootScope *scope) {
    MCObjectHeap *heap=caps?caps->object.heap:NULL;
    if (!valid(caps)||(tag&&(((MCObject *)tag)->heap!=heap||NBTBase_getId((NBTBase *)tag)!=10))||
        !MCObjectRootScope_begin(scope,heap)) {MCObjectHeap_fail(heap);return false;}
    return MCObjectRootScope_pin(scope,(MCObject *)caps)&&MCObjectRootScope_pin(scope,(MCObject *)tag);
}
static bool end(PlayerCapabilities *caps,MCObjectRootScope *scope,bool ok) {
    MCObjectHeap *heap=caps?caps->object.heap:NULL;
    ok=ok&&!MCObjectHeap_failed(heap);
    if (!ok) MCObjectHeap_fail(heap);
    MCObjectRootScope_end(scope);return ok;
}
bool PlayerCapabilities_writeCapabilitiesToNBT(PlayerCapabilities *caps,NBTTagCompound *tag) {
    MCObjectRootScope scope={0};bool ok=begin(caps,tag,&scope);
    NBTTagCompound *inner=ok?NBTTagCompound_new(caps->object.heap):NULL;
    /* The original allocates/fills this fresh compound before invoking the
       supplied outer tag. A NULL outer tag therefore fails after that prefix. */
    ok=inner&&NBTTagCompound_setBoolean_ascii(inner,"invulnerable",caps->disableDamage)&&
        NBTTagCompound_setBoolean_ascii(inner,"flying",caps->isFlying)&&
        NBTTagCompound_setBoolean_ascii(inner,"mayfly",caps->allowFlying)&&
        NBTTagCompound_setBoolean_ascii(inner,"instabuild",caps->isCreativeMode)&&
        NBTTagCompound_setBoolean_ascii(inner,"mayBuild",caps->allowEdit)&&
        NBTTagCompound_setFloat_ascii(inner,"flySpeed",caps->flySpeed)&&
        NBTTagCompound_setFloat_ascii(inner,"walkSpeed",caps->walkSpeed);
    if (ok) ok=tag&&NBTTagCompound_setTag_ascii(tag,"abilities",(NBTBase *)inner);
    return end(caps,&scope,ok);
}
bool PlayerCapabilities_readCapabilitiesFromNBT(PlayerCapabilities *caps,NBTTagCompound *tag) {
    MCObjectRootScope scope={0};bool ok=begin(caps,tag,&scope);
    MCObjectHeap *heap=caps?caps->object.heap:NULL;
    if (!ok||!tag) return end(caps,&scope,false);
    bool has=NBTTagCompound_hasKeyType_ascii(tag,"abilities",10);
    if (MCObjectHeap_failed(heap)) return end(caps,&scope,false);
    if (has) {
        NBTTagCompound *inner=NBTTagCompound_getCompoundTag_ascii(tag,"abilities");
        if (!inner||MCObjectHeap_failed(heap)) return end(caps,&scope,false);
#define READ_FIELD(field,type,expression) do { \
    type value=(expression); \
    if (MCObjectHeap_failed(heap)) return end(caps,&scope,false); \
    caps->field=value;MCObjectHeap_touch(heap); \
} while (0)
        READ_FIELD(disableDamage,bool,NBTTagCompound_getBoolean_ascii(inner,"invulnerable"));
        READ_FIELD(isFlying,bool,NBTTagCompound_getBoolean_ascii(inner,"flying"));
        READ_FIELD(allowFlying,bool,NBTTagCompound_getBoolean_ascii(inner,"mayfly"));
        READ_FIELD(isCreativeMode,bool,NBTTagCompound_getBoolean_ascii(inner,"instabuild"));
        has=NBTTagCompound_hasKeyType_ascii(inner,"flySpeed",99);
        if (MCObjectHeap_failed(heap)) return end(caps,&scope,false);
        if (has) {
            READ_FIELD(flySpeed,float,NBTTagCompound_getFloat_ascii(inner,"flySpeed"));
            READ_FIELD(walkSpeed,float,NBTTagCompound_getFloat_ascii(inner,"walkSpeed"));
        }
        has=NBTTagCompound_hasKeyType_ascii(inner,"mayBuild",1);
        if (MCObjectHeap_failed(heap)) return end(caps,&scope,false);
        if (has) READ_FIELD(allowEdit,bool,NBTTagCompound_getBoolean_ascii(inner,"mayBuild"));
#undef READ_FIELD
    }
    return end(caps,&scope,true);
}
