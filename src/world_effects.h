#ifndef POP_WORLD_EFFECTS_H
#define POP_WORLD_EFFECTS_H
#include "controller.h"
#include "world_state.h"

/* Original AUX B600/B620/B640 record storage. Slot0 is scratch;1..31
 * are owned records. The active count stays in state->aux[0x95]. No pointers
 * are embedded, so a runtime can stage/copy all three owned objects by value. */
typedef struct PopWorldEffects {
    uint8_t cell[32];
    uint8_t room[32];
    uint8_t phase[32];
} PopWorldEffects;

/* Mutable link-map timer bytes and exactly the dirty counters written by
 * these producers. Kept separate from the96-byte queue fixture contract. */
typedef struct PopWorldEffectMemory {
    uint8_t link_map[256];       /* BDA0..BE9F */
    uint8_t full_tile[30];       /* 5E5A..5E77 */
    uint8_t wipe_repeat[30];     /* 5EB4..5ED1 */
    uint8_t wipe_height[30];     /* 5F0E..5F2B */
    uint8_t top[10];             /* 5F2C..5F35 */
} PopWorldEffectMemory;
bool pop_world_effect_memory_reset(PopWorldEffectMemory *memory);
bool pop_world_effect_memory_read(void *memory, uint16_t address, uint8_t *value);
bool pop_world_effect_memory_write(void *memory, uint16_t address, uint8_t value);
typedef bool (*PopWorldEffectWrite)(void *context, uint16_t address, uint8_t value);

/* Native E01E/E0F4 and EE1B/F826 row scans, EE1E/EEE0 queue producer,
 * EE18/EF85 predicate, EE27/EF10 spec write and F927/FC61 damage request. Unsupported
 * helpers/data return false and preserve effects,world,state,result together.
 * No original instructions or CPU execution are part of this component. */
bool pop_world_effect_query(PopWorldEffects *effects, PopWorldState *world,
    uint16_t helper_id, uint8_t argument, PopControllerState *state,
    PopControllerQueryResult *result);

/* EE03/EE2A, EE0C, EE06/EE36 and3A0F, with F429/F40C/F445 dirty
 * producers. Native queue/link data algorithms call native sound/animation
 * helpers; no original instruction execution is part of these routines.
 * Extended producers operate in-place on one caller-staged runtime: world,
 * queue,memory,state and external IO must belong to that same transaction.
 * The caller rolls back the whole runtime on false. SharedB6E0 is accessed
 * through IOread/typedwrite; it is never duplicated in effect memory. */
bool pop_world_effect_query_io(PopWorldState *world, PopWorldEffects *effects,
    PopWorldEffectMemory *memory, uint16_t helper_id, uint8_t argument,
    PopControllerState *state, PopControllerQueryResult *result,
    const PopControllerIO *io, PopWorldEffectWrite write);
#endif
