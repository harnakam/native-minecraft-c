#include "world/WorldSettingsGameType.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nbt/NBTTagByte.h"
#include "nbt/NBTTagFloat.h"
#include "nbt/NBTInternal.h"
static unsigned checks;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    fprintf(stderr,"capabilities check %u line %d: %s\n",checks,__LINE__,#condition); \
    exit(1); } } while (0)
static uint32_t bits(float value) { uint32_t out; memcpy(&out,&value,sizeof out); return out; }
static float number(uint32_t value) {float out;memcpy(&out,&value,sizeof out);return out;}
static unsigned flags(const PlayerCapabilities *caps) {
    return (caps->disableDamage?1u:0u)|(caps->isFlying?2u:0u)|(caps->allowFlying?4u:0u)|
        (caps->isCreativeMode?8u:0u)|(caps->allowEdit?16u:0u);
}
static void preset(PlayerCapabilities *caps,unsigned value,float fly,float walk) {
    caps->disableDamage=(value&1)!=0;caps->isFlying=(value&2)!=0;caps->allowFlying=(value&4)!=0;
    caps->isCreativeMode=(value&8)!=0;caps->allowEdit=(value&16)!=0;
    caps->flySpeed=fly;caps->walkSpeed=walk;MCObjectHeap_touch(caps->object.heap);
}
/* Missing allowEdit or speed initializers changes actual persisted abilities. */
static void constructor_defaults(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);
    PlayerCapabilities *caps=PlayerCapabilities_new(heap);CHECK(caps);
    CHECK(PlayerCapabilities_isInstance((MCObject *)caps));
    CHECK(!caps->disableDamage&&!caps->isFlying&&!caps->allowFlying&&!caps->isCreativeMode&&caps->allowEdit);
    CHECK(bits(PlayerCapabilities_getFlySpeed(caps))==UINT32_C(0x3d4ccccd));
    CHECK(bits(PlayerCapabilities_getWalkSpeed(caps))==UINT32_C(0x3dcccccd));
    CHECK(!MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
}
/* Source write replaces the abilities compound and read has asymmetric gates;
   unknown preservation/independent-speed reads would break these contracts. */
