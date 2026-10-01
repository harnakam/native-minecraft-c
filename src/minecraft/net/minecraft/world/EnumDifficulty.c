#include "world/EnumDifficulty.h"
static void trace_enum(MCObject *o,MCObjectVisitor v,void *c){EnumDifficulty *e=(EnumDifficulty *)o;e->difficultyResourceKey=(NBTString *)v((MCObject *)e->difficultyResourceKey,c);}
static void trace_statics(MCObject *o,MCObjectVisitor v,void *c) {
    EnumDifficultyStatics *s=(EnumDifficultyStatics *)o;
    s->PEACEFUL=(EnumDifficulty *)v((MCObject *)s->PEACEFUL,c);s->EASY=(EnumDifficulty *)v((MCObject *)s->EASY,c);
    s->NORMAL=(EnumDifficulty *)v((MCObject *)s->NORMAL,c);s->HARD=(EnumDifficulty *)v((MCObject *)s->HARD,c);
    for(size_t i=0;i<4;i++)s->difficultyEnums[i]=(EnumDifficulty *)v((MCObject *)s->difficultyEnums[i],c);
}
static const MCObjectClass enumClass={"net.minecraft.world.EnumDifficulty",MCObjectHeap_plainClone,trace_enum,NULL};
static const MCObjectClass staticsClass={"native.EnumDifficulty.Statics",MCObjectHeap_plainClone,trace_statics,NULL};
bool EnumDifficulty_isInstance(const MCObject *o){return o&&o->klass==&enumClass&&MCObjectHeap_objectSize(o)>=sizeof(EnumDifficulty);}
static bool valid(EnumDifficulty *e){if(EnumDifficulty_isInstance((MCObject *)e)&&!MCObjectHeap_failed(e->object.heap))return true;MCObjectHeap_fail(e?e->object.heap:NULL);return false;}
static bool any(const MCObject *o,void *c){(void)o;(void)c;return true;}
EnumDifficultyStatics *EnumDifficulty_getStatics(MCObjectHeap *h) {
    if(!h||MCObjectHeap_failed(h))return NULL;
    EnumDifficultyStatics *s=(EnumDifficultyStatics *)MCObjectHeap_findObject(h,&staticsClass,any,NULL);
    if(s)return s;
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    s=(EnumDifficultyStatics *)MCObjectHeap_alloc(h,sizeof *s,&staticsClass);MCObjectRoot root={0};
    if(s&&!MCObjectRoot_init(&root,h,(MCObject *)s))s=NULL;
    static const char *const names[4]={"options.difficulty.peaceful","options.difficulty.easy","options.difficulty.normal","options.difficulty.hard"};
    for(size_t i=0;s&&i<4&&!MCObjectHeap_failed(h);i++) {
        EnumDifficulty *e=(EnumDifficulty *)MCObjectHeap_alloc(h,sizeof *e,&enumClass);
        if(!e)break;
        e->difficultyId=(int32_t)i;e->difficultyResourceKey=NBTString_literalASCII(h,names[i]);
        if(!e->difficultyResourceKey)break;
        switch(i){case 0:s->PEACEFUL=e;break;case 1:s->EASY=e;break;case 2:s->NORMAL=e;break;default:s->HARD=e;break;}
        MCObjectHeap_touch(h);
    }
    if(s&&!MCObjectHeap_failed(h)){s->difficultyEnums[0]=s->PEACEFUL;s->difficultyEnums[1]=s->EASY;s->difficultyEnums[2]=s->NORMAL;s->difficultyEnums[3]=s->HARD;MCObjectHeap_touch(h);}
    MCObjectRootScope_end(&scope);return MCObjectHeap_failed(h)?NULL:s;
}
int32_t EnumDifficulty_getDifficultyId(EnumDifficulty *e){return valid(e)?e->difficultyId:0;}
NBTString *EnumDifficulty_getDifficultyResourceKey(EnumDifficulty *e){return valid(e)?e->difficultyResourceKey:NULL;}
EnumDifficulty *EnumDifficulty_getDifficultyEnum(MCObjectHeap *h,int32_t value) {
    EnumDifficultyStatics *s=EnumDifficulty_getStatics(h);int32_t index=value%4;
    if(!s||index<0){MCObjectHeap_fail(h);return NULL;}
    return s->difficultyEnums[index];
}
