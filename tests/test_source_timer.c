#include "util/Timer.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct {
    MCObject object;
    Timer *target;
    int64_t sys,nano;
    unsigned reads,fail;
    char order[64];
    int mutation;
    bool scopeObserved;
} Clock;
static void trace(MCObject *o,MCObjectVisitor v,void *c) {
    Clock *clock=(Clock *)o; clock->target=(Timer *)v((MCObject *)clock->target,c);
}
static const MCObjectClass clock_class={"test.Timer.Clock",MCObjectHeap_plainClone,trace,NULL};
static bool read_clock(Clock *c,char kind,int64_t *value) {
    CHECK(c->reads+1<sizeof c->order);
    c->order[c->reads++]=kind; c->order[c->reads]=0;
    c->scopeObserved=MCObjectHeap_hasBorrowers(c->object.heap);
    if (kind=='S' && c->mutation==1) c->target->lastSyncSysClock=880;
    if (kind=='N' && c->mutation==1) c->target->lastSyncSysClock=999;
    if (kind=='N' && c->mutation==2) c->target->counter=333;
    if (c->mutation==3) {
        CHECK(!MCObjectHeap_collect(c->object.heap));
        CHECK(!MCObjectHeap_clone(c->object.heap));
    }
    *value=kind=='S' ? c->sys : c->nano;
    return c->reads!=c->fail;
}
static bool system_time(MCObject *c,int64_t *v) { return read_clock((Clock *)c,'S',v); }
static bool nano_time(MCObject *c,int64_t *v) { return read_clock((Clock *)c,'N',v); }
static const TimerDependencies clocks={system_time,nano_time};
static Clock *clock_new(MCObjectHeap *h,int64_t sys,int64_t nano) {
    Clock *c=(Clock *)MCObjectHeap_alloc(h,sizeof *c,&clock_class); CHECK(c);
    c->sys=sys; c->nano=nano; return c;
}
static Timer *create(MCObjectHeap *h,Clock *c,float tps) {
    Timer *t=Timer_new(h,tps,&clocks,(MCObject *)c); CHECK(t);
    CHECK(Timer_isInstance((MCObject *)t)); CHECK(c->scopeObserved);
    CHECK(!MCObjectHeap_hasBorrowers(h)); CHECK(!strcmp(c->order,"SN"));
    c->target=t; return t;
}
static uint32_t bits(float x) { uint32_t b; memcpy(&b,&x,sizeof b); return b; }
static void first_update_and_discard(void) {
    /* Priming lastHRTime, retaining excess ticks or capping before subtracting
       whole ticks all break the independently derived first/second updates. */
    MCObjectHeap *h=MCObjectHeap_new(65536); CHECK(h);
    Clock *c=clock_new(h,1000,2500000000LL); Timer *t=create(h,c,20);
    CHECK(t->lastHRTime==0 && t->lastSyncSysClock==1000 && t->lastSyncHRClock==2500);
    CHECK(t->timerSpeed==1 && t->timeSyncAdjustment==1 && t->counter==0);
    CHECK(Timer_updateTimer(t)); CHECK(t->elapsedTicks==10);
    CHECK(t->elapsedPartialTicks==0 && t->renderPartialTicks==0 && t->lastHRTime==2.5);
    CHECK(Timer_updateTimer(t)); CHECK(t->elapsedTicks==0 && t->elapsedPartialTicks==0);
    c->sys=1025; c->nano=2525000000LL; CHECK(Timer_updateTimer(t));
    CHECK(t->elapsedTicks==0 && t->elapsedPartialTicks==0.5f && t->renderPartialTicks==0.5f);
    c->sys=1050; c->nano=2550000000LL; CHECK(Timer_updateTimer(t));
    CHECK(t->elapsedTicks==1 && t->elapsedPartialTicks==0 && t->counter==50);
    CHECK(!strcmp(c->order,"SNSNSNSNSN")); MCObjectHeap_free(h);
}
static void sync_and_signed_clocks(void) {
    MCObjectHeap *h=MCObjectHeap_new(65536); Clock *c=clock_new(h,0,0);
    Timer *t=create(h,c,20); t->counter=1000;
    CHECK(Timer_updateTimer(t)); CHECK(t->counter==1000 && t->timeSyncAdjustment==1);
    c->sys=1; c->nano=1000000; CHECK(Timer_updateTimer(t));
    CHECK(t->counter==0 && t->lastSyncHRClock==1 && t->timeSyncAdjustment>200 && t->timeSyncAdjustment<202);
    t->counter=INT64_MAX; t->lastSyncSysClock=INT64_MAX; t->lastSyncHRClock=8;
    c->sys=INT64_MIN; c->nano=-1000001;
    CHECK(Timer_updateTimer(t)); CHECK(t->counter==INT64_MIN && t->lastSyncHRClock==-1);
    CHECK(t->lastSyncSysClock==INT64_MIN && t->lastHRTime==-0.001);
    t->counter=41; t->lastSyncSysClock=10; c->sys=9; c->nano=-999999;
    CHECK(Timer_updateTimer(t)); CHECK(t->counter==41 && t->lastHRTime==0 && t->elapsedTicks==0);
    t->lastSyncSysClock=0; t->counter=77; c->sys=1001; c->nano=9000000000LL;
    CHECK(Timer_updateTimer(t)); CHECK(t->counter==77 && t->lastHRTime==9 && t->elapsedTicks==0);
    MCObjectHeap_free(h);
}
static void signed_and_nonfinite(void) {
    struct Case { float speed,partial; int32_t ticks; float fraction; bool nan; } cases[]={
        {0,0.5f,0,0.5f,false},{-1,0,-20,0,false},{-0.125f,0,-2,-0.5f,false},
        {INFINITY,0,10,INFINITY,false},{-INFINITY,0,INT32_MIN,-INFINITY,false},
        {NAN,0,0,NAN,true},{1,16777216,10,0,false},
        {1,2147483648.0f,10,0,false}
    };
    for (size_t i=0;i<sizeof cases/sizeof *cases;i++) {
        MCObjectHeap *h=MCObjectHeap_new(65536); Clock *c=clock_new(h,0,1000000000);
        Timer *t=create(h,c,20); t->timerSpeed=cases[i].speed; t->elapsedPartialTicks=cases[i].partial;
        CHECK(Timer_updateTimer(t)); CHECK(t->elapsedTicks==cases[i].ticks);
        if (cases[i].nan) CHECK(isnan(t->elapsedPartialTicks) && isnan(t->renderPartialTicks));
        else CHECK(bits(t->elapsedPartialTicks)==bits(cases[i].fraction) && bits(t->renderPartialTicks)==bits(cases[i].fraction));
        MCObjectHeap_free(h);
    }
    MCObjectHeap *h=MCObjectHeap_new(65536); Clock *c=clock_new(h,0,0); Timer *t=create(h,c,20);
    t->timeSyncAdjustment=NAN; CHECK(Timer_updateTimer(t));
    CHECK(t->elapsedTicks==0 && isnan(t->elapsedPartialTicks)); MCObjectHeap_free(h);
}
static void failures_and_callback_order(void) {
    for (unsigned fail=1;fail<=2;fail++) {
        MCObjectHeap *h=MCObjectHeap_new(65536); Clock *c=clock_new(h,11,22000000); c->fail=fail;
        CHECK(!Timer_new(h,20,&clocks,(MCObject *)c)); CHECK(MCObjectHeap_failed(h));
        CHECK(c->reads==fail && c->scopeObserved && !MCObjectHeap_hasBorrowers(h));
        MCObjectHeap_free(h);
    }
    for (unsigned fail=3;fail<=4;fail++) {
        MCObjectHeap *h=MCObjectHeap_new(65536); Clock *c=clock_new(h,1000,2000000000);
        Timer *t=create(h,c,20); t->counter=7; c->fail=fail;
        CHECK(!Timer_updateTimer(t)); CHECK(MCObjectHeap_failed(h));
        CHECK(t->lastSyncSysClock==1000 && t->lastSyncHRClock==2000 && t->counter==7 && t->lastHRTime==0);
        CHECK(c->reads==fail && !MCObjectHeap_hasBorrowers(h)); MCObjectHeap_free(h);
    }
    MCObjectHeap *h=MCObjectHeap_new(65536); Clock *c=clock_new(h,1000,2000000000);
    Timer *t=create(h,c,20); c->mutation=1;
    CHECK(Timer_updateTimer(t)); CHECK(t->counter==120 && t->lastSyncSysClock==1000);
    c->mutation=2; c->fail=c->reads+2;
    CHECK(!Timer_updateTimer(t)); CHECK(t->counter==333); MCObjectHeap_free(h);
}
static void context_lifetime(void) {
    MCObjectHeap *h=MCObjectHeap_new(65536); Clock *c=clock_new(h,10,20000000);
    Timer *a=create(h,c,20); c->reads=0; c->order[0]=0; Timer *b=create(h,c,10);
    MCObjectRoot ar={0},br={0},cr={0};
    CHECK(MCObjectRoot_init(&ar,h,(MCObject *)a)); CHECK(MCObjectRoot_init(&br,h,(MCObject *)b));
    CHECK(MCObjectRoot_init(&cr,h,(MCObject *)c)); CHECK(MCObjectHeap_collect(h));
    CHECK(MCObjectHeap_liveObjects(h)==3);
    MCObjectHeap *copy=MCObjectHeap_clone(h); CHECK(copy);
    MCObjectRoot ac={0},bc={0},cc={0};
    CHECK(MCObjectRoot_rebind(&ac,copy,&ar)); CHECK(MCObjectRoot_rebind(&bc,copy,&br));
    CHECK(MCObjectRoot_rebind(&cc,copy,&cr));
    Timer *ca=(Timer *)MCObjectRoot_get(&ac),*cb=(Timer *)MCObjectRoot_get(&bc);
    Clock *clock=(Clock *)MCObjectRoot_get(&cc);
    CHECK(ca!=a && cb!=b && clock!=c && ca->context==(MCObject *)clock && cb->context==(MCObject *)clock);
    CHECK(clock->target==cb && ca->dependencies==&clocks && cb->dependencies==&clocks);
    clock->sys=35; clock->nano=45000000; CHECK(Timer_updateTimer(ca));
    CHECK(ca->counter==25 && a->counter==0 && ca->lastSyncSysClock==35 && a->lastSyncSysClock==10);
    CHECK(MCObjectHeap_collect(copy)); CHECK(MCObjectHeap_liveObjects(copy)==3);
    MCObjectHeap_free(copy); MCObjectHeap_free(h);
    h=MCObjectHeap_new(65536); MCObjectHeap *foreign=MCObjectHeap_new(65536);
    c=clock_new(foreign,0,0); CHECK(!Timer_new(h,20,&clocks,(MCObject *)c));
    CHECK(MCObjectHeap_failed(h) && !c->reads); MCObjectHeap_free(h); MCObjectHeap_free(foreign);
}
static void native_binding_guards(void) {
    MCObjectHeap *h=MCObjectHeap_new(65536); Clock *c=clock_new(h,0,0);
    TimerDependencies incomplete={system_time,NULL};
    CHECK(!Timer_new(h,20,&incomplete,(MCObject *)c));
    CHECK(MCObjectHeap_failed(h) && c->reads==0 && !MCObjectHeap_hasBorrowers(h));
    MCObjectHeap_free(h);
    h=MCObjectHeap_new(65536); c=clock_new(h,0,0);
    CHECK(!Timer_updateTimer((Timer *)c)); CHECK(MCObjectHeap_failed(h) && !c->reads);
    MCObjectHeap_free(h);
    h=MCObjectHeap_new(65536); c=clock_new(h,0,0); c->mutation=3;
    CHECK(Timer_new(h,20,&clocks,(MCObject *)c));
    CHECK(!MCObjectHeap_failed(h) && c->reads==2 && !MCObjectHeap_hasBorrowers(h));
    MCObjectHeap_free(h);
    h=MCObjectHeap_new(65536); c=clock_new(h,0,0); Timer *t=create(h,c,20);
    c->mutation=3; CHECK(Timer_updateTimer(t));
    CHECK(!MCObjectHeap_failed(h) && c->reads==4 && t->lastHRTime==0 && t->counter==0);
    CHECK(!MCObjectHeap_hasBorrowers(h)); MCObjectHeap_free(h);
}
int main(void) {
    first_update_and_discard(); sync_and_signed_clocks(); signed_and_nonfinite();
    failures_and_callback_order(); context_lifetime(); native_binding_guards();
    printf("Source Timer: %u checks passed\n",checks); return 0;
}
