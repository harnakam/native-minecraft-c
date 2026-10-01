#include "command/CommandResultStats.h"
static void array_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    CommandResultStringArray *array=(CommandResultStringArray *)object;
    for(unsigned i=0;i<5;i++)array->items[i]=(NBTString *)visitor((MCObject *)array->items[i],context);
}
static const MCObjectClass array_class={"native.CommandResultStats.StringArray",MCObjectHeap_plainClone,array_trace,NULL};
bool CommandResultStringArray_isInstance(const MCObject *object) {return object&&object->klass==&array_class&&MCObjectHeap_objectSize(object)>=sizeof(CommandResultStringArray);}
typedef struct {MCObject object;CommandResultStringArray *STRING_RESULT_TYPES;} CommandStatics;
static void static_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    CommandStatics *fields=(CommandStatics *)object;
    fields->STRING_RESULT_TYPES=(CommandResultStringArray *)visitor((MCObject *)fields->STRING_RESULT_TYPES,context);
}
static const MCObjectClass static_class={"native.CommandResultStats.statics",MCObjectHeap_plainClone,static_trace,NULL};
static bool any(const MCObject *object,void *context) {(void)object;(void)context;return true;}
static CommandStatics *class_statics(MCObjectHeap *heap) {
    CommandStatics *fields=(CommandStatics *)MCObjectHeap_findObject(heap,&static_class,any,NULL);
    if(fields)return fields;
    fields=(CommandStatics *)MCObjectHeap_alloc(heap,sizeof *fields,&static_class);
    if(!fields)return NULL;
    fields->STRING_RESULT_TYPES=(CommandResultStringArray *)MCObjectHeap_alloc(heap,sizeof *fields->STRING_RESULT_TYPES,&array_class);
    MCObjectRoot root={0};
    return fields->STRING_RESULT_TYPES&&MCObjectRoot_init(&root,heap,(MCObject *)fields)?fields:NULL;
}
static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    CommandResultStats *stats=(CommandResultStats *)object;
    stats->entitiesID=(CommandResultStringArray *)visitor((MCObject *)stats->entitiesID,context);
    stats->objectives=(CommandResultStringArray *)visitor((MCObject *)stats->objectives,context);
}
static const MCObjectClass klass={"net.minecraft.command.CommandResultStats",MCObjectHeap_plainClone,trace,NULL};
bool CommandResultStats_isInstance(const MCObject *object) {return object&&object->klass==&klass&&MCObjectHeap_objectSize(object)>=sizeof(CommandResultStats);}
CommandResultStats *CommandResultStats_new(MCObjectHeap *heap) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    CommandStatics *fields=class_statics(heap);
    CommandResultStats *stats=fields?(CommandResultStats *)MCObjectHeap_alloc(heap,sizeof *stats,&klass):NULL;
    if(stats){stats->entitiesID=fields->STRING_RESULT_TYPES;stats->objectives=fields->STRING_RESULT_TYPES;}
    MCObjectRootScope_end(&scope);return stats;
}
