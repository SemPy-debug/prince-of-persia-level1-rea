#include "runtime.h"
#include "geometry.h"
#include "sequence.h"
#include "world_collision.h"
#include "items.h"
#include <stddef.h>
#include <string.h>

typedef struct SequenceBridge {
    PopRuntime *runtime;
    PopControllerState *state;
    PopControllerQueryResult *result;
} SequenceBridge;

static bool dispatch(PopRuntime *runtime, uint16_t helper, uint8_t argument,
                      PopControllerState *state, PopControllerQueryResult *result);

bool pop_runtime_reset(PopRuntime *runtime) {
    if (runtime == NULL) return false;
    memset(runtime, 0, sizeof(*runtime));
    return pop_world_reset(&runtime->world) && pop_guard_memory_reset(&runtime->guards) &&
           pop_world_effect_memory_reset(&runtime->effect_memory) &&
           pop_world_mechanism_memory_reset(&runtime->mechanisms);
}

bool pop_runtime_read(void *context, uint16_t address, uint8_t *value) {
    PopRuntime *runtime = context;
    if (runtime == NULL || value == NULL) return false;
    if (address >= 0xb600 && address < 0xb660) {
        const uint8_t *bytes = (const uint8_t *)&runtime->effects;
        *value = bytes[address - 0xb600u];
        return true;
    }
    if (address == 0xb6c0) {
        *value = runtime->sound_count;
        return true;
    }
    if (address > 0xb6c0 && address <= 0xb6e0) {
        *value = runtime->sounds[address - 0xb6c1u];
        return true;
    }
    if (pop_guard_memory_read(&runtime->guards, address, value)) return true;
    if (pop_world_effect_memory_read(&runtime->effect_memory, address, value)) return true;
    if (pop_world_mechanism_memory_read(&runtime->mechanisms, address, value)) return true;
    return pop_world_read(&runtime->world, address, value);
}

bool pop_runtime_write(void *context, uint16_t address, uint8_t value) {
    PopRuntime *runtime = context;
    if (runtime == NULL) return false;
    if (address >= 0xb600 && address < 0xb660) {
        uint8_t *bytes = (uint8_t *)&runtime->effects;
        bytes[address - 0xb600u] = value;
        return true;
    }
    if (address == 0xb6c0) {
        runtime->sound_count = value;
        return true;
    }
    if (address > 0xb6c0 && address <= 0xb6e0) {
        runtime->sounds[address - 0xb6c1u] = value;
        return true;
    }
    if (pop_guard_memory_write(&runtime->guards, address, value)) return true;
    if (pop_world_effect_memory_write(&runtime->effect_memory, address, value)) return true;
    if (pop_world_mechanism_memory_write(&runtime->mechanisms, address, value)) return true;
    return pop_world_write(&runtime->world, address, value);
}

static void sequence_to_aux(const PopSequenceState *sequence, PopControllerState *state) {
    memcpy(state->aux + 0x40, &sequence->actor, sizeof(sequence->actor));
    state->aux[0xc5] = sequence->flag_c5;
    state->aux[0xcd] = sequence->flag_cd;
    state->aux[0xe4] = sequence->flag_e4;
}

static void aux_to_sequence(const PopControllerState *state, PopSequenceState *sequence) {
    memcpy(&sequence->actor, state->aux + 0x40, sizeof(sequence->actor));
    sequence->flag_c5 = state->aux[0xc5];
    sequence->flag_cd = state->aux[0xcd];
    sequence->flag_e4 = state->aux[0xe4];
}

static bool sequence_read(void *context, uint16_t address, uint8_t *value) {
    SequenceBridge *bridge = context;
    if (bridge->runtime->animation_read != NULL) {
        return bridge->runtime->animation_read(bridge->runtime->animation_context, address, value);
    }
    return pop_runtime_read(bridge->runtime, address, value);
}

static bool sequence_effect(void *context, PopSequenceEffect effect, uint8_t argument,
                             PopSequenceState *sequence) {
    SequenceBridge *bridge = context;
    uint16_t helper;
    switch (effect) {
    case POP_SEQUENCE_EFFECT_E01E: helper = 0xe01e; break;
    case POP_SEQUENCE_EFFECT_D909: helper = 0xd909; break;
    case POP_SEQUENCE_EFFECT_4A2D: helper = 0x4a2d; break;
    case POP_SEQUENCE_EFFECT_F90F: helper = 0xf90f; break;
    default: return false;
    }
    sequence_to_aux(sequence, bridge->state);
    if (!dispatch(bridge->runtime, helper, argument, bridge->state, bridge->result)) return false;
    aux_to_sequence(bridge->state, sequence);
    return true;
}

