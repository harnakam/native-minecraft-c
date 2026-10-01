#include "world/WorldSettingsGameType.h"
#include "nbt/NBTInternal.h"
const WorldSettingsGameType WorldSettingsGameType_NOT_SET={-1,""};
const WorldSettingsGameType WorldSettingsGameType_SURVIVAL={0,"survival"};
const WorldSettingsGameType WorldSettingsGameType_CREATIVE={1,"creative"};
const WorldSettingsGameType WorldSettingsGameType_ADVENTURE={2,"adventure"};
const WorldSettingsGameType WorldSettingsGameType_SPECTATOR={3,"spectator"};
static const WorldSettingsGameType *const values[]={
    &WorldSettingsGameType_NOT_SET,&WorldSettingsGameType_SURVIVAL,
    &WorldSettingsGameType_CREATIVE,&WorldSettingsGameType_ADVENTURE,
    &WorldSettingsGameType_SPECTATOR
};
static int indexOf(const WorldSettingsGameType *mode) {
    for (int i=0;i<5;i++) if (mode==values[i]) return i;
    return -1;
}
bool WorldSettingsGameType_isCanonical(const WorldSettingsGameType *mode) {return indexOf(mode)>=0;}
int32_t WorldSettingsGameType_getID(const WorldSettingsGameType *mode) {
    return WorldSettingsGameType_isCanonical(mode)?mode->id:-1;
}
/* The native registry is a per-heap Java static-string root. Its strong edges
   preserve enum name identity during collection and all-root graph cloning;
   numeric enum identities themselves are immutable process-lifetime facts. */
typedef struct {MCObject object;NBTString *names[5];} NameRegistry;
static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    NameRegistry *registry=(NameRegistry *)object;
    for (int i=0;i<5;i++) registry->names[i]=(NBTString *)visitor((MCObject *)registry->names[i],context);
}
static const MCObjectClass registryClass={"native.WorldSettings.GameType.Names",MCObjectHeap_plainClone,trace,NULL};
static bool anyRegistry(const MCObject *object,void *context) {(void)object;(void)context;return true;}
NBTString *WorldSettingsGameType_getName(MCObjectHeap *heap,const WorldSettingsGameType *mode) {
    int index=indexOf(mode);MCObjectRootScope scope={0};
    if (index<0||!MCObjectRootScope_begin(&scope,heap)) {MCObjectHeap_fail(heap);return NULL;}
    NameRegistry *registry=(NameRegistry *)MCObjectHeap_findObject(heap,&registryClass,anyRegistry,NULL);
    if (!registry) {
        registry=(NameRegistry *)MCObjectHeap_alloc(heap,sizeof *registry,&registryClass);
        for (int i=0;registry&&i<5;i++) {
            registry->names[i]=NBTString_literalASCII(heap,values[i]->name);
            if (!registry->names[i]) registry=NULL;
        }
        MCObjectRoot staticRoot={0};
        if (registry&&!MCObjectRoot_init(&staticRoot,heap,(MCObject *)registry)) registry=NULL;
    }
    NBTString *name=registry?registry->names[index]:NULL;
    MCObjectRootScope_end(&scope);return MCObjectHeap_failed(heap)?NULL:name;
}
bool WorldSettingsGameType_isAdventure(const WorldSettingsGameType *mode) {
    return mode==&WorldSettingsGameType_ADVENTURE||mode==&WorldSettingsGameType_SPECTATOR;
}
bool WorldSettingsGameType_isCreative(const WorldSettingsGameType *mode) {return mode==&WorldSettingsGameType_CREATIVE;}
bool WorldSettingsGameType_isSurvivalOrAdventure(const WorldSettingsGameType *mode) {
    return mode==&WorldSettingsGameType_SURVIVAL||mode==&WorldSettingsGameType_ADVENTURE;
}
const WorldSettingsGameType *WorldSettingsGameType_getByID(int32_t id) {
    for (int i=0;i<5;i++) if (values[i]->id==id) return values[i];
    return &WorldSettingsGameType_SURVIVAL;
}
const WorldSettingsGameType *WorldSettingsGameType_getByName(const NBTString *name) {
    if (name) {
        MCObjectHeap *heap=name->object.heap;size_t bytes=MCObjectHeap_objectSize((const MCObject *)name);
        if (!NBTString_isInstance((const MCObject *)name)||bytes<sizeof *name||
            name->length>(bytes-sizeof *name)/sizeof *name->units||MCObjectHeap_failed(heap)) {
            MCObjectHeap_fail(heap);return NULL;
        }
    }
    for (int i=0;i<5;i++) if (NBTString_equalsASCII(name,values[i]->name)) return values[i];
    return &WorldSettingsGameType_SURVIVAL;
}
bool WorldSettingsGameType_configurePlayerCapabilities(const WorldSettingsGameType *mode,PlayerCapabilities *caps) {
    MCObjectHeap *heap=caps?caps->object.heap:NULL;MCObjectRootScope scope={0};
    if (!WorldSettingsGameType_isCanonical(mode)||!PlayerCapabilities_isInstance((MCObject *)caps)||
        !MCObjectRootScope_begin(&scope,heap)||!MCObjectRootScope_pin(&scope,(MCObject *)caps)) {
        MCObjectHeap_fail(heap);MCObjectRootScope_end(&scope);return false;
    }
#define ASSIGN(field,value) do {caps->field=(value);MCObjectHeap_touch(heap);} while (0)
    if (mode==&WorldSettingsGameType_CREATIVE) {
        ASSIGN(allowFlying,true);ASSIGN(isCreativeMode,true);ASSIGN(disableDamage,true);
    } else if (mode==&WorldSettingsGameType_SPECTATOR) {
        ASSIGN(allowFlying,true);ASSIGN(isCreativeMode,false);ASSIGN(disableDamage,true);ASSIGN(isFlying,true);
    } else {
        ASSIGN(allowFlying,false);ASSIGN(isCreativeMode,false);ASSIGN(disableDamage,false);ASSIGN(isFlying,false);
    }
    ASSIGN(allowEdit,!WorldSettingsGameType_isAdventure(mode));
#undef ASSIGN
    MCObjectRootScope_end(&scope);return true;
}
