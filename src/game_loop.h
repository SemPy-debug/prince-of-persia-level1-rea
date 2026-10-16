#ifndef POP_GAME_LOOP_H
#define POP_GAME_LOOP_H
#include "game.h"
#include "loop_state.h"
#include "loop_input.h"

typedef struct PopGameLoop {
    PopLoopModal modal;
    PopLoopVisual visual;
    bool awaiting_presentation,complete;
    PopLoopInputState input;
    PopLoopInputEvents input_events;
} PopGameLoop;
typedef enum PopGameLoopResult {
    POP_LOOP_FRAME_READY=0,POP_LOOP_MESSAGE=1,POP_LOOP_LEVEL_COMPLETE=2,
    POP_LOOP_HELPER_REQUIRED=3,POP_LOOP_INVALID=4,POP_LOOP_WAIT_KEY=5,
    POP_LOOP_RESTART_LEVEL=6,POP_LOOP_NEW_GAME=7,POP_LOOP_PLATFORM_TRANSFER=8,
    POP_LOOP_GAME_OVER=9,POP_LOOP_TIME_EXPIRED=10
} PopGameLoopResult;
bool pop_game_loop_reset(PopGameLoop *loop,PopGame *game);
/* Source218C caller around224E. The host supplies physical axes in place of
 * Apple keyboard/joystick axes. The default D93F hardware packet contains
 * the released, acknowledged last key and the supplied action button.
 * The successful boundary is immediately before22DD render/EA00 audio.
 * Both owned structures roll back on an unmet native helper. */
PopGameLoopResult pop_game_loop_before_presentation(PopGameLoop *loop,
    PopGame *game,PopGameInput input);
/* Explicit original keyboard/joystick packet for source comparisons and
 * platform shortcut handling. Typed waits/transfers stop before224E. */
PopGameLoopResult pop_game_loop_before_presentation_with_hardware(PopGameLoop *loop,
    PopGame *game,PopGameInput input,const PopLoopInputHardware *hardware);
/* Fully source-sampled input path: D93F hardware sample followed by047E's
 * keyboard/paddle cache and button merge at the player-controller boundary. */
PopGameLoopResult pop_game_loop_before_source_presentation(PopGameLoop *loop,
    PopGame *game,const PopLoopInputHardware *hardware);
/* Call after the host has completed the native22DD presentation and drained
 * the source EA00 sound requests. This performs D906,24AD,2641 and21BA.
 * It does not substitute for the renderer's semantic state commit. */
PopGameLoopResult pop_game_loop_after_presentation(PopGameLoop *loop,PopGame *game);
bool pop_game_loop_dismiss_message(PopGameLoop *loop,PopGame *game);
#endif
