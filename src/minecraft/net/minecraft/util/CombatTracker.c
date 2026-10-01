#include "util/CombatTracker.h"
#include "entity/EntityLivingBase.h"
static void array_trace(MCObject *o,MCObjectVisitor visitor,void *context) {
    CombatObjectArray *a=(CombatObjectArray *)o;
    for(int32_t i=0;i<a->length;i++)a->items[i]=visitor(a->items[i],context);
}
static const MCObjectClass array_class={"native.CombatTracker.ObjectArray",MCObjectHeap_plainClone,array_trace,NULL};
typedef struct {MCObject object;CombatObjectArray *empty,*defaultEmpty;} ArrayStatics;
static void statics_trace(MCObject *o,MCObjectVisitor visitor,void *context) {
    ArrayStatics *s=(ArrayStatics *)o;
    s->empty=(CombatObjectArray *)visitor((MCObject *)s->empty,context);
    s->defaultEmpty=(CombatObjectArray *)visitor((MCObject *)s->defaultEmpty,context);
}
static const MCObjectClass statics_class={"native.CombatTracker.ArrayListStatics",MCObjectHeap_plainClone,statics_trace,NULL};
static bool any(const MCObject *o,void *context){(void)o;(void)context;return true;}
static ArrayStatics *statics(MCObjectHeap *heap) {
    ArrayStatics *s=(ArrayStatics *)MCObjectHeap_findObject(heap,&statics_class,any,NULL);
    if(s)return s;
    s=(ArrayStatics *)MCObjectHeap_alloc(heap,sizeof *s,&statics_class);if(!s)return NULL;
    s->empty=(CombatObjectArray *)MCObjectHeap_alloc(heap,sizeof *s->empty,&array_class);
    s->defaultEmpty=(CombatObjectArray *)MCObjectHeap_alloc(heap,sizeof *s->defaultEmpty,&array_class);
    MCObjectRoot root={0};
    return s->empty&&s->defaultEmpty&&MCObjectRoot_init(&root,heap,(MCObject *)s)?s:NULL;
}
static void list_trace(MCObject *o,MCObjectVisitor visitor,void *context) {
    CombatEntryList *l=(CombatEntryList *)o;l->elementData=(CombatObjectArray *)visitor((MCObject *)l->elementData,context);
}
static const MCObjectClass list_class={"native.CombatTracker.ArrayList",MCObjectHeap_plainClone,list_trace,NULL};
static void tracker_trace(MCObject *o,MCObjectVisitor visitor,void *context) {
    CombatTracker *t=(CombatTracker *)o;
    t->combatEntries=(CombatEntryList *)visitor((MCObject *)t->combatEntries,context);
    t->fighter=(EntityLivingBase *)visitor((MCObject *)t->fighter,context);
    t->field_94551_f=(NBTString *)visitor((MCObject *)t->field_94551_f,context);
}
static const MCObjectClass tracker_class={"net.minecraft.util.CombatTracker",MCObjectHeap_plainClone,tracker_trace,NULL};
bool CombatTracker_isInstance(const MCObject *o) {return o&&o->klass==&tracker_class&&MCObjectHeap_objectSize(o)>=sizeof(CombatTracker);}
bool CombatEntryList_isInstance(const MCObject *o) {return o&&o->klass==&list_class&&MCObjectHeap_objectSize(o)>=sizeof(CombatEntryList);}
CombatTracker *CombatTracker_new(MCObjectHeap *heap,EntityLivingBase *fighter) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    CombatTracker *out=NULL;
    if(fighter&&(!EntityLivingBase_isInstance((MCObject *)fighter)||fighter->entity.object.heap!=heap))goto done;
    if(!MCObjectRootScope_pin(&scope,(MCObject *)fighter))goto done;
    CombatTracker *t=(CombatTracker *)MCObjectHeap_alloc(heap,sizeof *t,&tracker_class);if(!t)goto done;
    ArrayStatics *s=statics(heap);if(!s)goto done;
    t->combatEntries=(CombatEntryList *)MCObjectHeap_alloc(heap,sizeof *t->combatEntries,&list_class);
    if(!t->combatEntries)goto done;
    t->combatEntries->elementData=s->defaultEmpty;
    t->fighter=fighter;MCObjectHeap_touch(heap);out=t;
done:
    if(!out)MCObjectHeap_fail(heap);
    MCObjectRootScope_end(&scope);return out;
}
