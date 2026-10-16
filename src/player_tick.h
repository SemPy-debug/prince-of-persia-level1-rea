#ifndef POP_PLAYER_TICK_H
#define POP_PLAYER_TICK_H
#include "controller.h"

/* Original22F4 prefix: D0A8 actor copies,3A09 geometry and F918 guard.
 * Fallible effect callbacks must use the caller's staged world transaction. */
PopControllerResult pop_player_prepare_tick(PopControllerState *state,
                                           const PopControllerIO *io);
/* Original2519/258D envelope around the native human-input controller. Inputs
 * are already sampled host axes/button. Source20E2/25AA/2503 transfers return
 * typed RESTART_LEVEL/GAME_OVER/TIME_EXPIRED and commit their actual transfer
 * state. Missing implementation callbacks still roll back and name a helper;
 * no classic system-bank UI is executed by a typed transfer. */
PopControllerResult pop_player_sample_tick_input(PopControllerState *state,
    uint8_t horizontal,uint8_t vertical,uint8_t button,const PopControllerIO *io);
/* Original27C8 updates C5 and the modifier display byte0318. */
void pop_player_tick_modifier(PopControllerState *state);
#endif
