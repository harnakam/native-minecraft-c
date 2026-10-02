#include "world/storage/MapDataVisibility.h"
#include "item/ItemStackFrame.h"
#include "util/MCGameplayPlayer.h"
#include "util/NativeGameProfile.h"
#include "util/Vec4b.h"
#include "nbt/NBTInternal.h"
#include "world/World.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
static MCObject *foreign_context;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"visibility check %u line %d: %s\n",checks,__LINE__,#x);exit(1); } } while(0)
static MCObjectHeap *heap_new(void) { MCObjectHeap *h=MCObjectHeap_new(32u*1024u*1024u);CHECK(h);return h; }
static MCGameplayPlayer *player_new(MCObjectHeap *h,int id,const char *name) {
    MCGameplayPlayer *p=MCGameplayPlayer_nativeAllocate(h);CHECK(p);
    p->living.entity.entityId=id;
    p->inventory=InventoryPlayer_new(h,(MCObject *)p,MCGameplayPlayer_isCreativeMode);CHECK(p->inventory);
    p->gameProfile=NativeGameProfile_new(h,name?NULL:NativeJavaUUID_new(h,0,id),name?NBTString_fromASCII(h,name):NULL);CHECK(p->gameProfile);
    return p;
}
static ItemStack *map_stack(MCObjectHeap *h,int count,int damage) {
    ItemStack *s=ItemStack_new(h,ItemStack_registryItem(358),count,damage);CHECK(s);return s;
}
static MapData *map_new(MCObjectHeap *h) { MapData *m=MapData_new(h,NULL,NULL,NULL);CHECK(m);return m; }
static MapInfo *info(MapData *m,MCGameplayPlayer *p) {
    MapInfo *i=NULL;CHECK(MapData_getMapInfo(m,p,&i)==WORLD_SAVED_DATA_OK);CHECK(i);return i;
}
static Vec4b *icon(MapData *m,const char *key) {
    return (Vec4b *)NativeLinkedHashMap_get(m->mapDecorations,(MCObject *)NBTString_literalASCII(((MCObject *)m)->heap,key));
}
static void basics(void) {
    MCObjectHeap *h=heap_new();MapData *m=map_new(h);MCGameplayPlayer *p=player_new(h,5,"alice");
    ItemStack *s=map_stack(h,-1,42),*held=map_stack(h,0,42);p->inventory->mainInventory->items[8]=held;
    p->living.entity.posX=3;p->living.entity.posZ=-8;p->living.entity.rotationYaw=90;
    CHECK(MapData_updateVisiblePlayers(m,p,s)==WORLD_SAVED_DATA_OK);
    CHECK(NativeHashMap_size(m->playersHashMap)==1&&NativeReferenceList_size(m->playersArrayList)==1);
    MapInfo *i=(MapInfo *)NativeHashMap_get(m->playersHashMap,(MCObject *)p);CHECK(i&&i->outer==m&&i->entityplayerObj==p);
    CHECK(Vec4b_isInstance((MCObject *)icon(m,"alice")));
    CHECK(Vec4b_func_176112_b(icon(m,"alice"))==6&&Vec4b_func_176113_c(icon(m,"alice"))==-15);
    CHECK(MapData_updateVisiblePlayers(m,p,s)==WORLD_SAVED_DATA_OK);
    CHECK(NativeReferenceList_size(m->playersArrayList)==1);
    MCGameplayPlayer *equal=player_new(h,5,"equal-id");equal->inventory->mainInventory->items[0]=held;
    CHECK(MapData_updateVisiblePlayers(m,equal,s)==WORLD_SAVED_DATA_OK);
    CHECK(NativeReferenceList_size(m->playersArrayList)==1&&i->entityplayerObj==p);
    p->inventory->mainInventory->items[8]=NULL;
    CHECK(MapData_updateVisiblePlayers(m,p,s)==WORLD_SAVED_DATA_OK);
    CHECK(!NativeHashMap_containsKey(m->playersHashMap,(MCObject *)p)&&NativeReferenceList_size(m->playersArrayList)==0);
    CHECK(!icon(m,"alice")&&!MCObjectHeap_failed(h));MCObjectHeap_free(h);

    h=heap_new();m=map_new(h);p=player_new(h,1,NULL);s=map_stack(h,1,0);
    CHECK(NativeHashMap_put(m->playersHashMap,(MCObject *)p,NULL));
    CHECK(MapData_updateVisiblePlayers(m,p,s)==WORLD_SAVED_DATA_OK);
    CHECK(NativeHashMap_containsKey(m->playersHashMap,(MCObject *)p));
    CHECK(NativeReferenceList_size(m->playersArrayList)==0); /* containsKey, not get==NULL */
    CHECK(!MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
static void source_null_prefixes(void) {
    MCObjectHeap *h=heap_new();MapData *m=map_new(h);ItemStack *s=map_stack(h,1,0);
    CHECK(MapData_updateVisiblePlayers(m,NULL,s)==WORLD_SAVED_DATA_EXCEPTION);
    CHECK(NativeHashMap_containsKey(m->playersHashMap,NULL)&&NativeReferenceList_size(m->playersArrayList)==1);
    MapInfo *i=(MapInfo *)NativeReferenceList_get(m->playersArrayList,0);CHECK(i&&i->entityplayerObj==NULL&&!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h=heap_new();m=map_new(h);MCGameplayPlayer *p=player_new(h,2,"empty");
    CHECK(MapData_updateVisiblePlayers(m,p,NULL)==WORLD_SAVED_DATA_EXCEPTION);
    CHECK(NativeHashMap_size(m->playersHashMap)==1&&NativeReferenceList_size(m->playersArrayList)==1&&!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h=heap_new();m=map_new(h);p=player_new(h,3,"armor");s=map_stack(h,1,8);
    p->inventory->armorInventory->items[0]=s;p->inventory->mainInventory=NULL;
    CHECK(MapData_updateVisiblePlayers(m,p,s)==WORLD_SAVED_DATA_OK&&!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h=heap_new();m=map_new(h);p=player_new(h,4,"missing-inventory");p->inventory=NULL;s=map_stack(h,1,0);
    CHECK(MapData_updateVisiblePlayers(m,p,s)==WORLD_SAVED_DATA_EXCEPTION);
    CHECK(NativeHashMap_containsKey(m->playersHashMap,(MCObject *)p)&&NativeReferenceList_size(m->playersArrayList)==1&&!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h=heap_new();m=map_new(h);p=player_new(h,5,"missing-list");m->playersArrayList=NULL;s=map_stack(h,1,0);
    CHECK(MapData_updateVisiblePlayers(m,p,s)==WORLD_SAVED_DATA_EXCEPTION);
    CHECK(NativeHashMap_containsKey(m->playersHashMap,(MCObject *)p)&&!MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
typedef struct Recorder {
    MCObject object;
    MapData *map;
    MCGameplayPlayer *supplied,*replacementPlayer;
    MapInfo *mutatedInfo;
    NativeReferenceList *replacementList;
    NativeLinkedHashMap *replacementDecorations;
    NBTTagCompound *replacementTag;
    NBTTagList *list;
    World *capturedWorld;
    World *firstWorld,*secondWorld;
    EntityItemFrame *frame;
    int calls,failAt,mode,nameCalls,idCalls,updates,frameCalls,tagGets,counts;
    WorldSavedDataResult failureKind;
    char events[4096];
    NBTString *lastId;
    double x,z,rot;
    int type;
} Recorder;
static void recorder_trace(MCObject *o,MCObjectVisitor visit,void *c) {
    Recorder *r=(Recorder *)o;
#define EDGE(field,Type) r->field=(Type *)visit((MCObject *)r->field,c)
    EDGE(map,MapData);EDGE(supplied,MCGameplayPlayer);EDGE(replacementPlayer,MCGameplayPlayer);
    EDGE(mutatedInfo,MapInfo);EDGE(replacementList,NativeReferenceList);EDGE(replacementDecorations,NativeLinkedHashMap);
    EDGE(replacementTag,NBTTagCompound);EDGE(list,NBTTagList);EDGE(capturedWorld,World);EDGE(frame,EntityItemFrame);EDGE(lastId,NBTString);
    EDGE(firstWorld,World);EDGE(secondWorld,World);
#undef EDGE
}
static const MCObjectClass recorderClass={"test.MapVisibility.Recorder",MCObjectHeap_plainClone,recorder_trace,NULL};
static Recorder *recorder(MCObjectHeap *h,MapData *m) {
    Recorder *r=(Recorder *)MCObjectHeap_alloc(h,sizeof(*r),&recorderClass);CHECK(r);r->map=m;r->failureKind=WORLD_SAVED_DATA_EXCEPTION;return r;
}
static WorldSavedDataResult event(Recorder *r,const char *s) {
    CHECK(strlen(r->events)+strlen(s)+2<sizeof(r->events));strcat(r->events,s);strcat(r->events,";");
    r->calls++;return r->failAt==r->calls?r->failureKind:WORLD_SAVED_DATA_OK;
}
static WorldSavedDataResult get_name(MCObject *c,MCGameplayPlayer *p,NBTString **out) {
    Recorder *r=(Recorder *)c;
    if(r->mode==10){r->map->base.nativeContext=foreign_context;return WORLD_SAVED_DATA_EXCEPTION;}
    WorldSavedDataResult z=event(r,"name");if(z!=WORLD_SAVED_DATA_OK)return z;
    ++r->nameCalls;
    if(r->mode==1)r->map->mapDecorations=r->replacementDecorations;
    if(r->mode==2&&r->nameCalls==1)r->mutatedInfo->entityplayerObj=r->replacementPlayer;
    *out=EntityPlayer_getName(p);return WORLD_SAVED_DATA_OK;
}
static int64_t world_time(MCObject *c,WorldInfo *info) {
    Recorder *r=(Recorder *)c;CHECK(event(r,"time")==WORLD_SAVED_DATA_OK);
    r->updates++;r->capturedWorld=r->firstWorld->worldInfo==info?r->firstWorld:r->secondWorld;
    if(r->mode==3&&r->updates==1)r->mutatedInfo->entityplayerObj->living.entity.isDead=true;
    if(r->mode==8&&r->updates==1) {
        NBTTagCompound *t=NBTTagCompound_new(c->heap);CHECK(t);
        CHECK(NBTTagCompound_setString_ascii(t,"id",NBTString_literalASCII(c->heap,"late")));
        CHECK(NBTTagList_appendTag(r->list,(NBTBase *)t));
    }
    return 0;
}
static const WorldInfoVirtualMethods infoMethods={.getWorldTime=world_time};
static World *recorded_world(MCObjectHeap *h,Recorder *r) {
    World *w=World_nativeAllocate(h,NULL,NULL);CHECK(w);
    w->worldInfo=WorldInfo_nativeAllocate(h,&infoMethods,(MCObject *)r);CHECK(w->worldInfo);return w;
}
static WorldSavedDataResult get_size(MCObject *c,NativeReferenceList *l,int32_t *out) {
    Recorder *r=(Recorder *)c;WorldSavedDataResult z=event(r,"size");if(z!=WORLD_SAVED_DATA_OK)return z;
    *out=NativeReferenceList_size(l);if(r->mode==4)r->map->playersArrayList=r->replacementList;return WORLD_SAVED_DATA_OK;
}
static WorldSavedDataResult get_index(MCObject *c,NativeReferenceList *l,int32_t i,MCObject **out) {
    Recorder *r=(Recorder *)c;WorldSavedDataResult z=WORLD_SAVED_DATA_OK;
    if(r->mode!=4||l!=r->replacementList)z=event(r,"get");
    if(z!=WORLD_SAVED_DATA_OK)return z;
    NativeArrayResult a=NativeReferenceList_getSource(l,i,out);
    return a==NATIVE_ARRAY_OK?WORLD_SAVED_DATA_OK:a==NATIVE_ARRAY_EXCEPTION?WORLD_SAVED_DATA_EXCEPTION:WORLD_SAVED_DATA_FAILURE;
}
static void live_list(void) {
    MCObjectHeap *h=heap_new();MapData *m=map_new(h);ItemStack *s=map_stack(h,1,2);
    MCGameplayPlayer *a=player_new(h,1,"dead-a"),*b=player_new(h,2,"dead-b"),*p=player_new(h,3,"live");
    a->living.entity.isDead=b->living.entity.isDead=true;p->inventory->mainInventory->items[0]=s;
    info(m,a);MapInfo *bi=info(m,b);info(m,p);
    CHECK(MapData_updateVisiblePlayers(m,p,s)==WORLD_SAVED_DATA_OK);
    CHECK(NativeReferenceList_size(m->playersArrayList)==2&&NativeReferenceList_get(m->playersArrayList,0)==(MCObject *)bi);
    CHECK(!NativeHashMap_containsKey(m->playersHashMap,(MCObject *)a)&&NativeHashMap_containsKey(m->playersHashMap,(MCObject *)b));
    MCObjectHeap_free(h);

    h=heap_new();m=map_new(h);s=map_stack(h,1,0);p=player_new(h,1,"duplicate");a=player_new(h,2,"other");
    p->inventory->mainInventory->items[0]=s;a->inventory->mainInventory->items[0]=s;
    MapInfo *pi=info(m,p);info(m,a);CHECK(NativeReferenceList_add(m->playersArrayList,(MCObject *)pi));
    Recorder *r=recorder(h,m);r->mode=3;r->mutatedInfo=pi;
    r->firstWorld=recorded_world(h,r);p->living.entity.worldObj=(MCObject *)r->firstWorld;
    m->dimension=-1;p->living.entity.dimension=-1; /* reached actual WorldInfo getter */
    MapDataVisibilityDependencies d={0};
    CHECK(MapData_updateVisiblePlayersWithDependencies(m,p,s,&d,(MCObject *)r)==WORLD_SAVED_DATA_OK);
    CHECK(NativeReferenceList_size(m->playersArrayList)==2);
    CHECK(NativeReferenceList_get(m->playersArrayList,1)==(MCObject *)pi); /* removes FIRST equal */
    CHECK(!NativeHashMap_containsKey(m->playersHashMap,(MCObject *)p)&&!MCObjectHeap_failed(h));MCObjectHeap_free(h);

    h=heap_new();m=map_new(h);s=map_stack(h,1,0);p=player_new(h,1,"reread");p->inventory->mainInventory->items[0]=s;
    r=recorder(h,m);r->mode=4;r->replacementList=NativeReferenceList_new(h);CHECK(r->replacementList);
    d=(MapDataVisibilityDependencies){.listSize=get_size,.listGet=get_index};
    CHECK(MapData_updateVisiblePlayersWithDependencies(m,p,s,&d,(MCObject *)r)==WORLD_SAVED_DATA_EXCEPTION);
    CHECK(!strcmp(r->events,"size;")&&NativeHashMap_containsKey(m->playersHashMap,(MCObject *)p));
    CHECK(!MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
static void captures(void) {
    MCObjectHeap *h=heap_new();MapData *m=map_new(h);MCGameplayPlayer *p=player_new(h,1,"remove");ItemStack *s=map_stack(h,1,0);
    NativeLinkedHashMap *old=m->mapDecorations;NBTString *key=NBTString_literalASCII(h,"remove");
    CHECK(NativeLinkedHashMap_put(old,(MCObject *)key,NULL));Recorder *r=recorder(h,m);r->mode=1;
    r->replacementDecorations=NativeLinkedHashMap_new(h);CHECK(r->replacementDecorations);
    CHECK(NativeLinkedHashMap_put(r->replacementDecorations,(MCObject *)key,NULL));
    MapDataVisibilityDependencies d={.playerGetName=get_name};
    CHECK(MapData_updateVisiblePlayersWithDependencies(m,p,s,&d,(MCObject *)r)==WORLD_SAVED_DATA_OK);
    CHECK(!NativeLinkedHashMap_containsKey(old,(MCObject *)key));
    CHECK(NativeLinkedHashMap_containsKey(m->mapDecorations,(MCObject *)key));MCObjectHeap_free(h);

    h=heap_new();m=map_new(h);p=player_new(h,1,"old-player");s=map_stack(h,1,0);p->inventory->mainInventory->items[0]=s;
    MCGameplayPlayer *q=player_new(h,2,"new-player");q->living.entity.posX=17;q->living.entity.posZ=25;q->living.entity.rotationYaw=-45;
    r=recorder(h,m);r->mode=2;r->replacementPlayer=q;r->mutatedInfo=info(m,p);
    World *w1=recorded_world(h,r),*w2=recorded_world(h,r);r->firstWorld=w1;r->secondWorld=w2;
    p->living.entity.worldObj=(MCObject *)w1;q->living.entity.worldObj=(MCObject *)w2;m->dimension=-1;p->living.entity.dimension=-1;
    d=(MapDataVisibilityDependencies){.playerGetName=get_name};
    CHECK(MapData_updateVisiblePlayersWithDependencies(m,p,s,&d,(MCObject *)r)==WORLD_SAVED_DATA_OK);
    CHECK(r->capturedWorld==w1&&r->updates==1);
    CHECK(icon(m,"old-player")&&Vec4b_func_176112_b(icon(m,"old-player"))==34&&Vec4b_func_176113_c(icon(m,"old-player"))==50);
    CHECK(r->mutatedInfo->entityplayerObj==q&&!MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
static WorldSavedDataResult get_id(MCObject *c,NBTTagCompound *t,const char *key,NBTString **out) {
    Recorder *r=(Recorder *)c;WorldSavedDataResult z=event(r,"id");if(z!=WORLD_SAVED_DATA_OK)return z;
    ++r->idCalls;*out=NBTTagCompound_getString_ascii(t,key);
    if(r->mode==5&&r->idCalls==2) {
        *out=NBTString_literalASCII(c->heap,"second-id");r->supplied->living.entity.worldObj=(MCObject *)r->capturedWorld;
    }
    return WORLD_SAVED_DATA_OK;
}
static WorldSavedDataResult get_byte(MCObject *c,NBTTagCompound *t,const char *key,int32_t *out) {
    WorldSavedDataResult z=event((Recorder *)c,"type");if(z==WORLD_SAVED_DATA_OK)*out=NBTTagCompound_getByte_ascii(t,key);return z;
}
static WorldSavedDataResult get_double(MCObject *c,NBTTagCompound *t,const char *key,double *out) {
    WorldSavedDataResult z=event((Recorder *)c,key);if(z==WORLD_SAVED_DATA_OK)*out=NBTTagCompound_getDouble_ascii(t,key);return z;
}
static WorldSavedDataResult get_list(MCObject *c,NBTTagCompound *t,const char *key,int32_t kind,NBTTagList **out) {
    Recorder *r=(Recorder *)c;WorldSavedDataResult z=event(r,"list");if(z!=WORLD_SAVED_DATA_OK)return z;
    r->tagGets++;*out=NBTTagCompound_getTagList_ascii(t,key,kind);return WORLD_SAVED_DATA_OK;
}
static WorldSavedDataResult has_key(MCObject *c,NBTTagCompound *t,const char *key,int32_t type,bool *out) {
    Recorder *r=(Recorder *)c;WorldSavedDataResult z=event(r,"hasKey");if(z!=WORLD_SAVED_DATA_OK)return z;
    *out=NBTTagCompound_hasKeyType_ascii(t,key,type);
    r->tagGets++;
    if(r->mode==6)r->supplied->inventory->mainInventory->items[0]->stackTagCompound=r->replacementTag;
    return WORLD_SAVED_DATA_OK;
}
static WorldSavedDataResult tag_count(MCObject *c,NBTTagList *l,int32_t *out) {
    Recorder *r=(Recorder *)c;WorldSavedDataResult z=event(r,"count");if(z==WORLD_SAVED_DATA_OK){r->counts++;*out=NBTTagList_tagCount(l);}return z;
}
static NBTTagCompound *decorations_tag(MCObjectHeap *h,const char *id,NBTTagList **list) {
    NBTTagCompound *tag=NBTTagCompound_new(h),*entry=NBTTagCompound_new(h);CHECK(tag&&entry);
    *list=NBTTagList_new(h);CHECK(*list);
    CHECK(NBTTagCompound_setString_ascii(entry,"id",NBTString_literalASCII(h,id)));
    CHECK(NBTTagCompound_setByte_ascii(entry,"type",3));CHECK(NBTTagCompound_setDouble_ascii(entry,"x",8));
    CHECK(NBTTagCompound_setDouble_ascii(entry,"z",12));CHECK(NBTTagCompound_setDouble_ascii(entry,"rot",180));
    CHECK(NBTTagList_appendTag(*list,(NBTBase *)entry));CHECK(NBTTagCompound_setTag_ascii(tag,"Decorations",(NBTBase *)*list));return tag;
}
static void decorations(void) {
    for(int mode=0;mode<4;mode++) {
        MCObjectHeap *h=heap_new();MapData *m=map_new(h);MCGameplayPlayer *p=player_new(h,1,"player");ItemStack *s=map_stack(h,1,0);
        CHECK(NativeHashMap_put(m->playersHashMap,(MCObject *)p,NULL));
        Recorder *r=recorder(h,m);r->supplied=p;r->mode=mode==1?5:mode==2?6:mode==3?8:0;
        s->stackTagCompound=decorations_tag(h,"first-id",&r->list);
        if(mode==1||mode==3) {
            r->firstWorld=recorded_world(h,r);r->secondWorld=recorded_world(h,r);
            p->living.entity.worldObj=(MCObject *)r->firstWorld;r->capturedWorld=r->secondWorld;m->dimension=-1;
        }
        if(mode==2) {NBTTagList *ignored=NULL;r->replacementTag=decorations_tag(h,"replacement",&ignored);p->inventory->mainInventory->items[0]=s;}
        MapDataVisibilityDependencies d={.tagGetString=get_id,.tagGetByte=get_byte,.tagGetDouble=get_double,
            .tagGetTagList=get_list,.tagHasKeyType=has_key,.tagCount=tag_count};
        CHECK(MapData_updateVisiblePlayersWithDependencies(m,p,s,&d,(MCObject *)r)==WORLD_SAVED_DATA_OK);
        CHECK(r->tagGets==2&&r->idCalls==(mode==3?4:2));
        CHECK(r->counts==(mode==3?3:2));
        CHECK(icon(m,mode==1?"second-id":mode==2?"replacement":"first-id"));
        if(mode==1)CHECK(r->capturedWorld!=(World *)p->living.entity.worldObj);
        if(mode==3)CHECK(icon(m,"late"));
        CHECK(!MCObjectHeap_failed(h));MCObjectHeap_free(h);
    }
    MCObjectHeap *h=heap_new();MapData *m=map_new(h);MCGameplayPlayer *p=player_new(h,1,"player");ItemStack *s=map_stack(h,1,0);
    CHECK(NativeHashMap_put(m->playersHashMap,(MCObject *)p,NULL));Recorder *r=recorder(h,m);
    s->stackTagCompound=decorations_tag(h,"present-null",&r->list);
    CHECK(NativeLinkedHashMap_put(m->mapDecorations,(MCObject *)NBTString_literalASCII(h,"present-null"),NULL));
    MapDataVisibilityDependencies d={.tagGetString=get_id,.tagGetByte=get_byte,.tagGetDouble=get_double};
    CHECK(MapData_updateVisiblePlayersWithDependencies(m,p,s,&d,(MCObject *)r)==WORLD_SAVED_DATA_OK);
    CHECK(r->idCalls==1&&r->updates==0&&!strcmp(r->events,"id;"));MCObjectHeap_free(h);
}
static WorldSavedDataResult frame_eid(MCObject *c,EntityItemFrame *f,int32_t *out) {
    Recorder *r=(Recorder *)c;WorldSavedDataResult z=event(r,"entityId");if(z!=WORLD_SAVED_DATA_OK)return z;
    *out=Entity_getEntityId(&f->hanging.entity);
    if(r->mode==9) {f->hanging.hangingPosition=BlockPos_newDouble(c->heap,40,0,50);CHECK(f->hanging.hangingPosition);f->hanging.facingDirection=&NativeHangingFacing_EAST;}
    return WORLD_SAVED_DATA_OK;
}
static void frames(void) {
    MCObjectHeap *h=heap_new();MapData *m=map_new(h);MCGameplayPlayer *p=player_new(h,1,"frame-owner");ItemStack *s=map_stack(h,1,0);
    EntityItemFrame *f=EntityItemFrame_nativeAllocate(h,NULL,NULL,NULL,NULL);CHECK(f);
    CHECK(EntityItemFrame_construct(f,NULL,NULL,NULL,NativeJavaRandomRuntime_process(),NativeEntityIDRuntime_process())==ENTITY_FRAME_OK);
    f->hanging.entity.entityId=INT32_MIN;f->hanging.hangingPosition=BlockPos_newDouble(h,4,6,8);CHECK(f->hanging.hangingPosition);
    f->hanging.facingDirection=&NativeHangingFacing_NORTH;CHECK(ItemStack_setItemFrame(s,f));
    Recorder *r=recorder(h,m);r->mode=9;r->frame=f;
    MapDataVisibilityDependencies d={.frameGetEntityId=frame_eid};
    CHECK(MapData_updateVisiblePlayersWithDependencies(m,p,s,&d,(MCObject *)r)==WORLD_SAVED_DATA_OK);
    CHECK(!strcmp(r->events,"entityId;"));
    Vec4b *v=icon(m,"frame--2147483648");CHECK(v&&Vec4b_func_176110_a(v)==1&&Vec4b_func_176112_b(v)==8&&Vec4b_func_176113_c(v)==16&&Vec4b_func_176111_d(v)==12);
    CHECK(NativeHashMap_containsKey(m->playersHashMap,(MCObject *)p)&&icon(m,"frame--2147483648"));
    f->hanging.facingDirection=NULL;
    CHECK(MapData_updateVisiblePlayers(m,p,s)==WORLD_SAVED_DATA_EXCEPTION&&!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void failures_and_lifetime(void) {
    for(int failAt=1;failAt<=4;failAt++) {
        MCObjectHeap *h=heap_new();MapData *m=map_new(h);MCGameplayPlayer *p=player_new(h,1,"failure");ItemStack *s=map_stack(h,1,0);p->inventory->mainInventory->items[0]=s;
        Recorder *r=recorder(h,m);r->failAt=failAt;
        MapDataVisibilityDependencies d={.playerGetName=get_name,.listSize=get_size,.listGet=get_index};
        WorldSavedDataResult z=MapData_updateVisiblePlayersWithDependencies(m,p,s,&d,(MCObject *)r);
        CHECK(z==WORLD_SAVED_DATA_EXCEPTION&&r->calls==failAt&&!MCObjectHeap_failed(h));
        CHECK(NativeHashMap_containsKey(m->playersHashMap,(MCObject *)p));MCObjectHeap_free(h);
    }
    MCObjectHeap *h=heap_new();MapData *m=map_new(h);MCGameplayPlayer *p=player_new(h,1,"alias");ItemStack *s=map_stack(h,1,0);p->inventory->mainInventory->items[0]=s;
    CHECK(MapData_updateVisiblePlayers(m,p,s)==WORLD_SAVED_DATA_OK);MapInfo *i=info(m,p);
    CHECK(NativeReferenceList_add(m->playersArrayList,(MCObject *)i));
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)m));CHECK(MCObjectHeap_collect(h));
    MCObjectHeap *copy=MCObjectHeap_clone(h);CHECK(copy);MCObjectRoot other={0};CHECK(MCObjectRoot_rebind(&other,copy,&root));
    MapData *cm=(MapData *)MCObjectRoot_get(&other);MapInfo *ci=(MapInfo *)NativeReferenceList_get(cm->playersArrayList,0);
    CHECK(ci!=i&&ci->outer==cm&&NativeReferenceList_get(cm->playersArrayList,1)==(MCObject *)ci);
    CHECK(NativeHashMap_get(cm->playersHashMap,(MCObject *)ci->entityplayerObj)==(MCObject *)ci);
    CHECK(ci->entityplayerObj->inventory->mainInventory->items[0]!=s);
    CHECK(MapData_updateVisiblePlayers(cm,ci->entityplayerObj,ci->entityplayerObj->inventory->mainInventory->items[0])==WORLD_SAVED_DATA_OK);
    CHECK(MCObjectHeap_adopt(h,copy));m=(MapData *)MCObjectRoot_get(&root);CHECK(m==cm&&MCObjectHeap_collect(h));
    MCObjectHeap_free(copy);MCObjectRoot_drop(&root);MCObjectHeap_free(h);

    h=heap_new();m=map_new(h);p=player_new(h,1,"wrong-cast");s=map_stack(h,1,0);p->inventory->mainInventory->items[0]=s;
    CHECK(NativeHashMap_put(m->playersHashMap,(MCObject *)p,NULL));
    CHECK(NativeReferenceList_add(m->playersArrayList,(MCObject *)NBTString_literalASCII(h,"not MapInfo")));
    CHECK(MapData_updateVisiblePlayers(m,p,s)==WORLD_SAVED_DATA_EXCEPTION&&!MCObjectHeap_failed(h));MCObjectHeap_free(h);
    h=heap_new();m=map_new(h);p=player_new(h,1,"malformed");s=map_stack(h,1,0);p->inventory->mainInventory->items[0]=s;i=info(m,p);
    MCObject *small=MCObjectHeap_alloc(h,sizeof(MCObject),((MCObject *)i)->klass);CHECK(small);
    CHECK(NativeReferenceList_set(m->playersArrayList,0,small)==(MCObject *)i);
    CHECK(MapData_updateVisiblePlayers(m,p,s)==WORLD_SAVED_DATA_FAILURE&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
    h=heap_new();m=map_new(h);p=player_new(h,1,"fake-context");s=map_stack(h,1,0);
    MCObject fake={h,&recorderClass};CHECK(MapData_updateVisiblePlayersWithDependencies(m,p,s,NULL,&fake)==WORLD_SAVED_DATA_FAILURE&&MCObjectHeap_failed(h));MCObjectHeap_free(h);

    /* Every NBT virtual call is allowed to throw after the same real registration
       and iterator prefix. No rollback erases completed Source mutations. */
    for(int at=1;at<=10;at++) {
        h=heap_new();m=map_new(h);p=player_new(h,1,"nbt-failure");s=map_stack(h,1,0);
        CHECK(NativeHashMap_put(m->playersHashMap,(MCObject *)p,NULL));
        Recorder *r=recorder(h,m);s->stackTagCompound=decorations_tag(h,"entry",&r->list);r->failAt=at;
        MapDataVisibilityDependencies d={.tagGetTagList=get_list,.tagHasKeyType=has_key,
            .tagCount=tag_count,.tagGetString=get_id,.tagGetByte=get_byte,.tagGetDouble=get_double};
        CHECK(MapData_updateVisiblePlayersWithDependencies(m,p,s,&d,(MCObject *)r)==WORLD_SAVED_DATA_EXCEPTION);
        CHECK(r->calls==at&&!MCObjectHeap_failed(h));
        CHECK(NativeLinkedHashMap_size(m->mapDecorations)==(at==10?1:0));
        MCObjectHeap_free(h);
    }
    h=MCObjectHeap_new(65536);CHECK(h);m=map_new(h);p=player_new(h,1,"oom");s=map_stack(h,1,0);
    size_t remaining=65536-MCObjectHeap_liveBytes(h);CHECK(remaining>sizeof(MCObject)+1);
    CHECK(MCObjectHeap_alloc(h,remaining-1,&recorderClass));
    CHECK(MapData_updateVisiblePlayers(m,p,s)==WORLD_SAVED_DATA_FAILURE&&MCObjectHeap_failed(h));
    /* Allocation fails before either registration store. Avoid invoking a
       collection accessor on the deliberately failed native heap. */
    CHECK(m->playersArrayList->size==0);MCObjectHeap_free(h);
}
static void native_boundary_guards(void) {
    MCObjectHeap *h=heap_new(),*foreign=heap_new();MapData *m=map_new(h);MCGameplayPlayer *p=player_new(h,1,"context");ItemStack *s=map_stack(h,1,0);
    p->inventory->mainInventory->items[0]=s;Recorder *r=recorder(h,m);r->mode=10;
    foreign_context=(MCObject *)map_new(foreign);
    MapDataVisibilityDependencies d={.playerGetName=get_name};
    CHECK(MapData_updateVisiblePlayersWithDependencies(m,p,s,&d,(MCObject *)r)==WORLD_SAVED_DATA_FAILURE);
    CHECK(MCObjectHeap_failed(h)&&NativeReferenceList_isInstance((MCObject *)m->playersArrayList)&&m->playersArrayList->size==1);
    m->base.nativeContext=NULL;MCObjectHeap_free(h);MCObjectHeap_free(foreign);foreign_context=NULL;
    for(int mode=0;mode<4;mode++) {
        h=heap_new();m=map_new(h);p=player_new(h,1,"malformed-payload");s=map_stack(h,1,0);
        if(mode==0) {
            MCObject *tiny=MCObjectHeap_alloc(h,sizeof(MCObject),s->object.klass);CHECK(tiny);
            p->inventory->mainInventory->items[0]=(ItemStack *)tiny;
        } else if(mode==1) {
            MCObject *tiny=MCObjectHeap_alloc(h,sizeof(MCObject),p->inventory->object.klass);CHECK(tiny);p->inventory=(InventoryPlayer *)tiny;
        } else if(mode==2) {
            MCObject *tiny=MCObjectHeap_alloc(h,sizeof(MCObject),p->inventory->armorInventory->object.klass);CHECK(tiny);
            p->inventory->armorInventory=(ItemStackArray *)tiny;
        } else p->inventory->armorInventory->length=INT32_MAX;
        CHECK(MapData_updateVisiblePlayers(m,p,s)==WORLD_SAVED_DATA_FAILURE&&MCObjectHeap_failed(h));
        CHECK(m->playersArrayList->size==1);MCObjectHeap_free(h);
    }
    h=heap_new();m=map_new(h);p=player_new(h,1,"unreached-malformed-main");s=map_stack(h,1,0);
    p->inventory->armorInventory->items[0]=s;p->inventory->mainInventory->length=INT32_MAX;
    CHECK(MapData_updateVisiblePlayers(m,p,s)==WORLD_SAVED_DATA_OK&&!MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
int main(void) {
    native_boundary_guards();basics();source_null_prefixes();live_list();captures();decorations();frames();failures_and_lifetime();
    printf("Source map visibility: %u checks passed\n",checks);return 0;
}