static void nbt_write_and_read_gates(void) {
    MCObjectHeap *heap=MCObjectHeap_new(8*1024*1024);CHECK(heap);
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,heap));
    PlayerCapabilities *caps=PlayerCapabilities_new(heap);CHECK(caps);
    for(unsigned value=0;value<32;value++) {
        preset(caps,value,number(UINT32_C(0x7fc01234)),number(UINT32_C(0x80000000)));
        NBTTagCompound *root=NBTTagCompound_new(heap),*stale=NBTTagCompound_new(heap);CHECK(root&&stale);
        CHECK(NBTTagCompound_setInteger_ascii(stale,"extra",99));
        CHECK(NBTTagCompound_setTag_ascii(root,"abilities",(NBTBase *)stale));
        CHECK(PlayerCapabilities_writeCapabilitiesToNBT(caps,root));
        NBTTagCompound *saved=NBTTagCompound_getCompoundTag_ascii(root,"abilities");
        CHECK(saved&&saved!=stale&&!NBTTagCompound_hasKey_ascii(saved,"extra"));
        CHECK(NBTTagCompound_getInteger_ascii(stale,"extra")==99);
        static const char *const booleans[]={"invulnerable","flying","mayfly","instabuild","mayBuild"};
        for(unsigned i=0;i<5;i++) {
            CHECK(NBTTagCompound_hasKeyType_ascii(saved,booleans[i],1));
            CHECK(NBTTagCompound_getBoolean_ascii(saved,booleans[i])==((value&(1u<<i))!=0));
        }
        CHECK(NBTTagCompound_hasKeyType_ascii(saved,"flySpeed",5)&&NBTTagCompound_hasKeyType_ascii(saved,"walkSpeed",5));
        PlayerCapabilities *copy=PlayerCapabilities_new(heap);CHECK(copy);
        CHECK(PlayerCapabilities_readCapabilitiesFromNBT(copy,root));CHECK(flags(copy)==value);
        CHECK(bits(PlayerCapabilities_getFlySpeed(copy))==UINT32_C(0x7fc01234));
        CHECK(bits(PlayerCapabilities_getWalkSpeed(copy))==UINT32_C(0x80000000));
    }
    /* Literal results independently observed from the unchanged target JAR. */
    static const struct {unsigned flags;uint32_t fly,walk;} want[]={
        {15,0xbf000000,0xbe800000},{15,0xbf000000,0xbe800000},
        {0,0xbf000000,0xbe800000},{0,0xbf000000,0xbe800000},
        {16,0xbf000000,0xbe800000},{0,0x3e800000,0},
        {0,0xbf000000,0xbe800000},{0,0xbf000000,0xbe800000},
        {0,0x40000000,0xbf800000},{1,0xbf000000,0xbe800000},
        {16,0xbf000000,0xbe800000},{0,0x7fc00000,0x7f800000},
        {0,0x80000000,0x80000000},{0,0xbf000000,0xbe800000},
        {0,0xbf000000,0xbe800000},{15,0xbf000000,0xbe800000},
        {0,0x3e800000,0},{0,0x5f000000,0xff800000}
    };
    for(unsigned test=0;test<sizeof(want)/sizeof(*want);test++) {
        preset(caps,15,-0.5f,-0.25f);NBTTagCompound *root=NBTTagCompound_new(heap),*inner=NBTTagCompound_new(heap);CHECK(root&&inner);
        if(test==1) CHECK(NBTTagCompound_setTag_ascii(root,"abilities",(NBTBase *)NBTTagList_new(heap)));
        else if(test>=2) CHECK(NBTTagCompound_setTag_ascii(root,"abilities",(NBTBase *)inner));
        switch(test) {
            case 3:CHECK(NBTTagCompound_setInteger_ascii(inner,"mayBuild",1));break;
            case 4:CHECK(NBTTagCompound_setByte_ascii(inner,"mayBuild",1));break;
            case 5:CHECK(NBTTagCompound_setFloat_ascii(inner,"flySpeed",0.25f));break;
            case 6:CHECK(NBTTagCompound_setFloat_ascii(inner,"walkSpeed",0.75f));break;
            case 7:CHECK(NBTTagCompound_setString_ascii(inner,"flySpeed",NBTString_fromASCII(heap,"wrong")));break;
            case 8:CHECK(NBTTagCompound_setInteger_ascii(inner,"flySpeed",2)&&NBTTagCompound_setFloat_ascii(inner,"walkSpeed",-1));break;
            case 9:CHECK(NBTTagCompound_setShort_ascii(inner,"invulnerable",1));break;
            case 10:CHECK(NBTTagCompound_setByte_ascii(inner,"mayBuild",2));break;
            case 11:CHECK(NBTTagCompound_setFloat_ascii(inner,"flySpeed",number(0x7fc00000))&&NBTTagCompound_setFloat_ascii(inner,"walkSpeed",number(0x7f800000)));break;
            case 12:CHECK(NBTTagCompound_setFloat_ascii(inner,"flySpeed",number(0x80000000))&&NBTTagCompound_setDouble_ascii(inner,"walkSpeed",-0.0));break;
            case 13:CHECK(NBTTagCompound_setTag_ascii(inner,"invulnerable",(NBTBase *)NBTTagCompound_new(heap))&&NBTTagCompound_setTag_ascii(inner,"flying",(NBTBase *)NBTTagList_new(heap)));break;
            case 14:CHECK(NBTTagCompound_setFloat_ascii(inner,"mayBuild",1));break;
            case 15:CHECK(NBTTagCompound_setByte_ascii(inner,"invulnerable",-128)&&NBTTagCompound_setByte_ascii(inner,"flying",-128)&&NBTTagCompound_setByte_ascii(inner,"mayfly",-128)&&NBTTagCompound_setByte_ascii(inner,"instabuild",-128));break;
            case 16:CHECK(NBTTagCompound_setFloat_ascii(inner,"flySpeed",0.25f)&&NBTTagCompound_setString_ascii(inner,"walkSpeed",NBTString_fromASCII(heap,"wrong")));break;
            case 17:CHECK(NBTTagCompound_setLong_ascii(inner,"flySpeed",INT64_MAX)&&NBTTagCompound_setDouble_ascii(inner,"walkSpeed",-(double)number(0x7f800000)));break;
            default:break;
        }
        CHECK(PlayerCapabilities_readCapabilitiesFromNBT(caps,root));
        CHECK(flags(caps)==want[test].flags&&bits(caps->flySpeed)==want[test].fly&&bits(caps->walkSpeed)==want[test].walk);
    }
    MCObjectRootScope_end(&scope);CHECK(!MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
}
static void speed_accessors(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);
    PlayerCapabilities *caps=PlayerCapabilities_new(heap);CHECK(caps);
    static const uint32_t values[]={0,0x80000000,0x3d4ccccd,0xbf800000,0x7f800000,0xff800000,0x7fc01234};
    for(size_t i=0;i<sizeof values/sizeof *values;i++) {
        CHECK(PlayerCapabilities_setFlySpeed(caps,number(values[i])));
        CHECK(PlayerCapabilities_setPlayerWalkSpeed(caps,number(values[i])));
        CHECK(bits(PlayerCapabilities_getFlySpeed(caps))==values[i]);
        CHECK(bits(PlayerCapabilities_getWalkSpeed(caps))==values[i]);
    }
    CHECK(flags(caps)==16);MCObjectHeap_free(heap);
}
/* Creative preserves isFlying; spectator clears creative and mayBuild. This
   is capabilities configuration, independent of any controller GameType. */
