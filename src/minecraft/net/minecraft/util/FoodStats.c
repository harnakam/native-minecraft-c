#include "util/FoodStats.h"
#include "nbt/NBTTagCompound.h"
#include "nbt/NBTInternal.h"
#include <limits.h>
#include <math.h>
static const MCObjectClass food_class={"FoodStats",MCObjectHeap_plainClone,NULL,NULL};
bool FoodStats_isInstance(const MCObject *o){return o&&o->klass==&food_class&&MCObjectHeap_objectSize(o)>=sizeof(FoodStats);}
static bool valid(const FoodStats *s){if(!FoodStats_isInstance((const MCObject *)s)){MCObjectHeap_fail(s?s->object.heap:NULL);return false;}return !MCObjectHeap_failed(s->object.heap);}
static bool effect(FoodStats *s,bool ok){if(!ok)MCObjectHeap_fail(s->object.heap);return ok&&!MCObjectHeap_failed(s->object.heap);}
static bool begin(FoodStats *s,MCObjectRootScope *scope){if(!valid(s)||!MCObjectRootScope_begin(scope,s->object.heap))return false;if(MCObjectRootScope_pin(scope,(MCObject *)s))return true;MCObjectRootScope_end(scope);return false;}
static int32_t signed32(uint32_t n){return n<=INT32_MAX?(int32_t)n:-1-(int32_t)(UINT32_MAX-n);}
static float minf(float a,float b){if(isnan(a))return a;if(a==0&&b==0)return signbit(a)?a:b;return a<=b?a:b;}
static float maxf(float a,float b){if(isnan(a))return a;if(a==0&&b==0)return signbit(a)?b:a;return a>=b?a:b;}
FoodStats *FoodStats_new(MCObjectHeap *heap) {
    FoodStats *s=(FoodStats *)MCObjectHeap_alloc(heap,sizeof *s,&food_class);
    if(s){s->foodLevel=20;s->foodSaturationLevel=5.0f;s->prevFoodLevel=20;}return s;
}
bool FoodStats_addStats(FoodStats *s,int32_t amount,float modifier){
    if(!valid(s))return false;
    int32_t sum=signed32((uint32_t)amount+(uint32_t)s->foodLevel);s->foodLevel=sum<20?sum:20;
    volatile float product=(float)amount*modifier;product=product*2.0f;
    volatile float saturation=s->foodSaturationLevel+product;
    s->foodSaturationLevel=minf(saturation,(float)s->foodLevel);MCObjectHeap_touch(s->object.heap);return true;
}
bool FoodStats_addStats_item(FoodStats *s,const Item *item,ItemStack *stack,const FoodStatsItemDependencies *d,MCObject *ctx){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return false;bool ok=false;int32_t amount;float modifier;
    if((ctx&&ctx->heap!=s->object.heap)||(stack&&(!ItemStack_isInstance((MCObject *)stack)||stack->object.heap!=s->object.heap)))goto done;
    if(!MCObjectRootScope_pin(&scope,ctx)||!MCObjectRootScope_pin(&scope,(MCObject *)stack)||!item||!d||!d->getHealAmount)goto done;
    if(!effect(s,d->getHealAmount(ctx,item,stack,&amount)))goto done;
    if(!d->getSaturationModifier||!effect(s,d->getSaturationModifier(ctx,item,stack,&modifier)))goto done;
    ok=FoodStats_addStats(s,amount,modifier);
done:if(!ok)MCObjectHeap_fail(s->object.heap);MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(s->object.heap);
}
static MCObject *borrow(FoodStats *s,MCObjectRootScope *scope,MCObject *o){
    if(!o||o->heap!=s->object.heap||!MCObjectRootScope_pin(scope,o)||MCObjectHeap_failed(s->object.heap)){MCObjectHeap_fail(s->object.heap);return NULL;}return o;
}
bool FoodStats_onUpdate(FoodStats *s,MCObject *player,const FoodStatsPlayerDependencies *d,MCObject *ctx){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return false;bool ok=false;int32_t difficulty;bool regenerate=false;
    if(!player||player->heap!=s->object.heap||(ctx&&ctx->heap!=s->object.heap)||!d||!d->getWorld||
       !MCObjectRootScope_pin(&scope,player)||!MCObjectRootScope_pin(&scope,ctx))goto done;
    MCObject *world=borrow(s,&scope,d->getWorld(ctx,player));
    if(!world||!d->getDifficulty||!effect(s,d->getDifficulty(ctx,world,&difficulty)))goto done;
    s->prevFoodLevel=s->foodLevel;MCObjectHeap_touch(s->object.heap);
    if(s->foodExhaustionLevel>4.0f){
        volatile float exhausted=s->foodExhaustionLevel-4.0f;s->foodExhaustionLevel=exhausted;
        if(s->foodSaturationLevel>0.0f){volatile float saturation=s->foodSaturationLevel-1.0f;s->foodSaturationLevel=maxf(saturation,0.0f);}
        else if(difficulty!=0){int32_t level=signed32((uint32_t)s->foodLevel-UINT32_C(1));s->foodLevel=level>0?level:0;}
        MCObjectHeap_touch(s->object.heap);
    }
    world=borrow(s,&scope,d->getWorld(ctx,player));
    if(!world||!d->getGameRules)goto done;
    MCObject *rules=borrow(s,&scope,d->getGameRules(ctx,world));
    if(!rules||!d->getBoolean||!effect(s,d->getBoolean(ctx,rules,"naturalRegeneration",&regenerate)))goto done;
    bool heal=false,eligible=regenerate&&s->foodLevel>=18;
    if(eligible){if(!d->shouldHeal||!effect(s,d->shouldHeal(ctx,player,&heal)))goto done;}
    if(eligible&&heal){
        s->foodTimer=signed32((uint32_t)s->foodTimer+UINT32_C(1));MCObjectHeap_touch(s->object.heap);
        if(s->foodTimer>=80){
            if(!d->heal||!effect(s,d->heal(ctx,player,1.0f)))goto done;
            if(!FoodStats_addExhaustion(s,3.0f))goto done;
            s->foodTimer=0;MCObjectHeap_touch(s->object.heap);
        }
    }else if(s->foodLevel<=0){
        s->foodTimer=signed32((uint32_t)s->foodTimer+UINT32_C(1));MCObjectHeap_touch(s->object.heap);
        if(s->foodTimer>=80){
            float health;bool starve=false;
            if(!d->getHealth||!effect(s,d->getHealth(ctx,player,&health)))goto done;
            if(health>10.0f||difficulty==3)starve=true;
            else{if(!effect(s,d->getHealth(ctx,player,&health)))goto done;starve=health>1.0f&&difficulty==2;}
            if(starve){bool accepted;if(!d->attackStarve||!effect(s,d->attackStarve(ctx,player,1.0f,&accepted)))goto done;}
            s->foodTimer=0;MCObjectHeap_touch(s->object.heap);
        }
    }else{s->foodTimer=0;MCObjectHeap_touch(s->object.heap);}
    ok=true;
done:if(!ok)MCObjectHeap_fail(s->object.heap);MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(s->object.heap);
}
static bool nbt_valid(FoodStats *s,NBTTagCompound *n){return effect(s,n&&MCObjectHeap_objectSize((MCObject *)n)>=sizeof *n&&NBTBase_getId((NBTBase *)n)==10&&((MCObject *)n)->heap==s->object.heap);}
bool FoodStats_readNBT(FoodStats *s,NBTTagCompound *n){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return false;bool ok=nbt_valid(s,n)&&MCObjectRootScope_pin(&scope,(MCObject *)n);
    if(ok&&NBTTagCompound_hasKeyType_ascii(n,"foodLevel",99)){
        s->foodLevel=NBTTagCompound_getInteger_ascii(n,"foodLevel");MCObjectHeap_touch(s->object.heap);
        if(!MCObjectHeap_failed(s->object.heap)){s->foodTimer=NBTTagCompound_getInteger_ascii(n,"foodTickTimer");MCObjectHeap_touch(s->object.heap);}
        if(!MCObjectHeap_failed(s->object.heap)){s->foodSaturationLevel=NBTTagCompound_getFloat_ascii(n,"foodSaturationLevel");MCObjectHeap_touch(s->object.heap);}
        if(!MCObjectHeap_failed(s->object.heap)){s->foodExhaustionLevel=NBTTagCompound_getFloat_ascii(n,"foodExhaustionLevel");MCObjectHeap_touch(s->object.heap);}
    }
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(s->object.heap);
}
bool FoodStats_writeNBT(FoodStats *s,NBTTagCompound *n){
    MCObjectRootScope scope={0};if(!begin(s,&scope))return false;
    bool ok=nbt_valid(s,n)&&MCObjectRootScope_pin(&scope,(MCObject *)n)&&
        NBTTagCompound_setInteger_ascii(n,"foodLevel",s->foodLevel)&&NBTTagCompound_setInteger_ascii(n,"foodTickTimer",s->foodTimer)&&
        NBTTagCompound_setFloat_ascii(n,"foodSaturationLevel",s->foodSaturationLevel)&&NBTTagCompound_setFloat_ascii(n,"foodExhaustionLevel",s->foodExhaustionLevel);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(s->object.heap);
}
int32_t FoodStats_getFoodLevel(const FoodStats *s){return valid(s)?s->foodLevel:0;}
int32_t FoodStats_getPrevFoodLevel(const FoodStats *s){return valid(s)?s->prevFoodLevel:0;}
bool FoodStats_needFood(const FoodStats *s){return valid(s)&&s->foodLevel<20;}
bool FoodStats_addExhaustion(FoodStats *s,float amount){if(!valid(s))return false;volatile float sum=s->foodExhaustionLevel+amount;s->foodExhaustionLevel=minf(sum,40.0f);MCObjectHeap_touch(s->object.heap);return true;}
float FoodStats_getSaturationLevel(const FoodStats *s){return valid(s)?s->foodSaturationLevel:0;}
void FoodStats_setFoodLevel(FoodStats *s,int32_t n){if(valid(s)){s->foodLevel=n;MCObjectHeap_touch(s->object.heap);}}
void FoodStats_setFoodSaturationLevel(FoodStats *s,float f){if(valid(s)){s->foodSaturationLevel=f;MCObjectHeap_touch(s->object.heap);}}
