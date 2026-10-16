#ifndef POP_GUARD_COMBAT_H
#define POP_GUARD_COMBAT_H
#include "controller.h"
/* Original5403/5867 two-sided sword contact and5406/58EB hit resolution.
 * IO must refer to the caller-staged runtime, preserving animation/sound order. */
PopControllerResult pop_guard_check_sword_contact(PopControllerState *state,const PopControllerIO *io);
PopControllerResult pop_guard_resolve_sword_hits(PopControllerState *state,const PopControllerIO *io);
#endif