static void game_type_configuration(void) {
    const WorldSettingsGameType *modes[]={&WorldSettingsGameType_NOT_SET,&WorldSettingsGameType_SURVIVAL,
        &WorldSettingsGameType_CREATIVE,&WorldSettingsGameType_ADVENTURE,&WorldSettingsGameType_SPECTATOR};
    static const char *const names[]={"","survival","creative","adventure","spectator"};
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);
    PlayerCapabilities *caps=PlayerCapabilities_new(heap);CHECK(caps);
    for(unsigned m=0;m<5;m++) {
        CHECK(WorldSettingsGameType_isCanonical(modes[m]));
        CHECK(WorldSettingsGameType_getID(modes[m])==(int32_t)m-1);
        CHECK(WorldSettingsGameType_getByID((int32_t)m-1)==modes[m]);
        NBTString *name=WorldSettingsGameType_getName(heap,modes[m]);CHECK(name);
        CHECK(NBTString_equalsASCII(name,names[m]));
        CHECK(WorldSettingsGameType_getName(heap,modes[m])==name);
        CHECK(WorldSettingsGameType_getByName(name)==modes[m]);
        CHECK(WorldSettingsGameType_isAdventure(modes[m])==(m==3||m==4));
        CHECK(WorldSettingsGameType_isCreative(modes[m])==(m==2));
        CHECK(WorldSettingsGameType_isSurvivalOrAdventure(modes[m])==(m==1||m==3));
        for(unsigned f=0;f<32;f++) {
            preset(caps,f,number(0x80000000),number(0x7fc01234));
            CHECK(WorldSettingsGameType_configurePlayerCapabilities(modes[m],caps));
            unsigned wanted=m==2?29u|(f&2u):m==4?7u:m==3?0u:16u;
            CHECK(flags(caps)==wanted);
            CHECK(bits(caps->flySpeed)==0x80000000&&bits(caps->walkSpeed)==0x7fc01234);
        }
    }
    CHECK(WorldSettingsGameType_getByID(INT32_MIN)==&WorldSettingsGameType_SURVIVAL);
    CHECK(WorldSettingsGameType_getByID(INT32_MAX)==&WorldSettingsGameType_SURVIVAL);
    CHECK(WorldSettingsGameType_getByName(NULL)==&WorldSettingsGameType_SURVIVAL);
    CHECK(WorldSettingsGameType_getByName(NBTString_fromASCII(heap,"Creative"))==&WorldSettingsGameType_SURVIVAL);
    const uint16_t embedded[]={ 'c','r','e','a','t','i','v','e',0 };
    CHECK(WorldSettingsGameType_getByName(NBTString_fromUTF16(heap,embedded,9))==&WorldSettingsGameType_SURVIVAL);
    WorldSettingsGameType forged={1,"creative"};
    CHECK(!WorldSettingsGameType_isCanonical(&forged));
    CHECK(WorldSettingsGameType_getID(&forged)==-1&&!WorldSettingsGameType_isCreative(&forged));
    CHECK(!MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
}
static void managed_identity_and_adoption(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);
    PlayerCapabilities *caps=PlayerCapabilities_new(heap);CHECK(caps);
    MCObjectRoot alice={0},bob={0};
    CHECK(MCObjectRoot_init(&alice,heap,(MCObject *)caps));
    CHECK(MCObjectRoot_init(&bob,heap,(MCObject *)caps));
    NBTString *name=WorldSettingsGameType_getName(heap,&WorldSettingsGameType_CREATIVE);CHECK(name);
    size_t objects=MCObjectHeap_liveObjects(heap);
    CHECK(MCObjectHeap_collect(heap));CHECK(MCObjectHeap_liveObjects(heap)==objects);
    CHECK(WorldSettingsGameType_getName(heap,&WorldSettingsGameType_CREATIVE)==name);
    MCObjectRootScope borrow={0};CHECK(MCObjectRootScope_begin(&borrow,heap));
    CHECK(!MCObjectHeap_collect(heap)&&!MCObjectHeap_failed(heap));
    CHECK(!MCObjectHeap_clone(heap)&&!MCObjectHeap_failed(heap));
    MCObjectRootScope_end(&borrow);
    MCObjectHeap *working=MCObjectHeap_clone(heap);CHECK(working);
    MCObjectRoot branchAlice={0},branchBob={0};
    CHECK(MCObjectRoot_rebind(&branchAlice,working,&alice));
    CHECK(MCObjectRoot_rebind(&branchBob,working,&bob));
    PlayerCapabilities *shared=(PlayerCapabilities *)MCObjectRoot_get(&branchAlice);CHECK(shared&&shared!=caps);
    CHECK(MCObjectRoot_get(&branchBob)==(MCObject *)shared);
    NBTString *branchName=WorldSettingsGameType_getName(working,&WorldSettingsGameType_CREATIVE);
    CHECK(branchName&&branchName!=name&&NBTString_equals(branchName,name));
    CHECK(WorldSettingsGameType_configurePlayerCapabilities(&WorldSettingsGameType_CREATIVE,shared));
    CHECK(PlayerCapabilities_setFlySpeed(shared,number(0x80000000)));
    CHECK(flags(caps)==16&&flags(shared)==29);
    CHECK(MCObjectHeap_canAdopt(heap,working));CHECK(MCObjectHeap_adopt(heap,working));
    caps=(PlayerCapabilities *)MCObjectRoot_get(&alice);
    CHECK(caps&&MCObjectRoot_get(&bob)==(MCObject *)caps);
    CHECK(flags(caps)==29&&bits(caps->flySpeed)==0x80000000);
    CHECK(WorldSettingsGameType_getName(heap,&WorldSettingsGameType_CREATIVE)==branchName);
    MCObjectHeap_free(working);CHECK(MCObjectHeap_collect(heap));
    CHECK(WorldSettingsGameType_getName(heap,&WorldSettingsGameType_CREATIVE)==branchName);
    CHECK(!MCObjectHeap_failed(heap));MCObjectRoot_drop(&alice);MCObjectRoot_drop(&bob);MCObjectHeap_free(heap);
}
/* Native OOM is a real method failure: the fresh inner compound is attached
   only after every field succeeds. An aborted graph must not leak its prefix
   into the live owner's old abilities. Reads require no allocation. */
