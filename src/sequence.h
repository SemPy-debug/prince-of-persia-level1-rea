#ifndef POP_SEQUENCE_H
#define POP_SEQUENCE_H

#include "actor_state.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A game-specific animation bytecode interpreter, independently expressed in C.
 * This does not execute machine instructions or supply world/audio behavior.
 * Field names with addresses retain their exact original meaning pending the
 * separate world implementation. See research/animation-evidence/summary-4945.json. */
typedef enum PopSequenceEffect {
    POP_SEQUENCE_EFFECT_NONE = 0,
    POP_SEQUENCE_EFFECT_E01E = 1,
    POP_SEQUENCE_EFFECT_D909 = 2,
    POP_SEQUENCE_EFFECT_4A2D = 3,
    POP_SEQUENCE_EFFECT_F90F = 4
} PopSequenceEffect;

typedef struct PopSequenceState {
    PopActorState actor;
    uint8_t flag_c5;
    uint8_t flag_cd;
    uint8_t flag_e4;
    uint8_t pending_effect;
    uint8_t effect_argument;
} PopSequenceState;

typedef bool (*PopSequenceRead)(void *context, uint16_t address, uint8_t *value);
/* Return false if the caller cannot provide the required original effect.
 * The request stays pending; no later command or pose is consumed. Returning
 * true accepts the effect and any mutations made by the caller to state. */
typedef bool (*PopSequenceApplyEffect)(void *context, PopSequenceEffect effect,
                                       uint8_t argument, PopSequenceState *state);

typedef enum PopSequenceResult {
    POP_SEQUENCE_POSE = 0,
    POP_SEQUENCE_EFFECT_REQUIRED = 1,
    POP_SEQUENCE_UNREADABLE = 2,
    POP_SEQUENCE_LIMIT = 3,
    POP_SEQUENCE_INVALID = 4
} PopSequenceResult;

/* command_limit bounds malformed/self-looping input; it is not game cadence.
 * A failed operand read leaves that command's actor/flags/pointer untouched.
 * Earlier completed commands remain applied. Pending effects are explicit. */
PopSequenceResult pop_sequence_run(PopSequenceState *state, PopSequenceRead read,
                                  PopSequenceApplyEffect apply_effect,
                                  void *context, size_t command_limit);

#endif
