#include "world/border/WorldBorder.h"
#include "util/MCGameplayPlayer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"world border %u line %d: %s\n",checks,__LINE__,#x);exit(1); } } while(0)
static void original_initial_border(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);
    WorldBorder *border=WorldBorder_new(heap,NULL,NULL);CHECK(border);
    CHECK(WorldBorder_getStatus(border)==WORLD_BORDER_STATIONARY);
    double distance=0;CHECK(WorldBorder_getClosestDistance(border,0,0,&distance));
    CHECK(distance==29999984.0&&!MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}
static void managed_list_exists(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);
    NativeReferenceList *list=NativeReferenceList_new(heap);CHECK(list);
    MCObjectHeap_free(heap);
}
typedef struct {MCObject object;NativeReferenceList *owner;int32_t value;} Token;
static void token_trace(MCObject *o,MCObjectVisitor v,void *c) {
    Token *t=(Token *)o;t->owner=(NativeReferenceList *)v((MCObject *)t->owner,c);
}
static const MCObjectClass tokenClass={"fixture.border.token",MCObjectHeap_plainClone,token_trace,NULL};
static Token *token(MCObjectHeap *h,int32_t value) {
    Token *t=(Token *)MCObjectHeap_alloc(h,sizeof(*t),&tokenClass);CHECK(t);t->value=value;return t;
}
static uint64_t bits(double d) {uint64_t u;memcpy(&u,&d,sizeof u);return u;}
static void ordered_nullable_list_and_graph(void) {
    MCObjectHeap *h=MCObjectHeap_new(8*1024*1024);CHECK(h);
    NativeReferenceList *l=NativeReferenceList_new(h);CHECK(l&&NativeReferenceList_size(l)==0);
    Token *a=token(h,7);a->owner=l;
    CHECK(NativeReferenceList_add(l,NULL)&&NativeReferenceList_add(l,(MCObject *)a));
    CHECK(NativeReferenceList_add(l,(MCObject *)a)&&NativeReferenceList_add(l,(MCObject *)l));
    for(int32_t i=0;i<1024;i++)CHECK(NativeReferenceList_add(l,i%2?(MCObject *)a:NULL));
    CHECK(NativeReferenceList_size(l)==1028&&NativeReferenceList_get(l,0)==NULL);
    CHECK(NativeReferenceList_get(l,1)==(MCObject *)a&&NativeReferenceList_get(l,2)==(MCObject *)a);
    CHECK(NativeReferenceList_set(l,0,(MCObject *)a)==NULL&&!MCObjectHeap_failed(h));
    CHECK(NativeReferenceList_remove(l,2)==(MCObject *)a&&NativeReferenceList_size(l)==1027);
    CHECK(NativeReferenceList_get(l,2)==(MCObject *)l);
    NativeReferenceList *snapshot=NativeReferenceList_copy(l);CHECK(snapshot&&snapshot!=l&&snapshot->storage!=l->storage);
    CHECK(NativeReferenceList_get(snapshot,0)==(MCObject *)a&&NativeReferenceList_get(snapshot,2)==(MCObject *)l);
    CHECK(NativeReferenceList_set(l,0,NULL)==(MCObject *)a&&NativeReferenceList_get(snapshot,0)==(MCObject *)a);
    CHECK(NativeReferenceList_add(l,(MCObject *)snapshot));
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)l));CHECK(MCObjectHeap_collect(h));
    CHECK(NativeReferenceList_get(snapshot,0)==(MCObject *)a&&a->owner==l);
    MCObjectHeap *branch=MCObjectHeap_clone(h);CHECK(branch);MCObjectRoot clonedRoot={0};
    CHECK(MCObjectRoot_rebind(&clonedRoot,branch,&root));
    NativeReferenceList *cloned=(NativeReferenceList *)MCObjectRoot_get(&clonedRoot);
    CHECK(cloned&&cloned!=l&&cloned->storage!=l->storage&&NativeReferenceList_get(cloned,2)==(MCObject *)cloned);
    Token *ca=(Token *)NativeReferenceList_get(cloned,1);CHECK(ca&&ca!=a&&ca->value==7&&ca->owner==cloned);
    NativeReferenceList *cs=(NativeReferenceList *)NativeReferenceList_get(cloned,1027);
    CHECK(cs&&cs!=snapshot&&NativeReferenceList_get(cs,0)==(MCObject *)ca&&NativeReferenceList_get(cs,2)==(MCObject *)cloned);
    CHECK(MCObjectHeap_collect(branch)&&MCObjectHeap_adopt(h,branch));MCObjectHeap_free(branch);
    l=(NativeReferenceList *)MCObjectRoot_get(&root);CHECK(l==cloned&&NativeReferenceList_get(l,1)==(MCObject *)ca);
    CHECK(NativeReferenceList_clear(l)&&NativeReferenceList_size(l)==0);CHECK(MCObjectHeap_collect(h));
    CHECK(MCObjectHeap_liveObjects(h)==2);MCObjectRoot_drop(&root);CHECK(MCObjectHeap_collect(h)&&!MCObjectHeap_liveObjects(h));
    MCObjectHeap_free(h);
}
static void list_guards_and_failure_preservation(void) {
    for(unsigned mode=0;mode<8;mode++) {
        MCObjectHeap *h=MCObjectHeap_new(1024*1024),*other=MCObjectHeap_new(65536);CHECK(h&&other);
        NativeReferenceList *l=NativeReferenceList_new(h);CHECK(l);Token *a=token(h,3),*foreign=token(other,4);
        CHECK(NativeReferenceList_add(l,(MCObject *)a));
        int32_t before=l->size;NativeReferenceArray *storage=l->storage;
        if(mode==0)CHECK(!NativeReferenceList_add(l,(MCObject *)foreign));
        if(mode==1)CHECK(!NativeReferenceList_set(l,0,(MCObject *)foreign));
        if(mode==2)CHECK(!NativeReferenceList_get(l,-1));
        if(mode==3)CHECK(!NativeReferenceList_remove(l,1));
        if(mode==4)CHECK(!NativeReferenceList_set(l,1,NULL));
        if(mode==5){l->size=INT32_MAX;CHECK(NativeReferenceList_size(l)==-1);l->size=before;}
        if(mode==6){int32_t cap=storage->capacity;storage->capacity=INT32_MAX;CHECK(!NativeReferenceList_copy(l));storage->capacity=cap;}
        if(mode==7){MCObject *shortList=MCObjectHeap_alloc(h,sizeof(MCObject),l->object.klass);CHECK(shortList);CHECK(!NativeReferenceList_clear((NativeReferenceList *)shortList));}
        CHECK(MCObjectHeap_failed(h)&&!MCObjectHeap_failed(other)&&l->size==before&&l->storage==storage&&storage->items[0]==(MCObject *)a);
        CHECK(!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);MCObjectHeap_free(other);
    }
    MCObjectHeap *h=MCObjectHeap_new(65536);CHECK(h);NativeReferenceList *l=NativeReferenceList_new(h);CHECK(l);
    while(l->size<l->storage->capacity)CHECK(NativeReferenceList_add(l,NULL));
    int32_t before=l->size;NativeReferenceArray *storage=l->storage;
    CHECK(MCObjectHeap_alloc(h,65536-MCObjectHeap_liveBytes(h),&tokenClass));
    CHECK(!NativeReferenceList_add(l,NULL)&&MCObjectHeap_failed(h)&&l->size==before&&l->storage==storage);
    CHECK(!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);
}
typedef struct {
    MCObject object;WorldBorder *border;MCObject *replacement;Entity *positionTarget;
    int64_t now,step;unsigned clocks,failClock,events,failListener;
    int32_t ids[24];double values[24];unsigned mutation;
    bool mutateClock;
} Recorder;
typedef struct {MCObject object;Recorder *recorder;int32_t id;} Listener;
static void recorder_trace(MCObject *o,MCObjectVisitor v,void *c) {
    Recorder *r=(Recorder *)o;r->border=(WorldBorder *)v((MCObject *)r->border,c);r->replacement=v(r->replacement,c);
    r->positionTarget=(Entity *)v((MCObject *)r->positionTarget,c);
}
static void listener_trace(MCObject *o,MCObjectVisitor v,void *c) {
    Listener *l=(Listener *)o;l->recorder=(Recorder *)v((MCObject *)l->recorder,c);
}
static const MCObjectClass recorderClass={"fixture.border.clock",MCObjectHeap_plainClone,recorder_trace,NULL};
static const MCObjectClass listenerClass={"fixture.border.listener",MCObjectHeap_plainClone,listener_trace,NULL};
static bool clock_ms(MCObject *ctx,int64_t *out) {
    Recorder *r=(Recorder *)ctx;CHECK(r&&r->object.klass==&recorderClass);
    CHECK(MCObjectHeap_hasBorrowers(ctx->heap)&&!MCObjectHeap_collect(ctx->heap));
    ++r->clocks;if(r->failClock==r->clocks)return false;
    *out=r->now;
    uint64_t next=(uint64_t)r->now+(uint64_t)r->step;memcpy(&r->now,&next,sizeof next);
    if(r->mutateClock){r->border->startTime=100;r->border->endTime=500;r->border->startDiameter=24;r->border->endDiameter=48;r->border->centerX=99;}
    if(r->positionTarget){r->positionTarget->posX=200;r->positionTarget->posZ=-100;MCObjectHeap_touch(ctx->heap);}
    return true;
}
static bool size_changed(MCObject *ctx,MCObject *o,WorldBorder *b,double size) {
    Recorder *r=(Recorder *)ctx;Listener *l=(Listener *)o;
    CHECK(l&&l->object.klass==&listenerClass&&l->recorder==r&&r->border==b&&o->heap==ctx->heap);
    CHECK(MCObjectHeap_hasBorrowers(ctx->heap)&&!MCObjectHeap_collect(ctx->heap));
    CHECK(r->events<24);r->ids[r->events]=l->id;r->values[r->events++]=size;
    if(r->mutation&&r->events==1) {
        CHECK(NativeReferenceList_clear(b->listeners)&&WorldBorder_addListener(b,r->replacement));
        if(r->mutation==2)CHECK(WorldBorder_setTransition(b,12));
    }
    return r->events!=r->failListener;
}
static bool transition_started(MCObject *ctx,MCObject *o,WorldBorder *b,double old,double size,int64_t time) {
    CHECK(old==20&&size==40&&time==100);return size_changed(ctx,o,b,size);
}
static bool center_changed(MCObject *ctx,MCObject *o,WorldBorder *b,double x,double z) {
    CHECK(x==7&&z==-9);return size_changed(ctx,o,b,x);
}
static const WorldBorderDependencies dependencies={clock_ms,size_changed,transition_started,center_changed};
static Recorder *recorder(MCObjectHeap *h) {
    Recorder *r=(Recorder *)MCObjectHeap_alloc(h,sizeof(*r),&recorderClass);CHECK(r);
    r->border=WorldBorder_new(h,&dependencies,(MCObject *)r);CHECK(r->border);return r;
}
static Listener *listener(Recorder *r,int32_t id) {
    Listener *l=(Listener *)MCObjectHeap_alloc(r->object.heap,sizeof(*l),&listenerClass);CHECK(l);l->recorder=r;l->id=id;return l;
}
static void source_geometry_clock_and_ieee(void) {
    MCObjectHeap *h=MCObjectHeap_new(1024*1024);CHECK(h);Recorder *r=recorder(h);WorldBorder *b=r->border;
    CHECK(!r->clocks&&NativeReferenceList_size(b->listeners)==0&&b->damageAmount==0.2&&b->damageBuffer==5&&b->warningTime==15&&b->warningDistance==5);
    double out=77;CHECK(WorldBorder_minX(b,&out)&&out==-29999984);
    CHECK(WorldBorder_maxZ(b,&out)&&out==29999984&&!r->clocks);
    CHECK(WorldBorder_setCenter(b,7,-9)&&!r->events&&WorldBorder_getCenterX(b)==7&&WorldBorder_getCenterZ(b)==-9);
    CHECK(WorldBorder_setSize(b,-5)&&WorldBorder_getSize(b)==-5);
    CHECK(WorldBorder_minX(b,&out)&&out==5&&WorldBorder_maxX(b,&out)&&out==-5);
    CHECK(WorldBorder_setSize(b,INT32_MIN));CHECK(WorldBorder_minX(b,&out)&&out==-29999993);
    CHECK(WorldBorder_maxX(b,&out)&&out==-2147483648.0);
    CHECK(WorldBorder_setSize(b,29999984));b->centerX=b->centerZ=0;b->startDiameter=b->endDiameter=0;
    CHECK(WorldBorder_getClosestDistance(b,-0.0,-0.0,&out)&&bits(out)==UINT64_C(0x8000000000000000));
    CHECK(WorldBorder_getClosestDistance(b,NAN,0,&out)&&isnan(out));
    b->startDiameter=NAN;b->endDiameter=20;
    CHECK(WorldBorder_getStatus(b)==WORLD_BORDER_STATIONARY&&WorldBorder_getDiameter(b,&out)&&isnan(out)&&!r->clocks);
    b->startDiameter=10;b->endDiameter=20;b->startTime=0;b->endTime=100;r->now=20;r->step=20;
    CHECK(WorldBorder_getClosestDistance(b,2,1,&out)&&bits(out)==UINT64_C(0x4018000002000000)&&r->clocks==4);
    b->centerX=2;b->startTime=0;b->endTime=100;r->now=200;r->step=0;r->mutateClock=true;
    CHECK(WorldBorder_minX(b,&out)&&out==-13&&b->centerX==99); /* Capture center before clock; read times afterwards. */
    r->mutateClock=false;b->startDiameter=20;b->endDiameter=40;b->startTime=0;b->endTime=100;r->now=-50;
    CHECK(WorldBorder_getDiameter(b,&out)&&out==10); /* Source does not clamp negative interpolation. */
    b->startDiameter=0;b->endDiameter=8;b->startTime=b->endTime=0;r->now=0;
    /* Actual bytecode compares with dcmpl: unordered 0/0 remains in the
       interpolation branch, despite the decompiler's positive < condition. */
    unsigned before=r->clocks;CHECK(WorldBorder_getDiameter(b,&out)&&isnan(out)&&r->clocks==before+1&&b->startDiameter==0&&b->endDiameter==8);
    b->startDiameter=0;b->endDiameter=8;b->startTime=b->endTime=0;r->now=-1;
    before=r->clocks;CHECK(WorldBorder_getDiameter(b,&out)&&isinf(out)&&out<0&&r->clocks==before+1);
    b->startDiameter=10;b->endDiameter=20;b->startTime=INT64_MAX;b->endTime=INT64_MIN+99;r->now=INT64_MIN+49;
    CHECK(WorldBorder_getDiameter(b,&out)&&out==15);
    CHECK(!MCObjectHeap_failed(h)&&!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);
}
static void source_transitions_snapshot_and_reentry(void) {
    for(unsigned mode=0;mode<4;mode++) {
        MCObjectHeap *h=MCObjectHeap_new(1024*1024);CHECK(h);Recorder *r=recorder(h);WorldBorder *b=r->border;
        Listener *a=listener(r,1),*c=listener(r,3),*d=listener(r,2);r->replacement=(MCObject *)c;
        CHECK(WorldBorder_addListener(b,(MCObject *)a)&&WorldBorder_addListener(b,(MCObject *)d));
        if(mode==3)CHECK(WorldBorder_addListener(b,(MCObject *)a));
        r->mutation=mode==1?1:mode==2?2:0;r->now=42;
        CHECK(WorldBorder_setTransition(b,7));
        CHECK(b->startDiameter==(mode==2?12:7)&&b->endDiameter==b->startDiameter&&b->startTime==42&&b->endTime==42);
        CHECK(r->events==(mode==0||mode==1?2:3)&&r->ids[0]==1);
        if(mode==2)CHECK(r->ids[1]==3&&r->ids[2]==2&&r->values[0]==7&&r->values[1]==12&&r->values[2]==7);
        else CHECK(r->ids[1]==2&&r->values[1]==7);
        if(mode==3)CHECK(r->ids[2]==1);
        if(mode==1)CHECK(NativeReferenceList_size(b->listeners)==1&&NativeReferenceList_get(b->listeners,0)==(MCObject *)c);
        NativeReferenceList *copy=WorldBorder_getListeners(b);CHECK(copy&&copy!=b->listeners);
        CHECK(NativeReferenceList_clear(copy)&&NativeReferenceList_size(b->listeners)>0);
        if(mode==0) {
            r->events=0;CHECK(WorldBorder_setTransitionTimed(b,20,40,100));
            CHECK(b->startDiameter==20&&b->endDiameter==40&&b->startTime==42&&b->endTime==142&&r->events==2);
            CHECK(WorldBorder_getStatus(b)==WORLD_BORDER_GROWING);
            r->events=0;CHECK(WorldBorder_setCenter(b,7,-9)&&r->events==2&&b->centerX==7&&b->centerZ==-9);
        }
        CHECK(!MCObjectHeap_failed(h)&&!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);
    }
}
static void entity_distance_captures_source_coordinates(void) {
    MCObjectHeap *h=MCObjectHeap_new(1024*1024);CHECK(h);Recorder *r=recorder(h);WorldBorder *b=r->border;
    /* Native allocation supplies a real managed subclass for this method's
       field-read fixture; no Player constructor is claimed by this test. */
    MCGameplayPlayer *player=MCGameplayPlayer_nativeAllocate(h);CHECK(player);
    Entity *e=&player->living.entity;e->posX=2;e->posZ=1;r->positionTarget=e;
    b->startDiameter=10;b->endDiameter=20;b->startTime=0;b->endTime=100;r->now=20;r->step=20;
    double out=123;CHECK(WorldBorder_getClosestDistanceEntity(b,e,&out));
    CHECK(bits(out)==UINT64_C(0x4018000002000000)&&e->posX==200&&e->posZ==-100&&r->clocks==4);
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)b));CHECK(MCObjectHeap_collect(h));
    MCObjectHeap *copy=MCObjectHeap_clone(h);CHECK(copy);MCObjectRoot cr={0};CHECK(MCObjectRoot_rebind(&cr,copy,&root));
    WorldBorder *cb=(WorldBorder *)MCObjectRoot_get(&cr);Recorder *record=(Recorder *)cb->dependencyContext;
    CHECK(record->positionTarget!=e&&Entity_isInstance((MCObject *)record->positionTarget)&&record->positionTarget->posX==200);
    CHECK(!MCObjectHeap_failed(h)&&!MCObjectHeap_failed(copy));MCObjectHeap_free(copy);MCObjectHeap_free(h);
    for(unsigned mode=0;mode<2;mode++) {
        h=MCObjectHeap_new(65536);MCObjectHeap *other=MCObjectHeap_new(65536);CHECK(h&&other);b=WorldBorder_new(h,NULL,NULL);CHECK(b);
        player=MCGameplayPlayer_nativeAllocate(other);CHECK(player);out=123;
        CHECK(!WorldBorder_getClosestDistanceEntity(b,mode?&player->living.entity:NULL,&out)&&out==123);
        CHECK(MCObjectHeap_failed(h)&&!MCObjectHeap_failed(other)&&!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);MCObjectHeap_free(other);
    }
}
static void reached_failures_preserve_source_prefix(void) {
    for(unsigned mode=0;mode<7;mode++) {
        MCObjectHeap *h=MCObjectHeap_new(1024*1024);CHECK(h);Recorder *r=recorder(h);WorldBorder *b=r->border;
        b->endTime=5;b->startTime=4;r->now=42;
        if(mode==0)r->failClock=1;
        if(mode==1){b->dependencies=NULL;}
        if(mode>=2){CHECK(WorldBorder_addListener(b,(MCObject *)listener(r,1)));}
        if(mode==2)CHECK(WorldBorder_addListener(b,NULL));
        if(mode==3){static const WorldBorderDependencies missing={clock_ms,NULL,NULL,NULL};b->dependencies=&missing;}
        if(mode==4)r->failListener=1;
        if(mode==5){b->startDiameter=10;b->endDiameter=20;b->startTime=0;b->endTime=100;r->failClock=2;}
        if(mode==6){b->startDiameter=10;b->endDiameter=20;b->startTime=0;b->endTime=100;r->failClock=1;}
        double out=123;
        if(mode>=5)CHECK(!WorldBorder_getClosestDistance(b,0,0,&out)&&out==123);
        else CHECK(!WorldBorder_setTransition(b,8)&&b->startDiameter==8&&b->endDiameter==8);
        if(mode<=1)CHECK(b->endTime==5&&b->startTime==4&&!r->events);
        if(mode>=2&&mode<=4)CHECK(b->endTime==42&&b->startTime==42&&r->events==(mode==3?0:1));
        CHECK(MCObjectHeap_failed(h)&&!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);
    }
}
static void border_lifetime_and_guards(void) {
    MCObjectHeap *h=MCObjectHeap_new(1024*1024);CHECK(h);Recorder *r=recorder(h);
    CHECK(WorldBorder_addListener(r->border,(MCObject *)listener(r,1)));
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)r->border));CHECK(MCObjectHeap_collect(h));
    MCObjectHeap *branch=MCObjectHeap_clone(h);CHECK(branch);MCObjectRoot copy={0};CHECK(MCObjectRoot_rebind(&copy,branch,&root));
    WorldBorder *b=(WorldBorder *)MCObjectRoot_get(&copy);CHECK(b&&b!=r->border&&b->listeners!=r->border->listeners);
    Recorder *cr=(Recorder *)b->dependencyContext;Listener *cl=(Listener *)NativeReferenceList_get(b->listeners,0);
    CHECK(cr&&cr!=r&&cr->border==b&&cl&&cl->recorder==cr&&b->dependencies==&dependencies);
    CHECK(MCObjectHeap_collect(branch)&&WorldBorder_setTransition(b,20));CHECK(cr->events==1&&r->events==0);
    CHECK(MCObjectHeap_adopt(h,branch));MCObjectHeap_free(branch);b=(WorldBorder *)MCObjectRoot_get(&root);
    CHECK(b->dependencyContext==(MCObject *)cr&&WorldBorder_setTransition(b,21)&&cr->events==2);
    CHECK(MCObjectHeap_collect(h)&&!MCObjectHeap_failed(h));MCObjectRoot_drop(&root);CHECK(MCObjectHeap_collect(h)&&!MCObjectHeap_liveObjects(h));MCObjectHeap_free(h);
    h=MCObjectHeap_new(1024*1024);MCObjectHeap *other=MCObjectHeap_new(65536);CHECK(h&&other);r=recorder(h);
    CHECK(!WorldBorder_addListener(r->border,(MCObject *)token(other,1))&&MCObjectHeap_failed(h)&&!MCObjectHeap_failed(other));
    CHECK(!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);MCObjectHeap_free(other);
    h=MCObjectHeap_new(65536);CHECK(h);WorldBorder *valid=WorldBorder_new(h,NULL,NULL);CHECK(valid);
    WorldBorder *small=(WorldBorder *)MCObjectHeap_alloc(h,sizeof(MCObject),valid->object.klass);CHECK(small);
    double out=123;CHECK(!WorldBorder_getDiameter(small,&out)&&out==123&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
    for(unsigned mode=0;mode<3;mode++) {
        h=MCObjectHeap_new(65536);other=MCObjectHeap_new(65536);CHECK(h&&other);r=recorder(h);
        NativeReferenceList *foreign=NativeReferenceList_new(other);CHECK(foreign);
        CHECK(NativeReferenceList_add(foreign,NULL));size_t otherBytes=MCObjectHeap_liveBytes(other);
        r->border->listeners=foreign;r->now=42;
        if(mode==0)CHECK(!WorldBorder_getListeners(r->border));
        if(mode==1)CHECK(!WorldBorder_addListener(r->border,NULL));
        if(mode==2)CHECK(!WorldBorder_setTransition(r->border,8)&&r->border->startDiameter==8&&r->border->endTime==42);
        CHECK(MCObjectHeap_failed(h)&&!MCObjectHeap_failed(other)&&MCObjectHeap_liveBytes(other)==otherBytes&&foreign->size==1);
        CHECK(!MCObjectHeap_hasBorrowers(h)&&!MCObjectHeap_hasBorrowers(other));MCObjectHeap_free(h);MCObjectHeap_free(other);
    }
}
int main(void) {
    managed_list_exists();original_initial_border();ordered_nullable_list_and_graph();list_guards_and_failure_preservation();
    source_geometry_clock_and_ieee();source_transitions_snapshot_and_reentry();entity_distance_captures_source_coordinates();
    reached_failures_preserve_source_prefix();border_lifetime_and_guards();
    printf("source world border: %u checks passed\n",checks);return 0;
}
