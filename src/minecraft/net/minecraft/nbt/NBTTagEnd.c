#include "nbt/NBTInternal.h"
static const MCObjectClass endClass={"NBTTagEnd",MCObjectHeap_plainClone,NULL,NULL};
NBTTagEnd *NBTTagEnd_new(MCObjectHeap *heap) {
    NBTTagEnd *tag=(NBTTagEnd*)MCObjectHeap_alloc(heap,sizeof(*tag),&endClass);
    if (tag) tag->base.type=0;
    return tag;
}
