#ifndef POP_LOOP_INPUT_H
#define POP_LOOP_INPUT_H
#include "loop_state.h"

typedef struct PopLoopInputState { bool waiting_key; } PopLoopInputState;
typedef struct PopLoopInputHardware {
    /* Actual sampled C000/C010/C061/C062 bytes; C010's low bits are retained,
     * not assumed zero. Source host adapters own key-strobe acknowledgement. */
    uint8_t keyboard_data,keyboard_status,button0,button1;
    /* Native platform abstraction of0981's measured3B/3C counters. No guessed
     * mapping from modern joystick percentages to CPU-timed paddle counts. */
    uint8_t paddle_x,paddle_y;
    bool paddle_sample_valid;
} PopLoopInputHardware;
typedef struct PopLoopInputEvents {
    bool acknowledge_key,beep;
    uint16_t transfer;
} PopLoopInputEvents;
typedef struct PopLoopCachedInput {uint8_t horizontal,vertical,button;} PopLoopCachedInput;
/*047E/08EA cache consumer, without writing18/19/3D early. Optional source
 * result describes its actual A/X/C/Z/N projection. The controller applies
 * these three values at its own original point inside2519. */
bool pop_loop_input_cached(const PopControllerState *state,
    PopLoopCachedInput *cached,PopControllerQueryResult *result);
typedef enum PopLoopInputResult {
    POP_LOOP_INPUT_COMPLETE=0,POP_LOOP_INPUT_WAIT_KEY=1,
    POP_LOOP_INPUT_RESTART_LEVEL=2,POP_LOOP_INPUT_NEW_GAME=3,
    POP_LOOP_INPUT_PLATFORM_TRANSFER=4,POP_LOOP_INPUT_HELPER_REQUIRED=5,
    POP_LOOP_INPUT_INVALID=6
} PopLoopInputResult;

void pop_loop_input_reset(PopLoopInputState *input);
/* One original D93F or continuation of its Escape busy-wait. COMPLETE is an
 * actual semantic return; WAIT_KEY/transfer are typed nonreturn boundaries.
 * AUX/result/input/events commit on these explicit boundaries. Missing typed
 * I/O rolls back semantic state and reports required_helper. Caller stages
 * IO-owned writes together with these values, as with other native modules. */
PopLoopInputResult pop_loop_input_sample(PopLoopInputState *input,
    const PopLoopInputHardware *hardware,PopControllerState *state,
    PopControllerQueryResult *result,PopLoopInputEvents *events,
    const PopControllerIO *io,PopLoopWrite write);
#endif
