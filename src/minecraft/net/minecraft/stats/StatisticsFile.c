#include "stats/StatisticsFile.h"
#include <string.h>
typedef struct Entry {MCObject object;StatBase *key;int32_t value;struct Entry *next;} Entry;
struct StatisticsFileStatSet {MCObject object;Entry *head,*tail;size_t size;};
struct StatisticsFileIntMap {MCObject object;Entry *head,*tail;size_t size;};
static void entry_trace(MCObject *o,MCObjectVisitor v,void *c){Entry *e=(Entry *)o;e->key=(StatBase *)v((MCObject *)e->key,c);e->next=(Entry *)v((MCObject *)e->next,c);}
static void set_trace(MCObject *o,MCObjectVisitor v,void *c){StatisticsFileStatSet *s=(StatisticsFileStatSet *)o;s->head=(Entry *)v((MCObject *)s->head,c);s->tail=(Entry *)v((MCObject *)s->tail,c);}
static void map_trace(MCObject *o,MCObjectVisitor v,void *c){StatisticsFileIntMap *s=(StatisticsFileIntMap *)o;s->head=(Entry *)v((MCObject *)s->head,c);s->tail=(Entry *)v((MCObject *)s->tail,c);}
static void trace(MCObject *o,MCObjectVisitor v,void *c){StatisticsFile *s=(StatisticsFile *)o;StatFileWriter_trace(o,v,c);s->mcServer=v(s->mcServer,c);s->dependencyContext=v(s->dependencyContext,c);s->statsFilePath=(NBTString *)v((MCObject *)s->statsFilePath,c);s->field_150888_e=(StatisticsFileStatSet *)v((MCObject *)s->field_150888_e,c);}
static const MCObjectClass entry_class={"C919.Statistics.Entry",MCObjectHeap_plainClone,entry_trace,NULL};
static const MCObjectClass set_class={"C919.Statistics.HashSet",MCObjectHeap_plainClone,set_trace,NULL};
static const MCObjectClass map_class={"C919.Statistics.HashMap",MCObjectHeap_plainClone,map_trace,NULL};
static const MCObjectClass klass={"StatisticsFile",MCObjectHeap_plainClone,trace,NULL};
bool StatisticsFile_isInstance(const MCObject *o){return o&&o->klass==&klass;}
static bool failed(MCObjectHeap *h){MCObjectHeap_fail(h);return false;}
static bool append(MCObjectHeap *h,Entry **head,Entry **tail,size_t *count,StatBase *key,int32_t value) {
    if(!key||key->object.heap!=h||StatBase_getKind(key)<STAT_BASE_KIND_BASE)return failed(h);
    for(Entry *e=*head;e;e=e->next)if(StatBase_equals(e->key,key)){e->value=value;MCObjectHeap_touch(h);return !MCObjectHeap_failed(h);}
    if(*count==SIZE_MAX)return failed(h);
    Entry *e=(Entry *)MCObjectHeap_alloc(h,sizeof(*e),&entry_class);if(!e)return false;
    e->key=key;e->value=value;if(*tail)(*tail)->next=e;else *head=e;*tail=e;++*count;MCObjectHeap_touch(h);return true;
}
static bool set_add(StatisticsFileStatSet *s,StatBase *key){return append(s->object.heap,&s->head,&s->tail,&s->size,key,0);}
static bool map_put(StatisticsFileIntMap *s,StatBase *key,int32_t value){return append(s->object.heap,&s->head,&s->tail,&s->size,key,value);}
static bool override_unlock(StatFileWriter *s,MCObject *p,StatBase *stat,int32_t value){return StatisticsFile_unlockAchievement((StatisticsFile *)s,p,stat,value);}
static const StatFileWriterOverrides overrides={override_unlock};
static bool dependencies(const StatisticsFileDependencies *d){return d&&d->isAnnouncingPlayerAchievements&&d->sendAchievementChat&&d->getTickCounter&&d->sendStatistics;}
StatisticsFile *StatisticsFile_nativeNew(MCObjectHeap *h,MCObject *server,NBTString *path,MCObject *context,const StatisticsFileDependencies *d) {
    if(!dependencies(d)||(server&&server->heap!=h)||(path&&((MCObject *)path)->heap!=h)||(context&&context->heap!=h)){failed(h);return NULL;}
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    StatisticsFile *s=(StatisticsFile *)MCObjectHeap_alloc(h,sizeof(*s),&klass);
    if(s){s->mcServer=server;s->statsFilePath=path;s->dependencyContext=context;s->dependencies=d;s->field_150885_f=-300;
        if(!StatFileWriter_construct(&s->base,&overrides))s=NULL;
        else {s->field_150888_e=(StatisticsFileStatSet *)MCObjectHeap_alloc(h,sizeof(StatisticsFileStatSet),&set_class);if(!s->field_150888_e)s=NULL;}}
    MCObjectRootScope_end(&scope);return MCObjectHeap_failed(h)?NULL:s;
}
static bool begin(StatisticsFile *s,MCObject *player,MCObjectRootScope *scope) {
    MCObjectHeap *h=s?s->base.object.heap:NULL;
    if(!StatisticsFile_isInstance((MCObject *)s)||!dependencies(s->dependencies)||(player&&player->heap!=h))return failed(h);
    return MCObjectRootScope_begin(scope,h)&&MCObjectRootScope_pin(scope,(MCObject *)s)&&MCObjectRootScope_pin(scope,player);
}
static bool end(StatisticsFile *s,MCObjectRootScope *scope,bool ok){if(!ok)failed(s?s->base.object.heap:NULL);MCObjectRootScope_end(scope);return ok&&!MCObjectHeap_failed(s->base.object.heap);}
bool StatisticsFile_unlockAchievement(StatisticsFile *s,MCObject *player,StatBase *stat,int32_t value) {
    MCObjectRootScope scope={0};if(!begin(s,player,&scope)){MCObjectRootScope_end(&scope);return false;}
    MCObjectHeap *h=s->base.object.heap;bool achievement=StatBase_isAchievement(stat);
    int32_t before=achievement?StatFileWriter_readStat(&s->base,stat):0;
    bool ok=!MCObjectHeap_failed(h)&&StatFileWriter_unlockAchievementBase(&s->base,player,stat,value)&&set_add(s->field_150888_e,stat);
    if(ok&&achievement&&((before==0&&value>0)||(before>0&&value==0))) {
        s->field_150886_g=true;MCObjectHeap_touch(h);
        bool announcing=s->dependencies->isAnnouncingPlayerAchievements(s->dependencyContext,s->mcServer);
        ok=!MCObjectHeap_failed(h);
        if(ok&&announcing)ok=s->dependencies->sendAchievementChat(s->dependencyContext,s->mcServer,player,stat,before>0);
    }
    return end(s,&scope,ok);
}
StatisticsFileStatSet *StatisticsFile_func_150878_c(StatisticsFile *s) {
    MCObjectRootScope scope={0};if(!begin(s,NULL,&scope)){MCObjectRootScope_end(&scope);return NULL;}
    MCObjectHeap *h=s->base.object.heap;StatisticsFileStatSet *copy=(StatisticsFileStatSet *)MCObjectHeap_alloc(h,sizeof(*copy),&set_class);
    bool ok=copy!=NULL;
    for(Entry *e=s->field_150888_e->head;ok&&e;e=e->next)ok=set_add(copy,e->key);
    if(ok){s->field_150888_e->head=s->field_150888_e->tail=NULL;s->field_150888_e->size=0;s->field_150886_g=false;MCObjectHeap_touch(h);}
    ok=end(s,&scope,ok);return ok?copy:NULL;
}
bool StatisticsFile_func_150877_d(StatisticsFile *s) {
    MCObjectRootScope scope={0};if(!begin(s,NULL,&scope)){MCObjectRootScope_end(&scope);return false;}
    bool ok=true;size_t count=StatFileWriter_statCount(&s->base);
    for(size_t i=0;ok&&i<count;i++){StatBase *stat=NULL;ok=StatFileWriter_entryAt(&s->base,i,&stat,NULL,NULL)&&set_add(s->field_150888_e,stat);}
    return end(s,&scope,ok);
}
bool StatisticsFile_func_150879_e(const StatisticsFile *s){return s&&s->field_150886_g;}
bool StatisticsFile_func_150876_a(StatisticsFile *s,MCObject *player) {
    MCObjectRootScope scope={0};if(!begin(s,player,&scope)){MCObjectRootScope_end(&scope);return false;}
    MCObjectHeap *h=s->base.object.heap;int32_t tick=s->dependencies->getTickCounter(s->dependencyContext,s->mcServer);
    StatisticsFileIntMap *map=!MCObjectHeap_failed(h)?(StatisticsFileIntMap *)MCObjectHeap_alloc(h,sizeof(*map),&map_class):NULL;
    bool ok=map!=NULL;uint32_t difference=(uint32_t)tick-(uint32_t)s->field_150885_f;int32_t signedDifference;memcpy(&signedDifference,&difference,sizeof(signedDifference));
    if(ok&&(s->field_150886_g||signedDifference>300)) {
        s->field_150885_f=tick;MCObjectHeap_touch(h);
        StatisticsFileStatSet *set=StatisticsFile_func_150878_c(s);ok=set!=NULL;
        for(Entry *e=set?set->head:NULL;ok&&e;e=e->next)ok=map_put(map,e->key,StatFileWriter_readStat(&s->base,e->key))&&!MCObjectHeap_failed(h);
    }
    if(ok)ok=s->dependencies->sendStatistics(s->dependencyContext,player,map);
    return end(s,&scope,ok);
}
size_t StatisticsFileStatSet_size(const StatisticsFileStatSet *s){return s?s->size:0;}
StatBase *StatisticsFileStatSet_get(StatisticsFileStatSet *s,size_t index){if(!s||index>=s->size){failed(s?s->object.heap:NULL);return NULL;}Entry *e=s->head;while(index--)e=e->next;return e->key;}
size_t StatisticsFileIntMap_size(const StatisticsFileIntMap *s){return s?s->size:0;}
bool StatisticsFileIntMap_entry(StatisticsFileIntMap *s,size_t index,StatBase **key,int32_t *value){if(!s||index>=s->size)return false;Entry *e=s->head;while(index--)e=e->next;if(key)*key=e->key;if(value)*value=e->value;return true;}
