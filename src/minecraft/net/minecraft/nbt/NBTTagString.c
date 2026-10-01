#include "nbt/NBTInternal.h"
static void trace(MCObject *object,MCObjectVisitor visit,void *context) {
    NBTTagString *tag=(NBTTagString*)object; tag->data=(NBTString*)visit((MCObject*)tag->data,context);
}
static const MCObjectClass tagClass={"NBTTagString",MCObjectHeap_plainClone,trace,NULL};
NBTTagString *NBTTagString_new(MCObjectHeap *heap,NBTString *value) {
    if (!value || !nbt_sameHeap(heap,(MCObject*)value)) { MCObjectHeap_fail(heap); return NULL; }
    NBTTagString *tag=(NBTTagString*)MCObjectHeap_alloc(heap,sizeof(*tag),&tagClass);
    if (tag) { tag->base.type=8;tag->data=value; }
    return tag;
}
NBTString *NBTTagString_getString(const NBTTagString *tag) { return tag?tag->data:NULL; }
