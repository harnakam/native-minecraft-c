#include "world/GameRules.h"
#include "util/NativeJavaNumber.h"
#include "nbt/NBTInternal.h"
#include <limits.h>
const GameRulesValueType GameRules_ANY_VALUE={0},GameRules_BOOLEAN_VALUE={1},GameRules_NUMERICAL_VALUE={2};
static bool fail(MCObjectHeap *h){MCObjectHeap_fail(h);return false;}
static void trace_value(MCObject *o,MCObjectVisitor v,void *c){GameRulesValue *p=(GameRulesValue *)o;p->valueString=(NBTString *)v((MCObject *)p->valueString,c);}
static void trace_rules(MCObject *o,MCObjectVisitor v,void *c){GameRules *g=(GameRules *)o;g->theGameRules=(NativeSortedStringMap *)v((MCObject *)g->theGameRules,c);}
static void trace_array(MCObject *o,MCObjectVisitor v,void *c){GameRulesStringArray *a=(GameRulesStringArray *)o;for(int32_t i=0;i<a->length;i++)a->items[i]=(NBTString *)v((MCObject *)a->items[i],c);}
static const MCObjectClass valueClass={"net.minecraft.world.GameRules.Value",MCObjectHeap_plainClone,trace_value,NULL};
static const MCObjectClass rulesClass={"net.minecraft.world.GameRules",MCObjectHeap_plainClone,trace_rules,NULL};
static const MCObjectClass arrayClass={"native.GameRules.String[]",MCObjectHeap_plainClone,trace_array,NULL};
bool GameRules_isInstance(const MCObject *o){return o&&o->klass==&rulesClass&&MCObjectHeap_objectSize(o)>=sizeof(GameRules);}
bool GameRulesValue_isInstance(const MCObject *o){return o&&o->klass==&valueClass&&MCObjectHeap_objectSize(o)>=sizeof(GameRulesValue);}
static bool value_valid(GameRulesValue *p){return GameRulesValue_isInstance((MCObject *)p)&&!MCObjectHeap_failed(p->object.heap)?true:fail(p?p->object.heap:NULL);}
static bool valid(GameRules *g){return GameRules_isInstance((MCObject *)g)&&!MCObjectHeap_failed(g->object.heap)?true:fail(g?g->object.heap:NULL);}
static bool map_valid(GameRules *g){return NativeSortedStringMap_isInstance((MCObject *)g->theGameRules)&&g->theGameRules->object.heap==g->object.heap?true:fail(g->object.heap);}
static bool string_ref(MCObjectHeap *h,NBTString *s){return !s||(NBTString_isInstance((MCObject *)s)&&s->object.heap==h)?true:fail(h);}
static bool type_ref(const GameRulesValueType *t){return !t||t==&GameRules_ANY_VALUE||t==&GameRules_BOOLEAN_VALUE||t==&GameRules_NUMERICAL_VALUE;}
bool GameRulesValue_setValue(GameRulesValue *p,NBTString *text) {
    if(!value_valid(p)||!string_ref(p?p->object.heap:NULL,text))return false;
    MCObjectHeap *h=p->object.heap;MCObjectRootScope scope={0};bool ok=false;
    if(!MCObjectRootScope_begin(&scope,h)||!MCObjectRootScope_pin(&scope,(MCObject *)p)||!MCObjectRootScope_pin(&scope,(MCObject *)text)){MCObjectRootScope_end(&scope);return fail(h);}
    p->valueString=text;MCObjectHeap_touch(h);bool boolean;
    if(!NativeJavaNumber_parseBoolean(text,&boolean)){fail(h);goto done;}
    p->valueBoolean=boolean;p->valueInteger=boolean?1:0;MCObjectHeap_touch(h);
    int32_t integer;NativeJavaNumberResult result=NativeJavaNumber_parseInt(text,&integer);
    if(result==NATIVE_NUMBER_OK){p->valueInteger=integer;MCObjectHeap_touch(h);}
    else if(result!=NATIVE_NUMBER_FORMAT){fail(h);goto done;}
    double number;result=NativeJavaNumber_parseDouble(text,&number);
    if(result==NATIVE_NUMBER_OK){p->valueDouble=number;MCObjectHeap_touch(h);}
    else if(result!=NATIVE_NUMBER_FORMAT){fail(h);goto done;}
    ok=true;
done:
    MCObjectRootScope_end(&scope);return ok;
}
GameRulesValue *GameRulesValue_new(MCObjectHeap *h,NBTString *text,const GameRulesValueType *type) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    GameRulesValue *p=(GameRulesValue *)MCObjectHeap_alloc(h,sizeof *p,&valueClass);
    if(p){if(!type_ref(type)){fail(h);p=NULL;}else {p->type=type;MCObjectHeap_touch(h);if(!GameRulesValue_setValue(p,text))p=NULL;}}
    MCObjectRootScope_end(&scope);return p;
}
NBTString *GameRulesValue_getString(GameRulesValue *p){return value_valid(p)?p->valueString:NULL;}
bool GameRulesValue_getBoolean(GameRulesValue *p){return value_valid(p)&&p->valueBoolean;}
int32_t GameRulesValue_getInt(GameRulesValue *p){return value_valid(p)?p->valueInteger:0;}
const GameRulesValueType *GameRulesValue_getType(GameRulesValue *p){return value_valid(p)?p->type:NULL;}
GameRules *GameRules_nativeAllocate(MCObjectHeap *h){return (GameRules *)MCObjectHeap_alloc(h,sizeof(GameRules),&rulesClass);}
bool GameRules_addGameRule(GameRules *g,NBTString *key,NBTString *value,const GameRulesValueType *type) {
    if(!valid(g))return false;
    MCObjectHeap *h=g->object.heap;MCObjectRootScope scope={0};bool ok=false;
    if(!MCObjectRootScope_begin(&scope,h)||!MCObjectRootScope_pin(&scope,(MCObject *)g)){MCObjectRootScope_end(&scope);return fail(h);}
    NativeSortedStringMap *receiver=g->theGameRules;
    GameRulesValue *newValue=GameRulesValue_new(h,value,type);
    if(newValue&&NativeSortedStringMap_isInstance((MCObject *)receiver)&&receiver->object.heap==h)ok=NativeSortedStringMap_put(receiver,key,(MCObject *)newValue);
    if(!ok)fail(h);
    MCObjectRootScope_end(&scope);return ok;
}
bool GameRules_construct(GameRules *g) {
    if(!valid(g))return false;
    MCObjectHeap *h=g->object.heap;MCObjectRootScope scope={0};bool ok=false;
    if(!MCObjectRootScope_begin(&scope,h)||!MCObjectRootScope_pin(&scope,(MCObject *)g)){MCObjectRootScope_end(&scope);return fail(h);}
    NativeSortedStringMap *map=NativeSortedStringMap_new(h);if(!map)goto done;
    g->theGameRules=map;MCObjectHeap_touch(h);
    static const char *const names[15]={"doFireTick","mobGriefing","keepInventory","doMobSpawning","doMobLoot","doTileDrops","doEntityDrops","commandBlockOutput","naturalRegeneration","doDaylightCycle","logAdminCommands","showDeathMessages","randomTickSpeed","sendCommandFeedback","reducedDebugInfo"};
    for(size_t i=0;i<15;i++) {
        NBTString *key=NBTString_literalASCII(h,names[i]);
        NBTString *text=key?NBTString_literalASCII(h,i==12?"3":i==2||i==14?"false":"true"):NULL;
        if(!text||!GameRules_addGameRule(g,key,text,i==12?&GameRules_NUMERICAL_VALUE:&GameRules_BOOLEAN_VALUE))goto done;
    }
    ok=true;
done:
    MCObjectRootScope_end(&scope);return ok;
}
GameRules *GameRules_new(MCObjectHeap *h) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    GameRules *g=GameRules_nativeAllocate(h);if(g&&!GameRules_construct(g))g=NULL;
    MCObjectRootScope_end(&scope);return g;
}
static GameRulesValue *lookup(GameRules *g,NBTString *key) {
    if(!valid(g)||!map_valid(g))return NULL;
    MCObject *value=NativeSortedStringMap_get(g->theGameRules,key);
    if(value&&(!GameRulesValue_isInstance(value)||value->heap!=g->object.heap)){fail(g->object.heap);return NULL;}
    return (GameRulesValue *)value;
}
bool GameRules_setOrCreateGameRule(GameRules *g,NBTString *key,NBTString *text) {
    GameRulesValue *p=lookup(g,key);if(!g||MCObjectHeap_failed(g->object.heap))return false;
    return p?GameRulesValue_setValue(p,text):GameRules_addGameRule(g,key,text,&GameRules_ANY_VALUE);
}
NBTString *GameRules_getString(GameRules *g,NBTString *key) {
    if(!valid(g))return NULL;
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,g->object.heap))return NULL;
    GameRulesValue *p=lookup(g,key);NBTString *result=NULL;
    if(!MCObjectHeap_failed(g->object.heap))result=p?GameRulesValue_getString(p):NBTString_literalASCII(g->object.heap,"");
    MCObjectRootScope_end(&scope);return result;
}
bool GameRules_getBoolean(GameRules *g,NBTString *key){GameRulesValue *p=lookup(g,key);return p&&GameRulesValue_getBoolean(p);}
int32_t GameRules_getInt(GameRules *g,NBTString *key){GameRulesValue *p=lookup(g,key);return p?GameRulesValue_getInt(p):0;}
bool GameRules_hasRule(GameRules *g,NBTString *key){return valid(g)&&map_valid(g)&&NativeSortedStringMap_containsKey(g->theGameRules,key);}
bool GameRules_areSameType(GameRules *g,NBTString *key,const GameRulesValueType *type) {
    GameRulesValue *p=lookup(g,key);return p&&(GameRulesValue_getType(p)==type||type==&GameRules_ANY_VALUE);
}
GameRulesStringArray *GameRules_getRules(GameRules *g) {
    if(!valid(g)||!map_valid(g))return NULL;
    MCObjectHeap *h=g->object.heap;MCObjectRootScope scope={0};
    if(!MCObjectRootScope_begin(&scope,h)||!MCObjectRootScope_pin(&scope,(MCObject *)g)){MCObjectRootScope_end(&scope);return NULL;}
    int32_t n=g->theGameRules->size;GameRulesStringArray *a=NULL;
    if(n<0||(size_t)n>(SIZE_MAX-sizeof *a)/sizeof *a->items){fail(h);goto done;}
    a=(GameRulesStringArray *)MCObjectHeap_alloc(h,sizeof *a+(size_t)n*sizeof *a->items,&arrayClass);
    if(a){NativeSortedStringCursor cursor;a->length=n;if(!NativeSortedStringMap_beginCursor(g->theGameRules,&cursor))a=NULL;for(int32_t i=0;a&&i<n;i++){MCObject *value;if(!NativeSortedStringMap_next(&cursor,&a->items[i],&value)){a=NULL;break;}}}
done:
    MCObjectRootScope_end(&scope);return a;
}
NBTTagCompound *GameRules_writeToNBT(GameRules *g) {
    if(!valid(g))return NULL;
    MCObjectHeap *h=g->object.heap;MCObjectRootScope scope={0};
    if(!MCObjectRootScope_begin(&scope,h)||!MCObjectRootScope_pin(&scope,(MCObject *)g)){MCObjectRootScope_end(&scope);return NULL;}
    NBTTagCompound *tag=NBTTagCompound_new(h);
    if(tag&&!map_valid(g))tag=NULL;
    NativeSortedStringCursor cursor;
    if(tag&&!NativeSortedStringMap_beginCursor(g->theGameRules,&cursor))tag=NULL;
    while(tag&&cursor.next) {
        NBTString *key;MCObject *value;
        if(!NativeSortedStringMap_next(&cursor,&key,&value))tag=NULL;
        else if(!GameRulesValue_isInstance(value)||value->heap!=h){fail(h);tag=NULL;}
        else if(!NBTTagCompound_setString(tag,key,GameRulesValue_getString((GameRulesValue *)value)))tag=NULL;
    }
    MCObjectRootScope_end(&scope);return tag;
}
bool GameRules_readFromNBT(GameRules *g,NBTTagCompound *tag) {
    if(!valid(g)||!NBTTagCompound_isInstance((MCObject *)tag)||tag->base.object.heap!=g->object.heap)return fail(g?g->object.heap:NULL);
    MCObjectHeap *h=g->object.heap;MCObjectRootScope scope={0};bool ok=false;
    if(!MCObjectRootScope_begin(&scope,h)||!MCObjectRootScope_pin(&scope,(MCObject *)g)||!MCObjectRootScope_pin(&scope,(MCObject *)tag)){MCObjectRootScope_end(&scope);return fail(h);}
    NBTCompoundKeySet *keys=NBTTagCompound_getKeySet(tag);if(!keys)goto done;
    for(int32_t i=0;i<NBTCompoundKeySet_size(keys);i++) {
        NBTString *key=NBTCompoundKeySet_keyAt(keys,i);NBTString *text=NBTTagCompound_getString(tag,key);
        if(MCObjectHeap_failed(h)||!GameRules_setOrCreateGameRule(g,key,text))goto done;
    }
    ok=true;
done:
    MCObjectRootScope_end(&scope);return ok;
}
