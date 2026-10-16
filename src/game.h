#ifndef POP_GAME_H
#define POP_GAME_H

#include "runtime.h"

/* Physical axes: FF=left/up,01=right/down,00=released. Action uses its high
 * bit as in the observed human-input sampler. No host keyboard codes here. */
typedef struct PopGameInput {
    uint8_t horizontal;
    uint8_t vertical;
    uint8_t action;
} PopGameInput;

typedef struct PopGame {
    PopRuntime runtime;
    PopControllerState state;
    uint64_t ticks;
    uint16_t required_helper;
    bool initialized;
} PopGame;

typedef enum PopGameResult {
    POP_GAME_UPDATED = 0,
    POP_GAME_HELPER_REQUIRED = 1,
    POP_GAME_INVALID = 2,
    POP_GAME_RESTART_LEVEL = 3,
    POP_GAME_GAME_OVER = 4,
    POP_GAME_TIME_EXPIRED = 5
} PopGameResult;

bool pop_game_reset(PopGame *game);
/* Native player update in original22F4 order. Unknown effects or transfers
 * roll back the frame and name the unmet boundary; no failed stage is skipped.
 * Legitimate restart/game-over/time transfers commit their source prefix and
 * return a typed result before remaining player work. This API alone does
 * not include outer-loop guard/room/clock controllers. */
PopGameResult pop_game_step(PopGame *game, PopGameInput input);
/* Original224E outer gameplay update, with the player, guard, mechanisms,
 * combat, room and clock stages. The entire owned state is transactional. */
PopGameResult pop_game_frame(PopGame *game, PopGameInput input);

#endif
