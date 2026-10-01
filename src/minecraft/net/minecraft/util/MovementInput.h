#ifndef C919_SOURCE_MOVEMENT_INPUT_H
#define C919_SOURCE_MOVEMENT_INPUT_H
#include "util/MCObjectHeap.h"
/* Original four fields and implicit constructor. MovementInputFromOptions and
   its GameSettings/key bindings are a separate, untranslated dependency. */
typedef struct MovementInput {
  MCObject object;
  float moveStrafe, moveForward;
  bool jump, sneak;
} MovementInput;
MovementInput *MovementInput_new(MCObjectHeap *);
bool MovementInput_isInstance(const MCObject *);
/* This method's supplied original base body is empty. */
bool MovementInput_updatePlayerMoveState(MovementInput *);
#endif
