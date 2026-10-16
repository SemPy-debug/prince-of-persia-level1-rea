#ifndef POP_GUARD_TICK_H
#define POP_GUARD_TICK_H
#include "motion.h"
/* Whole original234E guard update. Callback writes use a caller-staged
 * Runtime; state commits only after every native helper succeeds. */
PopControllerResult pop_guard_tick(PopControllerState *state,
    const PopControllerIO *io,PopMotionWrite write);
#endif