static void set_value(PopControllerQueryResult *result, uint8_t value) {
    result->a = value;
    result->zero = value == 0;
    result->negative = (value & 0x80u) != 0;
}

static bool dispatch(PopRuntime *runtime, uint16_t helper, uint8_t argument,
                      PopControllerState *state, PopControllerQueryResult *result) {
    switch (helper) {
    case 0x25f0: {
        uint8_t sum;
        /* Original health applies byte-sized deltas and preserves an old
         * total when the wrapped result exceeds its cap; it does not clamp. */
        if (state->aux[0x3f4] == 12 &&
            (uint8_t)(state->aux[0x3a9] | state->aux[0x4d]) == 1) {
            if ((state->aux[0xcf] & 128u) != 0) state->aux[0xd2] = state->aux[0xcf];
            else if ((state->aux[0xd2] & 128u) != 0) state->aux[0xcf] = state->aux[0xd2];
        }
        sum = (uint8_t)(state->aux[0xce] + state->aux[0xcf]);
        if (sum <= state->aux[0xd0]) state->aux[0xce] = sum;
        sum = (uint8_t)(state->aux[0xcc] + state->aux[0xd2]);
        if (sum <= state->aux[0xdc]) state->aux[0xcc] = sum;
        result->a = sum;
        result->carry = sum >= state->aux[0xdc];
        result->zero = sum == state->aux[0xdc];
        result->negative = ((uint8_t)(sum - state->aux[0xdc]) & 128u) != 0;
        return true;
    }
    case 0x26c6:
        if (state->aux[0x50] == 167) return dispatch(runtime, 0xd909, 17, state, result);
        if (state->aux[0x60] == 167) return dispatch(runtime, 0xd909, 18, state, result);
        result->a = 167;
        result->carry = 167 >= state->aux[0x60];
        result->zero = 167 == state->aux[0x60];
        result->negative = ((uint8_t)(167 - state->aux[0x60]) & 128u) != 0;
        return true;
    case 0xf918:
        result->a = state->aux[0x3f4];
        result->carry = result->a >= 12;
        result->zero = result->a == 12;
        result->negative = ((uint8_t)(result->a - 12u) & 128u) != 0;
        if (result->a != 12) return true; /* FB29's actual first-level return. */
        result->a = state->aux[0x3a9] | state->aux[0x4d];
        result->carry = result->a >= 1;
        result->zero = result->a == 1;
        result->negative = ((uint8_t)(result->a - 1u) & 128u) != 0;
        if (result->a != 1) return true;
        set_value(result,state->aux[0x4f]);
        if (!result->negative) return true;
        set_value(result,state->aux[0x3ab]);
        if (result->negative) return true;
        state->aux[0x89] = 255;
        state->aux[0x88] = 5;
        if (!dispatch(runtime,0xd909,5,state,result)) return false;
        set_value(result,100);
        return dispatch(runtime,0xf927,100,state,result);
    case 0xd066: {
        uint8_t parity;
        if (!pop_geometry_project_actor(state->aux)) break;
        result->x = state->aux[0x4d];
        result->y = 4;
        parity = state->aux[0x85] ^ state->aux[0x74];
        if ((parity & 128u) != 0) {
            result->carry = true; /* Original SEC/SBC#0 before finalEOR. */
            set_value(result, parity);
        } else {
            result->carry = state->aux[0x71] == 0;
            set_value(result, state->aux[0x71]);
            if (result->carry) {
                /* INC72 changesNZ, while A remains the wrapped pixel byte. */
                result->zero = state->aux[0x72] == 0;
                result->negative = (state->aux[0x72] & 128u) != 0;
            }
        }
        return true;
    }
    case 0xd033:
        if (!pop_geometry_bounds(state->aux)) break;
        result->x = state->aux[0xbd];
        result->y = state->aux[0xb8];
        result->carry = state->aux[0xb8] >= state->aux[0xe9];
        set_value(result, state->aux[0xbf]);
        return true;
    case 0x25c6: {
        uint8_t selector = state->aux[0xcd];
        set_value(result, selector);
        if (selector == 0) return true;
        state->aux[0xcd] = 0;
        argument = state->aux[0x45];
        if ((selector & 0x80u) == 0) {
            result->carry = argument >= 1;
            --argument;
        }
        set_value(result, argument);
        return dispatch(runtime, 0xee1b, argument, state, result);
    }
    case 0x3a09:
        if (!pop_geometry_load_frame(state->aux)) break;
        result->x = state->aux[0x4d]; /* D724 selects by character ID. */
        if (!pop_world_query(&runtime->world, 0xd006, argument, state, result) ||
            !pop_world_query(&runtime->world, 0xd00c, result->a, state, result)) break;
        state->aux[0x44] = result->a;
        return true;
    case 0x450c: {
        PopSequenceState sequence = {0};
        SequenceBridge bridge = {runtime, state, result};
        PopSequenceResult outcome;
        aux_to_sequence(state, &sequence);
        outcome = pop_sequence_run(&sequence, sequence_read, sequence_effect, &bridge, 4096);
        if (outcome != POP_SEQUENCE_POSE) {
            if (runtime->missing_helper == 0) runtime->missing_helper = outcome == POP_SEQUENCE_UNREADABLE ? 0xd003 : 0x4945;
            return false;
        }
        sequence_to_aux(&sequence, state);
        result->a = sequence.actor.pose;
        result->y = 0; /* The final D003 fetch sets Y=0. */
        result->carry = result->a >= 0xf3;
        result->zero = result->a == 0xf3;
        result->negative = ((uint8_t)(result->a - 0xf3u) & 0x80u) != 0;
        return true;
    }
    case 0xd909:
        runtime->sound_scratch_x = result->x;
        result->carry = runtime->sound_count >= 32;
        if (!result->carry) runtime->sounds[runtime->sound_count++] = argument;
        result->a = argument;
        result->zero = result->x == 0;
        result->negative = (result->x & 0x80u) != 0;
        return true;
    case 0xd930:
        state->aux[0x319] = argument;
        state->aux[0x308] = result->x;
        result->a = argument; /* Stores preserve the incoming conditions. */
        return true;
    case 0x4a2d: {
        uint8_t level = state->aux[0x3f4];
        result->carry = level >= 13;
        if (level == 13) result->a = level;
        else {
            result->carry = level >= 4;
            result->a = level == 4 ? 6 : 9;
            result->x = 25;
            state->aux[0x319] = result->a;
            state->aux[0x308] = result->x;
        }
        ++state->aux[0x31c];
        result->zero = state->aux[0x31c] == 0;
        result->negative = (state->aux[0x31c] & 0x80u) != 0;
        return true;
    }
    case 0xf90f:
    case 0xe030:
    case 0xd0b7: {
        const PopControllerIO io = {pop_runtime_query,pop_runtime_read,runtime};
        if (pop_item_query(helper,argument,state,result,&io,pop_runtime_write)) return true;
        if (state->required_helper) {
            runtime->missing_helper=state->required_helper;return false;
        }
        break;
    }
    default: {
        const PopControllerIO io = {pop_runtime_query, pop_runtime_read, runtime};
        if (pop_world_query(&runtime->world, helper, argument, state, result)) return true;
        if (pop_world_collision_query_io(&runtime->world, helper, argument, state, result, &io)) return true;
        if (pop_world_effect_query_io(&runtime->world, &runtime->effects, &runtime->effect_memory,
                                     helper, argument, state, result, &io, pop_runtime_write)) return true;
        if (pop_world_mechanism_query_io(&runtime->world, &runtime->effects, &runtime->effect_memory,
            &runtime->mechanisms, helper, argument, state, result, &io, pop_runtime_write)) return true;
        break;
    }
    }
    runtime->missing_helper = helper;
    return false;
}

bool pop_runtime_query(void *context, uint16_t helper, uint8_t argument,
                       PopControllerState *state, PopControllerQueryResult *result) {
    PopRuntime *runtime = context;
    PopRuntime staged;
    PopControllerState working;
    PopControllerQueryResult returned;
    if (runtime == NULL || state == NULL || result == NULL) return false;
    staged = *runtime;
    working = *state;
    returned = *result;
    staged.missing_helper = 0;
    if (!dispatch(&staged, helper, argument, &working, &returned)) {
        runtime->missing_helper = staged.missing_helper;
        return false;
    }
    *runtime = staged;
    *state = working;
    *result = returned;
    return true;
}
