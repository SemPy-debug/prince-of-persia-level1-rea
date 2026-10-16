#ifndef POP_ACTOR_STATE_H
#define POP_ACTOR_STATE_H

#include <stddef.h>
#include <stdint.h>

/* Documentary 16-byte representation, corroborated by original copy sites.
 * See research/player-state-evidence/REPORT.md. Keep bytes unsigned here;
 * consumers must explicitly reproduce their observed signed/unsigned rules.
 * This declares storage only, not movement behavior or a recovered tick rate. */
typedef struct PopActorState {
    uint8_t pose;
    uint8_t x;
    uint8_t y;
    uint8_t facing;
    uint8_t block_x;
    uint8_t block_y;
    uint8_t action;
    uint8_t velocity_x;
    uint8_t velocity_y;
    uint8_t sequence_lo;
    uint8_t sequence_hi;
    uint8_t room;
    uint8_t repeat;
    uint8_t character_id;
    uint8_t sword;
    uint8_t life;
} PopActorState;

_Static_assert(sizeof(PopActorState) == 16, "Original actor record is 16 bytes");
_Static_assert(offsetof(PopActorState, sequence_lo) == 9, "Sequence byte offset");
_Static_assert(offsetof(PopActorState, life) == 15, "Life byte offset");

#endif
