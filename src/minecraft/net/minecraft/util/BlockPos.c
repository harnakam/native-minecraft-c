#include "util/BlockPos.h"

bool BlockPos_isInstance(const MCObject *object) {
    return DataWatcher_blockPosIsInstance(object)&&MCObjectHeap_objectSize(object)>=sizeof(BlockPos);
}
static int32_t signed_bits(uint32_t bits) {
    return bits<=INT32_MAX?(int32_t)bits:-1-(int32_t)(UINT32_MAX-bits);
}
BlockPos *BlockPos_add(BlockPos *self,int32_t x,int32_t y,int32_t z) {
    MCObjectHeap *heap=self?self->object.heap:NULL;
    if(!BlockPos_isInstance((MCObject *)self)){MCObjectHeap_fail(heap);return NULL;}
    MCObjectRootScope scope={0};
    if(!MCObjectRootScope_begin(&scope,heap)||!MCObjectRootScope_pin(&scope,(MCObject *)self)) {
        MCObjectHeap_fail(heap);MCObjectRootScope_end(&scope);return NULL;
    }
    BlockPos *result=x==0&&y==0&&z==0?self:DataWatcher_blockPos(heap,
        signed_bits((uint32_t)self->x+(uint32_t)x),signed_bits((uint32_t)self->y+(uint32_t)y),
        signed_bits((uint32_t)self->z+(uint32_t)z));
    MCObjectRootScope_end(&scope);return result;
}
typedef struct {MCObject object;BlockPos *ORIGIN;} OriginFields;
static void trace(MCObject *object,MCObjectVisitor visit,void *context) {
    OriginFields *fields=(OriginFields *)object;
    fields->ORIGIN=(BlockPos *)visit((MCObject *)fields->ORIGIN,context);
}
static const MCObjectClass statics={"native.BlockPos.statics",MCObjectHeap_plainClone,trace,NULL};
static bool any(const MCObject *object,void *context) {(void)object;(void)context;return true;}
BlockPos *NativeBlockPos_origin(MCObjectHeap *heap) {
    MCObjectRootScope scope={0};
    if(!MCObjectRootScope_begin(&scope,heap)){MCObjectHeap_fail(heap);return NULL;}
    OriginFields *fields=(OriginFields *)MCObjectHeap_findObject(heap,&statics,any,NULL);
    if(!fields) {
        fields=(OriginFields *)MCObjectHeap_alloc(heap,sizeof(*fields),&statics);
        if(fields)fields->ORIGIN=DataWatcher_blockPos(heap,0,0,0);
        MCObjectRoot root={0};
        if(!fields||!fields->ORIGIN||!MCObjectRoot_init(&root,heap,(MCObject *)fields))fields=NULL;
    }
    BlockPos *origin=fields?fields->ORIGIN:NULL;
    if(!BlockPos_isInstance((MCObject *)origin)||origin->object.heap!=heap||
        origin->x!=0||origin->y!=0||origin->z!=0) {
        MCObjectHeap_fail(heap);origin=NULL;
    }
    MCObjectRootScope_end(&scope);return origin;
}
