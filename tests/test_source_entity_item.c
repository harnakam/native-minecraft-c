#include "entity/item/EntityItem.h"
#include "nbt/NBTTagCompound.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"source entity item check %u at %d: %s\n",checks,__LINE__,#x); exit(1); } } while (0)
typedef struct TestPlayer { MCObject object; InventoryPlayer *inventory; NBTString *name; bool creative; } TestPlayer;
enum { EVENT_LOG=1,EVENT_WATCH,EVENT_ACHIEVEMENT,EVENT_SOUND,EVENT_PICKUP,EVENT_DEAD };
typedef struct {
    MCObject object; TestPlayer *players[2]; bool remote,silent; unsigned events[32],count,randomCalls;
    int failEvent; EntityItem *lastEntity; ItemStack *lastStack; MCObject *lastPlayer;
    int32_t originalCount; float pitch; EntityItemAchievement achievements[8]; unsigned achievementCount;
} TestContext;
static void player_trace(MCObject *o,MCObjectVisitor v,void *c) {
    TestPlayer *p=(TestPlayer *)o;p->inventory=(InventoryPlayer *)v((MCObject *)p->inventory,c);p->name=(NBTString *)v((MCObject *)p->name,c);
}
static void context_trace(MCObject *o,MCObjectVisitor v,void *c) {
    TestContext *t=(TestContext *)o;
    for (unsigned i=0;i<2;i++) t->players[i]=(TestPlayer *)v((MCObject *)t->players[i],c);
    t->lastEntity=(EntityItem *)v((MCObject *)t->lastEntity,c);t->lastStack=(ItemStack *)v((MCObject *)t->lastStack,c);t->lastPlayer=v(t->lastPlayer,c);
}
static const MCObjectClass player_class={"fixture.entityitem.Player",MCObjectHeap_plainClone,player_trace,NULL};
static const MCObjectClass context_class={"fixture.entityitem.World",MCObjectHeap_plainClone,context_trace,NULL};
static bool creative(const MCObject *o) { return ((const TestPlayer *)o)->creative; }
static bool event(TestContext *c,unsigned value) { CHECK(c->count<32);c->events[c->count++]=value;MCObjectHeap_touch(c->object.heap);return c->failEvent!=(int)value; }
static bool mark(MCObject *o,EntityItem *e,int32_t index) { TestContext *c=(TestContext *)o;CHECK(index==10);c->lastEntity=e;c->lastStack=e->watchedItem;return event(c,EVENT_WATCH); }
static bool log_missing(MCObject *o,int32_t id) { (void)id;return event((TestContext *)o,EVENT_LOG); }
static bool remote(MCObject *o,MCObject *world) { CHECK(world==o);return ((TestContext *)o)->remote; }
static InventoryPlayer *inventory(MCObject *o,MCObject *p) { (void)o;return ((TestPlayer *)p)->inventory; }
static const NBTString *name(MCObject *o,MCObject *p) { (void)o;return ((TestPlayer *)p)->name; }
static MCObject *find_player(MCObject *o,MCObject *world,const NBTString *n) {
    TestContext *c=(TestContext *)o;CHECK(world==o);
    for (unsigned i=0;i<2;i++) if (NBTString_equals(c->players[i]->name,n)) return (MCObject *)c->players[i];
    return NULL;
}
static bool achievement(MCObject *o,MCObject *p,EntityItemAchievement a) {
    TestContext *c=(TestContext *)o;CHECK(c->achievementCount<8);c->achievements[c->achievementCount++]=a;c->lastPlayer=p;return event(c,EVENT_ACHIEVEMENT);
}
static bool silent(MCObject *o,const EntityItem *e) { (void)e;return ((TestContext *)o)->silent; }
static float random_float(MCObject *o,EntityItem *e) { (void)e;TestContext *c=(TestContext *)o;return c->randomCalls++%2==0?0.25F:0.75F; }
static bool sound(MCObject *o,MCObject *world,MCObject *p,const char *n,float volume,float pitch) {
    TestContext *c=(TestContext *)o;CHECK(world==o&&strcmp(n,"random.pop")==0&&volume==0.2F);c->pitch=pitch;c->lastPlayer=p;return event(c,EVENT_SOUND);
}
static bool pickup(MCObject *o,MCObject *p,EntityItem *e,int32_t count) {
    TestContext *c=(TestContext *)o;c->lastPlayer=p;c->lastEntity=e;c->lastStack=e->watchedItem;c->originalCount=count;return event(c,EVENT_PICKUP);
}
static bool dead(MCObject *o,EntityItem *e) { e->isDead=true;MCObjectHeap_touch(e->object.heap);return event((TestContext *)o,EVENT_DEAD); }
static const EntityItemDependencies dependencies={mark,log_missing,remote,inventory,name,find_player,achievement,silent,random_float,sound,pickup,dead};
static TestContext *setup(MCObjectHeap *h) {
    TestContext *c=(TestContext *)MCObjectHeap_alloc(h,sizeof(*c),&context_class);CHECK(c);
    for (unsigned i=0;i<2;i++) {
        TestPlayer *p=(TestPlayer *)MCObjectHeap_alloc(h,sizeof(*p),&player_class);CHECK(p);c->players[i]=p;
        p->name=NBTString_fromASCII(h,i?"Thrower":"Collector");CHECK(p->name);p->inventory=InventoryPlayer_new(h,(MCObject *)p,creative);CHECK(p->inventory);
    }
    return c;
}
static ItemStack *stack(MCObjectHeap *h,int id,int32_t count,int32_t damage) { ItemStack *s=ItemStack_new(h,ItemStack_registryItem(id),count,damage);CHECK(s);return s; }
static EntityItem *entity(TestContext *c,ItemStack *s) {
    EntityItem *e=EntityItem_nativeNew(c->object.heap,(MCObject *)c,(MCObject *)c,&dependencies);CHECK(e);CHECK(EntityItem_setEntityItemStack(e,s));return e;
}
static void reset_events(TestContext *c) { c->count=0;c->randomCalls=0;c->achievementCount=0;c->lastEntity=NULL;c->lastStack=NULL;c->lastPlayer=NULL; }
static void get_set(void) {
    MCObjectHeap *h=MCObjectHeap_new(16*1024*1024);CHECK(h);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));TestContext *c=setup(h);
    EntityItem *e=EntityItem_nativeNew(h,(MCObject *)c,(MCObject *)c,&dependencies);CHECK(e);
    ItemStack *one=EntityItem_getEntityItem(e),*two=EntityItem_getEntityItem(e);
    CHECK(one&&two&&one!=two&&one->item==ItemStack_registryItem(1)&&one->stackSize==1);CHECK(!e->watchedItem&&c->count==2);
    e->worldObj=NULL;CHECK(EntityItem_getEntityItem(e));CHECK(c->count==2);e->worldObj=(MCObject *)c;
    const int32_t counts[]={0,-1,INT32_MIN,INT32_MAX};
    for (unsigned i=0;i<4;i++) { ItemStack *s=stack(h,1,counts[i],0);CHECK(EntityItem_setEntityItemStack(e,s));CHECK(EntityItem_getEntityItem(e)==s&&s->stackSize==counts[i]);CHECK(EntityItem_setEntityItemStack(e,s)); }
    CHECK(EntityItem_setEntityItemStack(e,NULL));CHECK(!e->watchedItem);CHECK(c->count==11);
    NBTString *owner=NBTString_fromUTF8(h,"所有者");CHECK(owner);CHECK(EntityItem_setOwner(e,owner));CHECK(EntityItem_getOwner(e)==owner);CHECK(EntityItem_setThrower(e,owner));CHECK(EntityItem_getThrower(e)==owner);
    EntityItem_setDefaultPickupDelay(e);CHECK(e->delayBeforeCanPickup==10&&EntityItem_cannotPickup(e));EntityItem_setInfinitePickupDelay(e);CHECK(e->delayBeforeCanPickup==32767);
    EntityItem_setPickupDelay(e,-7);CHECK(!EntityItem_cannotPickup(e));EntityItem_setNoPickupDelay(e);CHECK(e->delayBeforeCanPickup==0);
    EntityItem_setNoDespawn(e);CHECK(EntityItem_getAge(e)==-6000);EntityItem_func_174870_v(e);CHECK(e->age==5999&&e->delayBeforeCanPickup==32767);
    for (unsigned i=0;i<4;i++) {
        e->age=counts[i];EntityItem_setAgeToCreativeDespawnTime(e);
        CHECK(e->age==4800&&e->delayBeforeCanPickup==32767&&e->owner==owner&&e->thrower==owner&&!e->watchedItem&&!e->isDead);
    }
    CHECK(!MCObjectHeap_failed(h));MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
}
static void merge(void) {
    const int32_t sizes[][2]={{2,2},{5,3},{0,1},{-2,-1},{INT32_MAX,1}};
    for (unsigned i=0;i<5;i++) {
        MCObjectHeap *h=MCObjectHeap_new(16*1024*1024);CHECK(h);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));TestContext *c=setup(h);
        ItemStack *a=stack(h,1,sizes[i][0],0),*b=stack(h,1,sizes[i][1],0);EntityItem *x=entity(c,a),*y=entity(c,b);reset_events(c);
        x->age=30;y->age=70;x->delayBeforeCanPickup=8;y->delayBeforeCanPickup=3;
        CHECK(EntityItem_combineItems(x,y));EntityItem *kept=sizes[i][0]>sizes[i][1]?x:y,*removed=kept==x?y:x;
        uint32_t bits=(uint32_t)sizes[i][0]+(uint32_t)sizes[i][1];int32_t sum;memcpy(&sum,&bits,sizeof(sum));
        CHECK(kept->watchedItem->stackSize==sum&&removed->isDead&&!kept->isDead);CHECK(removed->watchedItem==(removed==x?a:b));
        CHECK(kept->age==30&&kept->delayBeforeCanPickup==8);CHECK(c->count==2&&c->events[0]==EVENT_WATCH&&c->events[1]==EVENT_DEAD);
        CHECK(!MCObjectHeap_failed(h));MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
    }
    MCObjectHeap *h=MCObjectHeap_new(16*1024*1024);CHECK(h);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));TestContext *c=setup(h);
    ItemStack *shared=stack(h,1,2,0);EntityItem *a=entity(c,shared),*b=entity(c,shared);reset_events(c);CHECK(EntityItem_combineItems(a,b));CHECK(a->isDead&&!b->isDead&&a->watchedItem==b->watchedItem&&shared->stackSize==4);
    EntityItem *gold1=entity(c,stack(h,266,2,7)),*gold2=entity(c,stack(h,266,2,8));CHECK(EntityItem_combineItems(gold1,gold2));CHECK(gold2->watchedItem->stackSize==4&&gold2->watchedItem->itemDamage==8);
    EntityItem *wool1=entity(c,stack(h,35,2,0)),*wool2=entity(c,stack(h,35,2,1));CHECK(!EntityItem_combineItems(wool1,wool2));CHECK(!wool1->isDead&&!wool2->isDead);
    wool1->watchedItem->itemDamage=1;wool1->delayBeforeCanPickup=32767;CHECK(!EntityItem_combineItems(wool1,wool2));wool1->delayBeforeCanPickup=0;wool1->age=-32768;CHECK(!EntityItem_combineItems(wool1,wool2));wool1->age=0;
    NBTTagCompound *tag=NBTTagCompound_new(h);CHECK(tag);CHECK(ItemStack_setTagCompound(wool1->watchedItem,tag));CHECK(!EntityItem_combineItems(wool1,wool2));CHECK(ItemStack_setTagCompound(wool2->watchedItem,tag));CHECK(EntityItem_combineItems(wool1,wool2));
    CHECK(!EntityItem_combineItems(b,b));CHECK(!MCObjectHeap_failed(h));MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
}
static void pickup_cases(void) {
    const int32_t sizes[]={0,-1,-2,1,10};
    for (int creative_mode=0;creative_mode<2;creative_mode++) for (unsigned n=0;n<5;n++) for (int full=0;full<2;full++) {
        MCObjectHeap *h=MCObjectHeap_new(16*1024*1024);CHECK(h);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));TestContext *c=setup(h);TestPlayer *p=c->players[0];p->creative=creative_mode!=0;
        if (full) for (int i=0;i<36;i++) CHECK(InventoryPlayer_setInventorySlotContents(p->inventory,i,stack(h,5,64,0)));
        ItemStack *s=stack(h,1,sizes[n],0);EntityItem *e=entity(c,s);reset_events(c);CHECK(EntityItem_onCollideWithPlayer(e,(MCObject *)p));
        bool picked=sizes[n]!=0 && (full?creative_mode!=0:sizes[n]>0);
        CHECK(e->watchedItem==s);CHECK(e->isDead==picked);CHECK(c->count==(picked?3u:0u));
        if (picked) {CHECK(c->events[0]==EVENT_SOUND&&c->events[1]==EVENT_PICKUP&&c->events[2]==EVENT_DEAD);CHECK(c->originalCount==sizes[n]);CHECK(s->stackSize==0&&c->pitch==1.3F&&c->randomCalls==2);}
        else if (!full&&sizes[n]<0) CHECK(s->stackSize==0&&p->inventory->mainInventory->items[0]->stackSize==sizes[n]);
        CHECK(!MCObjectHeap_failed(h));MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
    }
}
static void partial_and_owner(void) {
    MCObjectHeap *h=MCObjectHeap_new(16*1024*1024);CHECK(h);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));TestContext *c=setup(h);TestPlayer *p=c->players[0];
    for (int i=0;i<36;i++) CHECK(InventoryPlayer_setInventorySlotContents(p->inventory,i,stack(h,5,64,0)));
    CHECK(InventoryPlayer_setInventorySlotContents(p->inventory,0,stack(h,266,60,0)));ItemStack *source=stack(h,266,10,0);EntityItem *e=entity(c,source);reset_events(c);
    CHECK(EntityItem_onCollideWithPlayer(e,(MCObject *)p));CHECK(source->stackSize==6&&p->inventory->mainInventory->items[0]->stackSize==64&&!e->isDead&&c->count==0);
    InventoryPlayer_clear(p->inventory);NBTString *other=NBTString_fromASCII(h,"Other");CHECK(other);CHECK(EntityItem_setOwner(e,other));
    e->age=5799;CHECK(EntityItem_onCollideWithPlayer(e,(MCObject *)p));CHECK(source->stackSize==6&&c->count==0);
    e->age=5800;CHECK(EntityItem_onCollideWithPlayer(e,(MCObject *)p));CHECK(e->isDead&&source->stackSize==0&&c->originalCount==6);
    e=entity(c,stack(h,1,1,0));reset_events(c);CHECK(EntityItem_setOwner(e,NBTString_fromASCII(h,"")));CHECK(EntityItem_onCollideWithPlayer(e,(MCObject *)p));CHECK(!e->isDead&&c->count==0);
    e->age=INT32_MIN;CHECK(EntityItem_onCollideWithPlayer(e,(MCObject *)p));CHECK(e->isDead&&c->originalCount==1);
    e=entity(c,stack(h,264,1,0));CHECK(EntityItem_setThrower(e,c->players[1]->name));reset_events(c);CHECK(EntityItem_onCollideWithPlayer(e,(MCObject *)p));
    CHECK(c->count==5&&c->events[0]==EVENT_ACHIEVEMENT&&c->events[1]==EVENT_ACHIEVEMENT&&c->events[2]==EVENT_SOUND&&c->events[3]==EVENT_PICKUP&&c->events[4]==EVENT_DEAD);
    CHECK(c->achievements[0]==ENTITYITEM_ACH_DIAMONDS&&c->achievements[1]==ENTITYITEM_ACH_DIAMONDS_TO_YOU);
    e=entity(c,stack(h,1,1,0));reset_events(c);c->remote=true;CHECK(EntityItem_onCollideWithPlayer(e,NULL));CHECK(EntityItem_onCollideWithPlayer(e,(MCObject *)p));CHECK(c->count==0&&!e->isDead);
    c->remote=false;EntityItem_setPickupDelay(e,1);CHECK(EntityItem_onCollideWithPlayer(e,NULL));CHECK(c->count==0&&!e->isDead);EntityItem_setNoPickupDelay(e);c->silent=true;
    CHECK(EntityItem_onCollideWithPlayer(e,(MCObject *)p));CHECK(c->count==2&&c->events[0]==EVENT_PICKUP&&c->events[1]==EVENT_DEAD&&c->randomCalls==0);
    CHECK(!MCObjectHeap_failed(h));MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
}
static void snapshot_alias(void) {
    MCObjectHeap *h=MCObjectHeap_new(16*1024*1024);CHECK(h);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));TestContext *c=setup(h);
    ItemStack *shared=stack(h,387,1,7);CHECK(InventoryPlayer_setItemStack(c->players[0]->inventory,shared));EntityItem *e=entity(c,shared);
    CHECK(EntityItem_setOwner(e,c->players[1]->name));NBTString *thrower=NBTString_fromUTF8(h,"投げた人");CHECK(thrower);CHECK(EntityItem_setThrower(e,thrower));
    e->posX=1.25;e->motionY=-0.5;e->ticksExisted=INT32_MAX;e->onGround=true;e->noClip=true;
    MCObjectRoot context_root={0},entity_root={0};CHECK(MCObjectRoot_init(&context_root,h,(MCObject *)c));CHECK(MCObjectRoot_init(&entity_root,h,(MCObject *)e));MCObjectRootScope_end(&scope);
    MCObjectHeap *working=MCObjectHeap_clone(h);CHECK(working);MCObjectRoot wc={0},we={0};CHECK(MCObjectRoot_rebind(&wc,working,&context_root));CHECK(MCObjectRoot_rebind(&we,working,&entity_root));
    CHECK(MCObjectRootScope_begin(&scope,working));TestContext *copy=(TestContext *)MCObjectRoot_get(&wc);EntityItem *drop=(EntityItem *)MCObjectRoot_get(&we);
    CHECK(drop->watchedItem==InventoryPlayer_getItemStack(copy->players[0]->inventory)&&drop->watchedItem!=shared);
    CHECK(drop->worldObj==(MCObject *)copy&&drop->dependencyContext==(MCObject *)copy);CHECK(drop->owner==copy->players[1]->name&&drop->owner!=c->players[1]->name);
    CHECK(drop->thrower!=thrower&&NBTString_equals(drop->thrower,thrower));CHECK(drop->posX==1.25&&drop->motionY==-0.5&&drop->ticksExisted==INT32_MAX&&drop->onGround&&drop->noClip);
    reset_events(copy);CHECK(EntityItem_onCollideWithPlayer(drop,(MCObject *)copy->players[1]));
    CHECK(drop->isDead&&drop->watchedItem->stackSize==0&&InventoryPlayer_getItemStack(copy->players[0]->inventory)==drop->watchedItem);CHECK(shared->stackSize==1&&!e->isDead);
    MCObjectRootScope_end(&scope);CHECK(MCObjectHeap_adopt(h,working));MCObjectHeap_free(working);c=(TestContext *)MCObjectRoot_get(&context_root);e=(EntityItem *)MCObjectRoot_get(&entity_root);
    CHECK(e->watchedItem==InventoryPlayer_getItemStack(c->players[0]->inventory)&&e->watchedItem->stackSize==0);CHECK(MCObjectHeap_collect(h));MCObjectHeap_free(h);
}
static void callback_failures(void) {
    const int failures[]={EVENT_LOG,EVENT_WATCH,EVENT_ACHIEVEMENT,EVENT_SOUND,EVENT_PICKUP,EVENT_DEAD};
    for (unsigned n=0;n<6;n++) {
        MCObjectHeap *h=MCObjectHeap_new(16*1024*1024);CHECK(h);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));TestContext *c=setup(h);
        EntityItem *e=EntityItem_nativeNew(h,(MCObject *)c,(MCObject *)c,&dependencies);CHECK(e);
        ItemStack *s=stack(h,n==2?264:1,1,0);
        if (n>1) CHECK(EntityItem_setEntityItemStack(e,s));
        reset_events(c);c->failEvent=failures[n];
        if (n==0) {CHECK(!EntityItem_getEntityItem(e));CHECK(!e->watchedItem);}
        else if (n==1) {CHECK(!EntityItem_setEntityItemStack(e,s));CHECK(e->watchedItem==s);}
        else {CHECK(!EntityItem_onCollideWithPlayer(e,(MCObject *)c->players[0]));CHECK(s->stackSize==0);}
        CHECK(MCObjectHeap_failed(h));CHECK(c->events[c->count-1]==(unsigned)failures[n]);
        if (n<5) CHECK(!e->isDead);else CHECK(e->isDead);
        if (n==2||n==3) CHECK(c->count==1);
        if (n==4) CHECK(c->count==2&&c->events[0]==EVENT_SOUND);
        MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
    }
    for (int failure=EVENT_WATCH;failure<=EVENT_DEAD;failure+=EVENT_DEAD-EVENT_WATCH) {
        MCObjectHeap *h=MCObjectHeap_new(16*1024*1024);CHECK(h);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));TestContext *c=setup(h);
        EntityItem *a=entity(c,stack(h,1,2,0)),*b=entity(c,stack(h,1,2,0));reset_events(c);c->failEvent=failure;
        CHECK(!EntityItem_combineItems(a,b));CHECK(MCObjectHeap_failed(h));CHECK(b->watchedItem->stackSize==4&&a->watchedItem->stackSize==2);
        CHECK(a->isDead==(failure==EVENT_DEAD));CHECK(c->count==(failure==EVENT_DEAD?2u:1u));
        MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
    }
}
static void foreign_refs_and_missing_dependency(void) {
    for (unsigned missing=0;missing<12;missing++) {
        MCObjectHeap *h=MCObjectHeap_new(16*1024*1024);CHECK(h);EntityItemDependencies d=dependencies;
        switch(missing) {
            case 0:d.markWatched=NULL;break;case 1:d.logMissingItem=NULL;break;case 2:d.isRemote=NULL;break;
            case 3:d.inventory=NULL;break;case 4:d.name=NULL;break;case 5:d.findPlayer=NULL;break;
            case 6:d.triggerAchievement=NULL;break;case 7:d.isSilent=NULL;break;case 8:d.nextFloat=NULL;break;
            case 9:d.playSoundAtEntity=NULL;break;case 10:d.onItemPickup=NULL;break;default:d.setDead=NULL;break;
        }
        CHECK(!EntityItem_nativeNew(h,NULL,NULL,&d));CHECK(MCObjectHeap_failed(h));MCObjectHeap_free(h);
    }
    for (unsigned operation=0;operation<4;operation++) {
        MCObjectHeap *h=MCObjectHeap_new(16*1024*1024),*foreign=MCObjectHeap_new(16*1024*1024);CHECK(h&&foreign);
        MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));TestContext *c=setup(h);EntityItem *e=entity(c,stack(h,1,1,0));reset_events(c);
        if (operation==0) CHECK(!EntityItem_setEntityItemStack(e,stack(foreign,1,2,0)));
        else if (operation==1) CHECK(!EntityItem_setOwner(e,NBTString_fromASCII(foreign,"Other")));
        else if (operation==2) { TestContext *f=setup(foreign);CHECK(!EntityItem_combineItems(e,entity(f,stack(foreign,1,1,0)))); }
        else { TestContext *f=setup(foreign);CHECK(!EntityItem_onCollideWithPlayer(e,(MCObject *)f->players[0])); }
        CHECK(MCObjectHeap_failed(h)&&!MCObjectHeap_failed(foreign));CHECK(e->watchedItem->stackSize==1&&!e->isDead&&c->count==0);
        MCObjectRootScope_end(&scope);MCObjectHeap_free(h);MCObjectHeap_free(foreign);
    }
}
static void source_nbt(void) {
    const int32_t values[]={INT32_MIN,-32769,-129,-128,-1,0,127,128,255,256,32768,INT32_MAX};
    for(unsigned i=0;i<12;i++) {
        MCObjectHeap *h=MCObjectHeap_new(16*1024*1024);CHECK(h);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));TestContext *c=setup(h);
        ItemStack *s=stack(h,387,values[i],7);NBTTagCompound *itemTag=NBTTagCompound_new(h);CHECK(itemTag&&ItemStack_setTagCompound(s,itemTag));EntityItem *e=entity(c,s);e->health=e->age=e->delayBeforeCanPickup=values[i];
        NBTString *owner=NBTString_fromUTF8(h,"所有者");CHECK(owner&&EntityItem_setOwner(e,owner)&&EntityItem_setThrower(e,owner));NBTTagCompound *out=NBTTagCompound_new(h);CHECK(out&&NBTTagCompound_setInteger_ascii(out,"foreign",99));reset_events(c);
        CHECK(EntityItem_writeEntityToNBT(e,out));uint8_t bits8=(uint8_t)values[i];int8_t byte;memcpy(&byte,&bits8,1);uint16_t bits16=(uint16_t)values[i];int16_t shortValue;memcpy(&shortValue,&bits16,2);
        CHECK(NBTTagCompound_getShort_ascii(out,"Health")==byte&&NBTTagCompound_getShort_ascii(out,"Age")==shortValue&&NBTTagCompound_getShort_ascii(out,"PickupDelay")==shortValue&&NBTTagCompound_getInteger_ascii(out,"foreign")==99);
        CHECK(NBTTagCompound_getString_ascii(out,"Owner")==owner&&NBTTagCompound_getString_ascii(out,"Thrower")==owner);NBTTagCompound *item=NBTTagCompound_getCompoundTag_ascii(out,"Item");CHECK(NBTTagCompound_getTag_ascii(item,"tag")== (NBTBase *)itemTag&&NBTTagCompound_getByte_ascii(item,"Count")==byte&&c->count==0);
        EntityItem *copy=EntityItem_nativeNew(h,(MCObject *)c,(MCObject *)c,&dependencies);CHECK(copy&&EntityItem_isInstance((MCObject *)copy)&&!EntityItem_isInstance((MCObject *)c));CHECK(EntityItem_readEntityFromNBT(copy,out)==ITEMSTACK_NBT_OK);
        CHECK(copy->health==((int16_t)byte&255)&&copy->age==shortValue&&copy->delayBeforeCanPickup==shortValue&&copy->watchedItem&&copy->watchedItem!=s&&copy->watchedItem->stackSize==byte&&copy->watchedItem->stackTagCompound==itemTag&&!copy->isDead);
        CHECK(copy->owner==owner&&copy->thrower==owner&&c->count==1&&c->events[0]==EVENT_WATCH);CHECK(!MCObjectHeap_failed(h));MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
    }
    MCObjectHeap *h=MCObjectHeap_new(16*1024*1024);CHECK(h);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));TestContext *c=setup(h);EntityItem *e=EntityItem_nativeNew(h,(MCObject *)c,(MCObject *)c,&dependencies);CHECK(e);NBTTagCompound *out=NBTTagCompound_new(h);CHECK(out);reset_events(c);
    CHECK(EntityItem_writeEntityToNBT(e,out));CHECK(!e->watchedItem&&c->count==2&&c->events[0]==EVENT_LOG&&c->events[1]==EVENT_LOG&&NBTString_equalsASCII(NBTTagCompound_getString_ascii(NBTTagCompound_getCompoundTag_ascii(out,"Item"),"id"),"minecraft:stone"));
    e->delayBeforeCanPickup=91;NBTString *previous=NBTString_fromASCII(h,"Previous");CHECK(previous);e->owner=e->thrower=previous;NBTTagCompound *empty=NBTTagCompound_new(h);CHECK(empty);reset_events(c);CHECK(EntityItem_readEntityFromNBT(e,empty)==ITEMSTACK_NBT_OK);
    CHECK(e->health==0&&e->age==0&&e->delayBeforeCanPickup==91&&e->owner==previous&&e->thrower==previous&&!e->watchedItem&&!e->isDead);CHECK(c->count==2&&c->events[0]==EVENT_WATCH&&c->events[1]==EVENT_LOG);
    CHECK(NBTTagCompound_setInteger_ascii(empty,"Owner",3)&&NBTTagCompound_setInteger_ascii(empty,"Thrower",4)&&NBTTagCompound_setInteger_ascii(empty,"PickupDelay",-9));reset_events(c);CHECK(EntityItem_readEntityFromNBT(e,empty)==ITEMSTACK_NBT_OK);CHECK(e->delayBeforeCanPickup==-9&&NBTString_length(e->owner)==0&&NBTString_length(e->thrower)==0);CHECK(!MCObjectHeap_failed(h));MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
}
int main(void) {get_set();merge();pickup_cases();partial_and_owner();snapshot_alias();callback_failures();foreign_refs_and_missing_dependency();source_nbt();printf("source entity item: %u checks passed\n",checks);return 0;}