static void bounded_write_and_abort(void) {
    unsigned failures=0,successes=0;
    for(size_t budget=600;budget<2600;budget+=50) {
        MCObjectHeap *heap=MCObjectHeap_new(budget);CHECK(heap);
        PlayerCapabilities *caps=PlayerCapabilities_new(heap);
        NBTTagCompound *outer=NBTTagCompound_new(heap),*old=NBTTagCompound_new(heap);
        CHECK(caps&&outer&&old);CHECK(NBTTagCompound_setTag_ascii(outer,"abilities",(NBTBase *)old));
        MCObjectRoot owner={0};CHECK(MCObjectRoot_init(&owner,heap,(MCObject *)outer));
        MCObjectHeap *working=MCObjectHeap_clone(heap);CHECK(working);
        MCObjectRoot branch={0};CHECK(MCObjectRoot_rebind(&branch,working,&owner));
        NBTTagCompound *target=(NBTTagCompound *)MCObjectRoot_get(&branch);
        PlayerCapabilities *branchCaps=PlayerCapabilities_new(working);
        CHECK(branchCaps);preset(branchCaps,31,0.25f,-0.5f);
        bool ok=PlayerCapabilities_writeCapabilitiesToNBT(branchCaps,target);
        if(!ok) {
            failures++;CHECK(MCObjectHeap_failed(working));
            CHECK(NBTTagCompound_getTag_ascii(target,"abilities")!=(NBTBase *)NULL);
            CHECK(NBTBase_hasNoTags((NBTBase *)NBTTagCompound_getCompoundTag_ascii(target,"abilities")));
            CHECK(!MCObjectHeap_canAdopt(heap,working));
        } else {successes++;CHECK(!MCObjectHeap_failed(working));}
        MCObjectHeap_free(working);
        CHECK(NBTTagCompound_getTag_ascii(outer,"abilities")== (NBTBase *)old);
        CHECK(!MCObjectHeap_failed(heap));MCObjectRoot_drop(&owner);MCObjectHeap_free(heap);
    }
    CHECK(failures&&successes);
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);
    PlayerCapabilities *caps=PlayerCapabilities_new(heap);NBTTagCompound *tag=NBTTagCompound_new(heap);CHECK(caps&&tag);
    CHECK(PlayerCapabilities_writeCapabilitiesToNBT(caps,tag));
    size_t count=MCObjectHeap_liveObjects(heap),bytes=MCObjectHeap_liveBytes(heap);
    CHECK(PlayerCapabilities_readCapabilitiesFromNBT(caps,tag));
    CHECK(MCObjectHeap_liveObjects(heap)==count&&MCObjectHeap_liveBytes(heap)==bytes);
    size_t before=MCObjectHeap_liveObjects(heap);
    CHECK(!PlayerCapabilities_writeCapabilitiesToNBT(caps,NULL)&&MCObjectHeap_failed(heap));
    CHECK(MCObjectHeap_liveObjects(heap)>before&&!MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}
