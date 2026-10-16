#ifndef POP_WORLD_MECHANISMS_H
#define POP_WORLD_MECHANISMS_H
#include "world_effects.h"

/* Source secondary records: six sixteen-byte arrays, scratch slot0 and
 * records1..15, count in AUX030D. Dirty arrays complement EffectMemory's
 * full_tile/wipe_repeat/wipe_height/top without duplicate owned bytes. */
typedef struct PopWorldMechanismMemory {
    uint8_t falling[6][16]; /* B660..B6BF */
    uint8_t half_floor[30]; /* 5E3C..5E59 */
    uint8_t foreground[30]; /* 5E78..5E95 */
    uint8_t floor[30];      /* 5E96..5EB3 */
    uint8_t movable[30];    /* 5ED2..5EEF */
    uint8_t object[30];     /* 5EF0..5F0D */
} PopWorldMechanismMemory;

bool pop_world_mechanism_memory_reset(PopWorldMechanismMemory *memory);
bool pop_world_mechanism_memory_read(void *memory, uint16_t address, uint8_t *value);
bool pop_world_mechanism_memory_write(void *memory, uint16_t address, uint8_t value);

/* EE00/F0AF main object queue and EE0F/F565 falling-object physics.
 * Native data algorithms for persistent queues. All objects and external IO
 * must alias one caller-staged Runtime; caller rolls back the whole transaction
 * on false. Unknown helpers/data return false; no original CPU execution. */
bool pop_world_mechanism_query_io(PopWorldState *world, PopWorldEffects *effects,
    PopWorldEffectMemory *effect_memory, PopWorldMechanismMemory *mechanism_memory,
    uint16_t helper_id, uint8_t argument, PopControllerState *state,
    PopControllerQueryResult *result, const PopControllerIO *io,
    PopWorldEffectWrite write);
#endif
