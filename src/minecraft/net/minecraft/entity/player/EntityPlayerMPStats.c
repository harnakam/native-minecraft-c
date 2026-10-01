#include "entity/player/EntityPlayerMPStats.h"
static bool ready(const EntityPlayerMPStatsDependencies *d){return d&&d->getWorldScoreboard&&d->getCriteria&&d->getObjectivesFromCriteria&&d->iterator&&d->hasNext&&d->next&&d->getName&&d->getValueFromObjective&&d->increseScore&&d->setScorePoints;}
static bool ref(MCObjectHeap *h,MCObject *o,bool nullable){if((!o&&!nullable)||(o&&o->heap!=h)||MCObjectHeap_failed(h)){MCObjectHeap_fail(h);return false;}return true;}
static bool execute(MCGameplayPlayer *p,StatBase *stat,int32_t amount,bool reset,MCObject *context,const EntityPlayerMPStatsDependencies *d) {
    if(!stat)return true;
    MCObjectHeap *h=p?p->object.heap:stat->object.heap;
    if(!MCGameplayPlayer_isInstance((MCObject *)p)||stat->object.heap!=h||!ready(d)||
        !ref(h,context,true)||!StatisticsFile_isInstance((MCObject *)p->stats)||((MCObject *)p->stats)->heap!=h) {MCObjectHeap_fail(h);return false;}
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)p)&&MCObjectRootScope_pin(&scope,(MCObject *)stat)&&MCObjectRootScope_pin(&scope,context);
    if(ok)ok=reset?StatFileWriter_unlockAchievement(p->stats,(MCObject *)p,stat,0):StatFileWriter_increaseStat(p->stats,(MCObject *)p,stat,amount);
    MCObject *board=ok?d->getWorldScoreboard(context,p):NULL;if(ok)ok=ref(h,board,false);
    MCObject *criteria=ok?d->getCriteria(context,stat):NULL;if(ok)ok=ref(h,criteria,true);
    MCObject *collection=ok?d->getObjectivesFromCriteria(context,board,criteria):NULL;if(ok)ok=ref(h,collection,false);
    MCObject *iterator=ok?d->iterator(context,collection):NULL;if(ok)ok=ref(h,iterator,false);
    while(ok) {
        bool next=d->hasNext(context,iterator);ok=!MCObjectHeap_failed(h);if(!ok||!next)break;
        MCObject *objective=d->next(context,iterator);ok=ref(h,objective,true);
        board=ok?d->getWorldScoreboard(context,p):NULL;if(ok)ok=ref(h,board,false);
        NBTString *name=ok?d->getName(context,p):NULL;if(ok)ok=ref(h,(MCObject *)name,true);
        MCObject *score=ok?d->getValueFromObjective(context,board,name,objective):NULL;if(ok)ok=ref(h,score,false);
        if(ok)ok=reset?d->setScorePoints(context,score,0):d->increseScore(context,score,amount);
        ok=ok&&!MCObjectHeap_failed(h);
    }
    if(ok&&StatisticsFile_func_150879_e((StatisticsFile *)p->stats))ok=StatisticsFile_func_150876_a((StatisticsFile *)p->stats,(MCObject *)p);
    if(!ok)MCObjectHeap_fail(h);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(h);
}
bool EntityPlayerMP_addStat(MCGameplayPlayer *p,StatBase *s,int32_t amount,MCObject *c,const EntityPlayerMPStatsDependencies *d){return execute(p,s,amount,false,c,d);}
bool EntityPlayerMP_func_175145_a(MCGameplayPlayer *p,StatBase *s,MCObject *c,const EntityPlayerMPStatsDependencies *d){return execute(p,s,0,true,c,d);}