static void native_guards(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024),*foreign=MCObjectHeap_new(1024*1024);CHECK(heap&&foreign);
    PlayerCapabilities *caps=PlayerCapabilities_new(heap);NBTTagCompound *wrongHeap=NBTTagCompound_new(foreign);CHECK(caps&&wrongHeap);
    preset(caps,31,0.25f,-0.5f);
    CHECK(!PlayerCapabilities_readCapabilitiesFromNBT(caps,wrongHeap));
    CHECK(flags(caps)==31&&bits(caps->flySpeed)==0x3e800000);
    CHECK(MCObjectHeap_failed(heap)&&!MCObjectHeap_failed(foreign)&&!MCObjectHeap_hasBorrowers(heap));
    CHECK(!PlayerCapabilities_setFlySpeed(caps,1)&&bits(caps->flySpeed)==0x3e800000);
    CHECK(!WorldSettingsGameType_configurePlayerCapabilities(&WorldSettingsGameType_SURVIVAL,caps)&&flags(caps)==31);
    MCObjectHeap_free(heap);MCObjectHeap_free(foreign);
    heap=MCObjectHeap_new(1024*1024);CHECK(heap);caps=PlayerCapabilities_new(heap);CHECK(caps);
    WorldSettingsGameType forged={1,"creative"};
    CHECK(!WorldSettingsGameType_configurePlayerCapabilities(&forged,caps));
    CHECK(flags(caps)==16&&MCObjectHeap_failed(heap)&&!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(1024*1024);CHECK(heap);caps=PlayerCapabilities_new(heap);CHECK(caps);
    CHECK(!PlayerCapabilities_readCapabilitiesFromNBT(caps,NULL)&&MCObjectHeap_failed(heap));
    CHECK(flags(caps)==16&&!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(1024*1024);CHECK(heap);caps=PlayerCapabilities_new(heap);CHECK(caps);
    NBTTagByte *wrongType=NBTTagByte_new(heap,0);CHECK(wrongType);
    CHECK(!PlayerCapabilities_readCapabilitiesFromNBT(caps,(NBTTagCompound *)wrongType));
    CHECK(flags(caps)==16&&MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(1024*1024);CHECK(heap);
    caps=PlayerCapabilities_new(heap);CHECK(caps);
    MCObject *undersized=MCObjectHeap_alloc(heap,sizeof(MCObject),caps->object.klass);CHECK(undersized);
    CHECK(!PlayerCapabilities_isInstance(undersized));
    CHECK(!PlayerCapabilities_setFlySpeed((PlayerCapabilities *)undersized,1)&&MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(1024*1024);CHECK(heap);
    NBTString *malformed=NBTString_fromASCII(heap,"creative");CHECK(malformed);
    ((struct NBTString *)malformed)->length=SIZE_MAX;
    CHECK(!WorldSettingsGameType_getByName(malformed)&&MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(1024*1024);CHECK(heap);
    CHECK(!WorldSettingsGameType_getName(heap,&forged)&&MCObjectHeap_failed(heap)&&!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
    CHECK(!WorldSettingsGameType_configurePlayerCapabilities(&WorldSettingsGameType_CREATIVE,NULL));
}
int main(void) {
    constructor_defaults();nbt_write_and_read_gates();speed_accessors();game_type_configuration();
    managed_identity_and_adoption();bounded_write_and_abort();native_guards();
    printf("player capabilities: %u checks passed\n",checks);return 0;
}
