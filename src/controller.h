#ifndef POP_CONTROLLER_H
#define POP_CONTROLLER_H
#include <stdbool.h>
#include <stdint.h>

/* Original semantic variables, not CPU state. The working actor occupies
 * aux[0x40..0x4f]. Scratch variables retain their source addresses so separately
 * reconstructed world routines share the original data contract. */
typedef struct PopControllerState {
    uint8_t aux[1024];
    uint16_t required_helper;
    uint8_t selected_sequence;
} PopControllerState;

/* Values/conditions returned by an original geometry predicate. These are a
 * function result contract, not registers in an instruction interpreter. */
typedef struct PopControllerQueryResult {
    uint8_t a, x, y;
    bool carry, zero, negative;
} PopControllerQueryResult;
typedef bool (*PopControllerQuery)(void *context, uint16_t helper_id,
    uint8_t argument, PopControllerState *state, PopControllerQueryResult *result);
typedef bool (*PopControllerRead)(void *context, uint16_t address, uint8_t *value);
typedef struct PopControllerIO {
    PopControllerQuery query;
    PopControllerRead read;
    void *context;
} PopControllerIO;
typedef enum PopControllerResult {
    POP_CONTROLLER_COMPLETE = 0,
    POP_CONTROLLER_HELPER_REQUIRED = 1,
    POP_CONTROLLER_INVALID = 2,
    POP_CONTROLLER_RESTART_LEVEL = 3,
    POP_CONTROLLER_GAME_OVER = 4,
    POP_CONTROLLER_TIME_EXPIRED = 5
} PopControllerResult;

void pop_controller_update_latches(PopControllerState *state);
void pop_controller_mirror_horizontal(PopControllerState *state);
void pop_controller_save_latches(PopControllerState *state);
void pop_controller_restore_latches(PopControllerState *state);
void pop_controller_select_sequence(PopControllerState *state, uint8_t sequence_id);
/* AUX3D49 entry: input/latches already relative to facing. A missing world
 * predicate rolls back semantic-variable mutations and names its source ID. */
PopControllerResult pop_controller_decide(PopControllerState *state,
                                         const PopControllerIO *io);
/* AUX3CDA human-input path after platform input sampling. Applies the original
 * timer/life check, saved latches, normalization and save around 3D49. Demo
 * input (03F4==0) is an explicit unsupported original helper. */
PopControllerResult pop_controller_sampled_player(PopControllerState *state,
    uint8_t horizontal, uint8_t vertical, uint8_t button,
    const PopControllerIO *io);
#endif
