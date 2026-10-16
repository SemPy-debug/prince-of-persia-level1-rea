#ifndef POP_RUNTIME_H
#define POP_RUNTIME_H

#include "world_query.h"
#include "world_effects.h"
#include "guard.h"
#include "world_mechanisms.h"

typedef struct PopRuntime {
    PopWorldState world;
    uint8_t sound_count;
    uint8_t sounds[32];
    uint8_t sound_scratch_x;
    uint16_t missing_helper;
    /* Optional read-only bytecode fixture input. Normal gameplay leaves both
     * fields NULL and reads the pinned original animation data. This is not
     * a machine-code executor or an alternative world implementation. */
    PopControllerRead animation_read;
    void *animation_context;
    PopWorldEffects effects;
    PopGuardMemory guards;
    PopWorldEffectMemory effect_memory;
    PopWorldMechanismMemory mechanisms;
} PopRuntime;

bool pop_runtime_reset(PopRuntime *runtime);
bool pop_runtime_read(void *runtime, uint16_t address, uint8_t *value);
/* Write only owned mutable records. This preserves the originalB6E0 alias
 * between the effect counter and the final sound-request slot. */
bool pop_runtime_write(void *runtime, uint16_t address, uint8_t value);
/* Source-named callback joining independently native components. Unsupported
 * effects preserve world, AUX, return conditions and sound requests; only the
 * missing_helper diagnostic changes. Sound requests are not audio waveforms. */
bool pop_runtime_query(void *runtime, uint16_t helper, uint8_t argument,
                       PopControllerState *state, PopControllerQueryResult *result);

#endif
