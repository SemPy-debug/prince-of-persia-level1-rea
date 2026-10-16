#ifndef POP_NEWGAME_H
#define POP_NEWGAME_H
#include "controller.h"
/* Original AUX2015/2046 initialization, stopping before20E2 level reload.
 * IO must refer to caller-staged state when event3 calls native D930.
 * Does not choose/reset RNG, execute system-bank code, or load a level. */
bool pop_new_game_initialize(uint8_t level,PopControllerState *state,
    PopControllerQueryResult *result,const PopControllerIO *io);
#endif
