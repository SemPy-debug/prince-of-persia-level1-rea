#include "sequence.h"

static uint16_t sequence_address(const PopActorState *actor)
{
    return (uint16_t)((uint16_t)actor->sequence_lo |
                      ((uint16_t)actor->sequence_hi << 8));
}

static void set_sequence_address(PopActorState *actor, uint16_t address)
{
    actor->sequence_lo = (uint8_t)address;
    actor->sequence_hi = (uint8_t)(address >> 8);
}

static bool resolve_effect(PopSequenceState *state,
                           PopSequenceApplyEffect apply_effect, void *context)
{
    PopSequenceEffect effect = (PopSequenceEffect)state->pending_effect;
    if (effect == POP_SEQUENCE_EFFECT_NONE) {
        return true;
    }
    if (apply_effect == NULL ||
        !apply_effect(context, effect, state->effect_argument, state)) {
        return false;
    }
    state->pending_effect = POP_SEQUENCE_EFFECT_NONE;
    if (effect == POP_SEQUENCE_EFFECT_D909) {
        /* Original 49FA calls the sound helper before 49FF sets this flag. */
        state->flag_e4 = 1;
    }
    return true;
}

PopSequenceResult pop_sequence_run(PopSequenceState *state, PopSequenceRead read,
                                  PopSequenceApplyEffect apply_effect,
                                  void *context, size_t command_limit)
{
    size_t commands = 0;
    if (state == NULL || read == NULL ||
        state->pending_effect > POP_SEQUENCE_EFFECT_F90F) {
        return POP_SEQUENCE_INVALID;
    }
    for (;;) {
        uint8_t opcode, operand[2] = {0, 0};
        unsigned operand_count = 0;
        uint16_t address;
        if (!resolve_effect(state, apply_effect, context)) {
            return POP_SEQUENCE_EFFECT_REQUIRED;
        }
        if (commands == command_limit) {
            return POP_SEQUENCE_LIMIT;
        }
        address = sequence_address(&state->actor);
        if (!read(context, address, &opcode)) {
            return POP_SEQUENCE_UNREADABLE;
        }
        address = (uint16_t)(address + 1u);
        switch (opcode) {
        case 0xfb: case 0xfa: case 0xf9: case 0xf2: case 0xf3:
            operand_count = 1;
            break;
        case 0xff: case 0xf8: case 0xf7:
            operand_count = 2;
            break;
        default:
            break;
        }
        for (unsigned i = 0; i < operand_count; ++i) {
            if (!read(context, address, &operand[i])) {
                return POP_SEQUENCE_UNREADABLE;
            }
            address = (uint16_t)(address + 1u);
        }
        set_sequence_address(&state->actor, address);
        ++commands;
        switch (opcode) {
        case 0xfb: {
            uint8_t delta = operand[0];
            if ((state->actor.facing & 0x80u) != 0) {
                delta = (uint8_t)(0u - (unsigned)delta);
            }
            state->actor.x = (uint8_t)((unsigned)state->actor.x + delta);
            break;
        }
        case 0xfa:
            state->actor.y = (uint8_t)((unsigned)state->actor.y + operand[0]);
            break;
        case 0xfe:
            state->actor.facing ^= 0xffu;
            break;
        case 0xff:
            state->actor.sequence_lo = operand[0];
            state->actor.sequence_hi = operand[1];
            break;
        case 0xfd:
            state->actor.block_y = (uint8_t)(state->actor.block_y - 1u);
            state->pending_effect = POP_SEQUENCE_EFFECT_E01E;
            state->effect_argument = 0;
            break;
        case 0xfc:
            state->actor.block_y = (uint8_t)(state->actor.block_y + 1u);
            state->pending_effect = POP_SEQUENCE_EFFECT_E01E;
            state->effect_argument = 0;
            break;
        case 0xf9:
            state->actor.action = operand[0];
            break;
        case 0xf8:
            state->actor.velocity_x = operand[0];
            state->actor.velocity_y = operand[1];
            break;
        case 0xf7:
            if (state->flag_c5 != 0) {
                state->actor.sequence_lo = operand[0];
                state->actor.sequence_hi = operand[1];
            }
            break;
        case 0xf6:
            break;
        case 0xf5:
            state->flag_cd = 1;
            break;
        case 0xf4:
            state->flag_cd = 0xff;
            break;
        case 0xf2:
            if (operand[0] == 0) {
                state->flag_e4 = 1;
            } else if (operand[0] == 1 || operand[0] == 2) {
                state->pending_effect = POP_SEQUENCE_EFFECT_D909;
                state->effect_argument = operand[0] == 1 ? 9 : 13;
            }
            break;
        case 0xf1:
            state->pending_effect = POP_SEQUENCE_EFFECT_4A2D;
            state->effect_argument = 0;
            break;
        case 0xf3:
            if (operand[0] == 1) {
                state->pending_effect = POP_SEQUENCE_EFFECT_F90F;
                state->effect_argument = 0;
            }
            break;
        default:
            state->actor.pose = opcode;
            return POP_SEQUENCE_POSE;
        }
    }
}
