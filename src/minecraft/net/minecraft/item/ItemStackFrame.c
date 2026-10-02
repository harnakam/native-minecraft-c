#include "item/ItemStackFrame.h"
#include "entity/item/EntityItemFrame.h"
static bool same(const MCObject *o,void *context){return o==context;}
static bool tracked(MCObjectHeap *h,MCObject *o) {
    return !o||(o->heap==h&&MCObjectHeap_findObject(h,o->klass,same,o)==o);
}
static bool valid(ItemStack *s) {
    if(s&&tracked(s->object.heap,(MCObject *)s)&&ItemStack_isInstance((MCObject *)s)&&
       MCObjectHeap_objectSize((MCObject *)s)>=sizeof(ItemStack)&&
       !MCObjectHeap_failed(s->object.heap))return true;
    MCObjectHeap_fail(s?s->object.heap:NULL);return false;
}
bool ItemStack_isOnItemFrame(ItemStack *s) {return valid(s)&&s->itemFrame!=NULL;}
bool ItemStack_setItemFrame(ItemStack *s,EntityItemFrame *f) {
    if(!valid(s))return false;
    if(!tracked(s->object.heap,(MCObject *)f)||(f&&!EntityItemFrame_isInstance((MCObject *)f))) {
        MCObjectHeap_fail(s->object.heap);return false;
    }
    s->itemFrame=f;MCObjectHeap_touch(s->object.heap);return true;
}
EntityItemFrame *ItemStack_getItemFrame(ItemStack *s) {
    if(!valid(s))return NULL;
    EntityItemFrame *f=s->itemFrame;
    if(!tracked(s->object.heap,(MCObject *)f)||(f&&!EntityItemFrame_isInstance((MCObject *)f))) {
        MCObjectHeap_fail(s->object.heap);return NULL;
    }
    return f;
}
