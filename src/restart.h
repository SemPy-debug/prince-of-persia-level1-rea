#ifndef POP_RESTART_H
#define POP_RESTART_H
#include "game.h"
/* The presentation commit is a source-native semantic adapter, e.g.
 * pop_render_commit. Its helper ID is2439; no original code is executed. */
typedef bool (*PopRestartCommit)(PopRuntime*,PopControllerState*,uint16_t,uint16_t*);
/* Original20E2 through the actual22EA entry, before room/display activation. */
bool pop_game_restart_prepare(PopGame *game);
/* Complete20E2->218C composition. Both functions stage all owned state and
 * preserve host ticks and original source-unwritten variables. Level1 only. */
bool pop_game_restart(PopGame *game,PopRestartCommit commit);
/* Source2015->2046->20E2 for a new first-level game. Clears elapsed time and
 * starts with health3; retains incoming RNG until source room activation. */
bool pop_game_new_game(PopGame *game,PopRestartCommit commit);
#endif
