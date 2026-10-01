#include "stats/StatFileWriter.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"source stats: %s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
typedef struct { MCObject object; StatFileWriter *writer; StatBase *stat; unsigned calls; bool fail; int32_t value; } Progress;
static void progress_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    Progress *progress=(Progress *)object;
    progress->writer=(StatFileWriter *)visitor((MCObject *)progress->writer,context);
    progress->stat=(StatBase *)visitor((MCObject *)progress->stat,context);
}
static const MCObjectClass progress_class={"TestStatProgress",MCObjectHeap_plainClone,progress_trace,NULL};
static StatBase *stat(MCObjectHeap *heap,const char *id,StatBaseKind kind) {
    StatBase *value=StatBase_newIdentity(heap,NBTString_fromASCII(heap,id),kind); CHECK(value); return value;
}
static Achievement *achievement(MCObjectHeap *heap,const char *id,Achievement *parent) {
    Achievement *value=Achievement_newIdentity(heap,NBTString_fromASCII(heap,id),parent); CHECK(value); return value;
}
static bool override_unlock(StatFileWriter *writer,MCObject *player,StatBase *key,int32_t value) {
    Progress *progress=(Progress *)player; ++progress->calls; progress->value=value;
    CHECK(progress->writer==writer && progress->stat==key);
    CHECK(!MCObjectHeap_collect(player->heap)); CHECK(!MCObjectHeap_clone(player->heap));
    return !progress->fail && StatFileWriter_unlockAchievementBase(writer,player,key,value);
}
static const StatFileWriterOverrides overrides={override_unlock};
static void identity_and_registry(void) {
    MCObjectHeap *heap=MCObjectHeap_new(4*1024*1024); CHECK(heap);
    StatBase *base=stat(heap,"stat.craftItem.minecraft.paper",STAT_BASE_KIND_BASE);
    StatBase *same=stat(heap,"stat.craftItem.minecraft.paper",STAT_BASE_KIND_BASE);
    StatBase *basic=stat(heap,"stat.craftItem.minecraft.paper",STAT_BASE_KIND_BASIC);
    StatBase *crafting=stat(heap,"stat.craftItem.minecraft.paper",STAT_BASE_KIND_CRAFTING);
    Achievement *ach=achievement(heap,"stat.craftItem.minecraft.paper",NULL);
    CHECK(base!=same && base->statId!=same->statId && StatBase_equals(base,same));
    CHECK(!StatBase_equals(base,basic) && !StatBase_equals(base,crafting) && !StatBase_equals(base,&ach->base));
    CHECK(StatBase_equals(base,base) && !StatBase_equals(base,NULL));
    CHECK(StatBase_hashCode(base)==StatBase_hashCode(basic) && StatBase_hashCode(base)==StatBase_hashCode(&ach->base));
    CHECK(!StatBase_isAchievement(base) && StatBase_isAchievement(&ach->base) && Achievement_isAchievement(ach));
    CHECK(!base->isIndependent && StatBase_initIndependentStat(base)==base && base->isIndependent);
    CHECK(Achievement_initIndependentStat(ach)==ach && ach->base.isIndependent);
    CHECK(!Achievement_getSpecial(ach) && Achievement_setSpecial(ach)==ach && Achievement_getSpecial(ach));
    const uint16_t units[]={0,0xd800,0x6771,0xdfff};
    StatBase *unicode=StatBase_newIdentity(heap,NBTString_fromUTF16(heap,units,4),STAT_BASE_KIND_BASE);
    StatBase *unicode2=StatBase_newIdentity(heap,NBTString_fromUTF16(heap,units,4),STAT_BASE_KIND_BASE); CHECK(unicode&&unicode2);
    CHECK(StatBase_equals(unicode,unicode2));
    uint32_t hash=0; for (size_t i=0;i<4;i++) hash=hash*31u+units[i]; int32_t signed_hash; memcpy(&signed_hash,&hash,sizeof(hash));
    CHECK(StatBase_hashCode(unicode)==signed_hash);
    StatBaseRegistry *registry=StatBaseRegistry_new(heap); CHECK(registry);
    CHECK(StatBaseRegistry_register(registry,base)); CHECK(StatBaseRegistry_register(registry,unicode));
    CHECK(StatBaseRegistry_find_ascii(registry,"stat.craftItem.minecraft.paper")==base);
    CHECK(StatBaseRegistry_find(registry,unicode2->statId)==unicode);
    CHECK(StatBaseRegistry_size(registry)==2 && !StatBaseRegistry_find_ascii(registry,"absent"));
    CHECK(!MCObjectHeap_failed(heap)); CHECK(!StatBaseRegistry_register(registry,crafting));
    CHECK(MCObjectHeap_failed(heap) && StatBaseRegistry_size(registry)==2); MCObjectHeap_free(heap);
}
static void counters_and_equal_keys(void) {
    MCObjectHeap *heap=MCObjectHeap_new(4*1024*1024); StatFileWriter *writer=StatFileWriter_new(heap); CHECK(writer);
    StatBase *one=stat(heap,"one",STAT_BASE_KIND_BASE),*equal=stat(heap,"one",STAT_BASE_KIND_BASE);
    StatBase *different=stat(heap,"one",STAT_BASE_KIND_BASIC);
    CHECK(StatFileWriter_readStat(writer,one)==0 && StatFileWriter_statCount(writer)==0);
    CHECK(StatFileWriter_increaseStat(writer,NULL,one,0)); CHECK(StatFileWriter_statCount(writer)==1);
    CHECK(StatFileWriter_increaseStat(writer,NULL,equal,7)); CHECK(StatFileWriter_readStat(writer,one)==7);
    CHECK(StatFileWriter_unlockAchievement(writer,NULL,one,-3)); CHECK(StatFileWriter_readStat(writer,equal)==-3);
    CHECK(StatFileWriter_increaseStat(writer,NULL,different,9)); CHECK(StatFileWriter_statCount(writer)==2);
    CHECK(StatFileWriter_readStat(writer,one)==-3 && StatFileWriter_readStat(writer,different)==9);
    bool found=false;
    for (size_t i=0;i<StatFileWriter_statCount(writer);i++) {
        StatBase *key=NULL; int32_t value=0; MCObject *progress=(MCObject *)one;
        CHECK(StatFileWriter_entryAt(writer,i,&key,&value,&progress)); CHECK(progress==NULL);
        if (key==one) { CHECK(value==-3); found=true; }
        CHECK(key!=equal);
    }
    CHECK(found && !StatFileWriter_entryAt(writer,2,NULL,NULL,NULL));
    CHECK(StatFileWriter_unlockAchievement(writer,NULL,one,INT32_MAX));
    CHECK(StatFileWriter_increaseStat(writer,NULL,equal,1)); CHECK(StatFileWriter_readStat(writer,one)==INT32_MIN);
    CHECK(StatFileWriter_increaseStat(writer,NULL,one,-1)); CHECK(StatFileWriter_readStat(writer,one)==INT32_MAX);
    CHECK(!MCObjectHeap_failed(heap)); MCObjectHeap_free(heap);
}
static void parents(void) {
    MCObjectHeap *heap=MCObjectHeap_new(4*1024*1024); StatFileWriter *writer=StatFileWriter_new(heap); CHECK(writer);
    Achievement *root=achievement(heap,"root",NULL),*parent=achievement(heap,"parent",root),*child=achievement(heap,"child",parent);
    CHECK(StatFileWriter_canUnlockAchievement(writer,root)); CHECK(!StatFileWriter_canUnlockAchievement(writer,parent));
    CHECK(StatFileWriter_func_150874_c(writer,root)==0 && StatFileWriter_func_150874_c(writer,parent)==1 && StatFileWriter_func_150874_c(writer,child)==2);
    CHECK(StatFileWriter_increaseStat(writer,NULL,&child->base,1)); CHECK(StatFileWriter_statCount(writer)==0);
    CHECK(Achievement_initIndependentStat(child)==child);
    CHECK(StatFileWriter_increaseStat(writer,NULL,&child->base,1)); CHECK(StatFileWriter_statCount(writer)==0);
    CHECK(StatFileWriter_unlockAchievement(writer,NULL,&child->base,1));
    CHECK(StatFileWriter_hasAchievementUnlocked(writer,child) && StatFileWriter_func_150874_c(writer,child)==0);
    CHECK(!StatFileWriter_canUnlockAchievement(writer,child));
    CHECK(StatFileWriter_unlockAchievement(writer,NULL,&child->base,-3));
    CHECK(StatFileWriter_increaseStat(writer,NULL,&root->base,1)); CHECK(StatFileWriter_func_150874_c(writer,child)==1);
    CHECK(StatFileWriter_increaseStat(writer,NULL,&parent->base,1)); CHECK(StatFileWriter_func_150874_c(writer,child)==0);
    CHECK(StatFileWriter_increaseStat(writer,NULL,&child->base,5)); CHECK(StatFileWriter_readStat(writer,&child->base)==2);
    /* Only the direct parent gates increaseStat; the entire ancestor chain
       is used by func_150874_c, not by canUnlockAchievement. */
    CHECK(StatFileWriter_unlockAchievement(writer,NULL,&root->base,-1));
    CHECK(!StatFileWriter_canUnlockAchievement(writer,parent) && StatFileWriter_canUnlockAchievement(writer,child));
    CHECK(StatFileWriter_unlockAchievement(writer,NULL,&child->base,INT32_MAX));
    CHECK(StatFileWriter_increaseStat(writer,NULL,&child->base,1)); CHECK(!StatFileWriter_hasAchievementUnlocked(writer,child));
    CHECK(StatFileWriter_readStat(writer,&child->base)==INT32_MIN && !MCObjectHeap_failed(heap)); MCObjectHeap_free(heap);
}
static void progress_and_snapshots(void) {
    MCObjectHeap *heap=MCObjectHeap_new(8*1024*1024); StatFileWriter *writer=StatFileWriter_new(heap); CHECK(writer);
    StatBaseRegistry *registry=StatBaseRegistry_new(heap); CHECK(registry); StatBase *key=stat(heap,"counter",STAT_BASE_KIND_CRAFTING);
    CHECK(StatBaseRegistry_register(registry,key));
    Progress *progress=(Progress *)MCObjectHeap_alloc(heap,sizeof(*progress),&progress_class); CHECK(progress);
    progress->writer=writer; progress->stat=key; progress->value=23;
    CHECK(!StatFileWriter_func_150870_b(writer,key));
    CHECK(StatFileWriter_func_150872_a(writer,key,(MCObject *)progress)==(MCObject *)progress);
    CHECK(StatFileWriter_readStat(writer,key)==0 && StatFileWriter_statCount(writer)==1);
    CHECK(StatFileWriter_func_150870_b(writer,key)==(MCObject *)progress);
    CHECK(StatFileWriter_increaseStat(writer,NULL,key,4)); CHECK(StatFileWriter_func_150870_b(writer,key)==(MCObject *)progress);
    MCObjectRoot rw,rr; CHECK(MCObjectRoot_init(&rw,heap,(MCObject *)writer)); CHECK(MCObjectRoot_init(&rr,heap,(MCObject *)registry));
    CHECK(MCObjectHeap_collect(heap)); CHECK(StatFileWriter_func_150870_b(writer,key)==(MCObject *)progress);
    MCObjectHeap *working=MCObjectHeap_clone(heap); CHECK(working); MCObjectRoot cw,cr;
    CHECK(MCObjectRoot_rebind(&cw,working,&rw)); CHECK(MCObjectRoot_rebind(&cr,working,&rr));
    StatFileWriter *copy=(StatFileWriter *)MCObjectRoot_get(&cw); StatBaseRegistry *copiedRegistry=(StatBaseRegistry *)MCObjectRoot_get(&cr);
    StatBase *copiedKey=StatBaseRegistry_find_ascii(copiedRegistry,"counter"); CHECK(copiedKey&&copiedKey!=key);
    Progress *copiedProgress=(Progress *)StatFileWriter_func_150870_b(copy,copiedKey);
    CHECK(copiedProgress&&copiedProgress!=progress&&copiedProgress->writer==copy&&copiedProgress->stat==copiedKey);
    CHECK(StatFileWriter_increaseStat(copy,NULL,copiedKey,7)); CHECK(StatFileWriter_readStat(writer,key)==4);
    CHECK(MCObjectHeap_adopt(heap,working)); MCObjectHeap_free(working);
    writer=(StatFileWriter *)MCObjectRoot_get(&rw); registry=(StatBaseRegistry *)MCObjectRoot_get(&rr);
    key=StatBaseRegistry_find_ascii(registry,"counter"); CHECK(StatFileWriter_readStat(writer,key)==11);
    CHECK(StatFileWriter_func_150872_a(writer,key,NULL)==NULL); CHECK(!StatFileWriter_func_150870_b(writer,key));
    CHECK(StatFileWriter_readStat(writer,key)==11 && !MCObjectHeap_failed(heap));
    MCObjectRoot_drop(&rw); MCObjectRoot_drop(&rr); CHECK(MCObjectHeap_collect(heap)); CHECK(MCObjectHeap_liveObjects(heap)==0);
    MCObjectHeap_free(heap);
}
static void collisions_and_growth(void) {
    MCObjectHeap *heap=MCObjectHeap_new(16*1024*1024); StatFileWriter *writer=StatFileWriter_new(heap); CHECK(writer);
    StatBase *keys[512]; char id[19];
    for (unsigned n=0;n<512;n++) {
        for (unsigned i=0;i<9;i++) { id[i*2]=(n&(1u<<i)) ? 'A' : 'B'; id[i*2+1]=(n&(1u<<i)) ? 'a' : 'B'; } id[18]=0;
        keys[n]=stat(heap,id,STAT_BASE_KIND_BASE);
        CHECK(StatBase_hashCode(keys[n])==StatBase_hashCode(keys[0]));
        CHECK(StatFileWriter_increaseStat(writer,NULL,keys[n],(int32_t)n));
    }
    CHECK(StatFileWriter_statCount(writer)==512);
    for (unsigned n=0;n<512;n++) CHECK(StatFileWriter_readStat(writer,keys[n])==(int32_t)n);
    MCObjectRoot root; CHECK(MCObjectRoot_init(&root,heap,(MCObject *)writer)); CHECK(MCObjectHeap_collect(heap));
    for (unsigned n=0;n<512;n++) CHECK(StatFileWriter_readStat(writer,keys[n])==(int32_t)n);
    CHECK(!MCObjectHeap_failed(heap)); MCObjectHeap_free(heap);
}
static void virtual_unlock(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024); StatFileWriter *writer=StatFileWriter_newWithOverrides(heap,&overrides); CHECK(writer);
    StatBase *key=stat(heap,"virtual",STAT_BASE_KIND_BASE);
    Progress *player=(Progress *)MCObjectHeap_alloc(heap,sizeof(*player),&progress_class); CHECK(player);
    player->writer=writer; player->stat=key;
    CHECK(StatFileWriter_increaseStat(writer,(MCObject *)player,key,3));
    CHECK(player->calls==1 && player->value==3 && StatFileWriter_readStat(writer,key)==3);
    player->fail=true; CHECK(!StatFileWriter_increaseStat(writer,(MCObject *)player,key,2));
    CHECK(player->calls==2 && player->value==5 && MCObjectHeap_failed(heap)); MCObjectHeap_free(heap);
}
static void exception_boundaries(void) {
    for (unsigned mode=0;mode<4;mode++) {
        MCObjectHeap *heap=MCObjectHeap_new(1024*1024); StatFileWriter *writer=StatFileWriter_new(heap); CHECK(writer);
        if (mode==0) CHECK(StatFileWriter_readStat(writer,NULL)==0);
        if (mode==1) CHECK(!StatFileWriter_increaseStat(writer,NULL,NULL,1));
        if (mode==2) CHECK(!StatFileWriter_unlockAchievement(writer,NULL,NULL,1));
        if (mode==3) CHECK(!StatFileWriter_canUnlockAchievement(writer,NULL));
        CHECK(MCObjectHeap_failed(heap) && StatFileWriter_statCount(writer)==0); MCObjectHeap_free(heap);
    }
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024),*foreign=MCObjectHeap_new(1024*1024);
    StatFileWriter *writer=StatFileWriter_new(heap); CHECK(writer); StatBase *key=stat(heap,"local",STAT_BASE_KIND_BASE);
    Progress *progress=(Progress *)MCObjectHeap_alloc(foreign,sizeof(*progress),&progress_class); CHECK(progress);
    CHECK(!StatFileWriter_func_150872_a(writer,key,(MCObject *)progress)); CHECK(MCObjectHeap_failed(heap));
    CHECK(StatFileWriter_statCount(writer)==0 && !MCObjectHeap_failed(foreign)); MCObjectHeap_free(heap); MCObjectHeap_free(foreign);
    heap=MCObjectHeap_new(1024*1024); writer=StatFileWriter_new(heap); CHECK(writer);
    Achievement *a=achievement(heap,"cycle",NULL),*b=achievement(heap,"cycle2",a); a->parentAchievement=b;
    CHECK(!StatFileWriter_canUnlockAchievement(writer,a) && !MCObjectHeap_failed(heap));
    CHECK(StatFileWriter_func_150874_c(writer,a)==0 && MCObjectHeap_failed(heap)); MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(1024*1024); key=StatBase_newIdentity(heap,NULL,STAT_BASE_KIND_BASE); CHECK(key);
    CHECK(StatBase_equals(key,key)); CHECK(StatBase_hashCode(key)==0 && MCObjectHeap_failed(heap)); MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(1); CHECK(!StatFileWriter_new(heap)); CHECK(MCObjectHeap_failed(heap)); MCObjectHeap_free(heap);
}
int main(void) {
    identity_and_registry(); counters_and_equal_keys(); parents(); progress_and_snapshots(); collisions_and_growth(); virtual_unlock(); exception_boundaries();
    printf("Source stats: %u checks passed\n",checks); return 0;
}
