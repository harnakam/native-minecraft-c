#include "world/storage/SaveDataMemoryStorage.h"
static const MCObjectClass klass={"net.minecraft.world.storage.SaveDataMemoryStorage",MCObjectHeap_plainClone,MapStorage_nativeTraceFields,NULL};
bool SaveDataMemoryStorage_isInstance(const MCObject *o) {return o&&o->klass==&klass&&MCObjectHeap_objectSize(o)>=sizeof(SaveDataMemoryStorage);}
SaveDataMemoryStorage *SaveDataMemoryStorage_nativeNewCounterProvider(MCObjectHeap *h) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    SaveDataMemoryStorage *s=(SaveDataMemoryStorage *)MCObjectHeap_alloc(h,sizeof(*s),&klass);
    bool ok=s&&MapStorage_nativeInitializeCounterProvider(&s->base,NULL,NULL,NULL);
    MCObjectRootScope_end(&scope);return ok?s:NULL;
}
bool SaveDataMemoryStorage_getUniqueDataId(SaveDataMemoryStorage *s,NBTString *key,int32_t *out) {
    MCObjectHeap *h=s?s->base.object.heap:NULL;
    if(!SaveDataMemoryStorage_isInstance((MCObject *)s)||!out||MCObjectHeap_failed(h)||
       (key&&(!NBTString_isInstance((MCObject *)key)||((MCObject *)key)->heap!=h))) {MCObjectHeap_fail(h);return false;}
    /* Original override ignores the String and all superclass counter state. */
    *out=0;return true;
}
