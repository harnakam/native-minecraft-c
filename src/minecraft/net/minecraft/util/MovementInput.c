#include "util/MovementInput.h"
static const MCObjectClass klass = {"net.minecraft.util.MovementInput",
                                    MCObjectHeap_plainClone, NULL, NULL};
MovementInput *MovementInput_new(MCObjectHeap *heap) {
  return (MovementInput *)MCObjectHeap_alloc(heap, sizeof(MovementInput),
                                             &klass);
}
bool MovementInput_isInstance(const MCObject *object) {
  return object && object->klass == &klass &&
         MCObjectHeap_objectSize(object) >= sizeof(MovementInput);
}
bool MovementInput_updatePlayerMoveState(MovementInput *self) {
  if (!MovementInput_isInstance((MCObject *)self)) {
    MCObjectHeap_fail(self ? self->object.heap : NULL);
    return false;
  }
  return !MCObjectHeap_failed(self->object.heap);
}
