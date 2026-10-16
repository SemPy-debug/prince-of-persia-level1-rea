#include "world_query.h"
#include "data.h"
#include "geometry.h"
#include <stddef.h>

static void result_a(PopControllerQueryResult *result, uint8_t value) {
    result->a = value;
    result->zero = value == 0;
    result->negative = (value & 0x80u) != 0;
}

static bool neighbor(uint8_t *room, PopWorldDirection direction) {
    /* The original D150/D159/D162/D16B guard zero before indexing. */
    if (*room == 0) return true;
    return pop_world_initial_neighbor(*room, direction, room);
}

static bool query_cell(PopWorldState *world, uint8_t room, uint8_t column, uint8_t row,
                       PopControllerState *state, PopControllerQueryResult *result) {
    uint8_t low, high, row_offset, raw;
    unsigned type_pointer, spec_pointer;
    /* Natural4A47 collision scans also pass the255 room sentinel. Original
     * lookup arithmetic wraps its pointer to5500/57D0 and reads program bytes
     * as tile data. Admit only this measured, already-normalized30-cell case;
     * it is not a new playable room or an unrestricted memory fallback. */
    if (room > POP_WORLD_ROOM_COUNT && room != 255) return false;
    if (room == 255 && (column >= 10 || row >= 3)) return false;
    state->aux[0x91] = column;
    state->aux[0x92] = row;
    state->aux[0x93] = room;
    /* D0F3 resolves all horizontal transitions before the vertical ones. The
     * directed source map is not assumed rectangular or reciprocal. */
    while ((column & 0x80u) != 0 || column >= 10) {
        PopWorldDirection direction = (column & 0x80u) != 0 ? POP_WORLD_LEFT : POP_WORLD_RIGHT;
        column = (uint8_t)((unsigned)column + (direction == POP_WORLD_LEFT ? 10u : 246u));
        if (!neighbor(&room, direction)) return false;
    }
    while ((row & 0x80u) != 0 || row >= 3) {
        PopWorldDirection direction = (row & 0x80u) != 0 ? POP_WORLD_ABOVE : POP_WORLD_BELOW;
        row = (uint8_t)((unsigned)row + (direction == POP_WORLD_ABOVE ? 3u : 253u));
        if (!neighbor(&room, direction)) return false;
    }
    state->aux[0x91] = column;
    state->aux[0x92] = row;
    state->aux[0x93] = room;
    result->carry = false; /* Final normalized CMP row,#3. */
    if (room == 0) {
        result_a(result, 20);
        result->x = 20;
        return true; /* Pointers and caller Y are untouched. */
    }
    /* The original shifts into an eight-bit table index before the loads. */
    if (!pop_data_read(NULL, (uint16_t)(0x1220u + (uint8_t)(2u * (room - 1u))), &low) ||
        !pop_data_read(NULL, (uint16_t)(0x1221u + (uint8_t)(2u * (room - 1u))), &high) ||
        !pop_data_read(NULL, (uint16_t)(0x1200u + row), &row_offset)) return false;
    type_pointer = (0xb700u + low + 256u * high) & 0xffffu;
    spec_pointer = (type_pointer + 720u) & 0xffffu;
    state->aux[0x24] = (uint8_t)type_pointer;
    state->aux[0x25] = (uint8_t)(type_pointer >> 8);
    state->aux[0x26] = (uint8_t)spec_pointer;
    state->aux[0x27] = (uint8_t)(spec_pointer >> 8);
    result->y = (uint8_t)((unsigned)row_offset + column);
    if (!pop_world_read(world, (uint16_t)(type_pointer + result->y), &raw)) return false;
    result_a(result, raw & 0x1fu);
    result->x = result->a;
    return true;
}

static uint8_t foot_x(const PopControllerState *state, PopControllerQueryResult *result) {
    uint8_t delta = (uint8_t)(state->aux[0x83] - (state->aux[0x85] & 0x1fu));
    unsigned sum;
    if ((state->aux[0x43] & 0x80u) != 0) delta = (uint8_t)(0u - delta);
    sum = (unsigned)state->aux[0x41] + delta;
    result->carry = sum >= 256;
    result_a(result, (uint8_t)sum);
    return result->a;
}

static bool column_from_x(PopControllerState *state, PopControllerQueryResult *result,
                          uint8_t x, bool subtract_seven) {
    uint8_t remainder, column;
    if (subtract_seven) {
        result->carry = x >= 7;
        x = (uint8_t)(x - 7u);
    }
    if (!pop_data_read(NULL, (uint16_t)(0x1100u + x), &remainder) ||
        !pop_data_read(NULL, (uint16_t)(0x1000u + x), &column)) return false;
    state->aux[3] = remainder;
    result->y = x;
    result_a(result, column);
    return true;
}

static bool dispatch(PopWorldState *world, uint16_t helper, uint8_t argument,
                     PopControllerState *state, PopControllerQueryResult *result) {
    uint8_t column = state->aux[0x44], row = state->aux[0x45], room = state->aux[0x4b];
    uint8_t value;
    switch (helper) {
    case 0xd024:
        return query_cell(world, state->aux[0x93], state->aux[0x91], state->aux[0x92], state, result);
    case 0xd021:
        return query_cell(world, argument, result->x, result->y, state, result);
    case 0xd0ba:
        result->y = row;
        return query_cell(world, room, column, row, state, result);
    case 0xd0bd: case 0xd0c0: case 0xd0c6: case 0xd01e: {
        bool forward = helper == 0xd0bd || helper == 0xd0c6;
        if (state->aux[0x43] != 0 && state->aux[0x43] != 0xff) return false;
        value = (uint8_t)((unsigned)column + ((state->aux[0x43] == 0) == forward ? 1u : 255u));
        state->aux[forward ? 0x8e : 0x8f] = value;
        if (helper == 0xd0c6 || helper == 0xd01e) {
            row = (uint8_t)(row - 1u);
            state->aux[0x90] = row;
        }
        result->y = row;
        return query_cell(world, room, value, row, state, result);
    }
    case 0xd0c3:
        row = (uint8_t)(row - 1u);
        state->aux[0x90] = row;
        result->y = row;
        return query_cell(world, room, column, row, state, result);
    case 0xd093:
        /* D212: two facing-relative additions, without the single-step
         * wrappers'8E/8F scratch writes. The final D0D0 owns normalization. */
        if (state->aux[0x43] != 0 && state->aux[0x43] != 255) return false;
        column = (uint8_t)(column + (state->aux[0x43] == 0 ? 2u : 254u));
        result->y = row;
        return query_cell(world, room, column, row, state, result);
    case 0xd006:
        (void)foot_x(state, result);
        return true;
    case 0xd009: case 0xd00c:
        return column_from_x(state, result, argument, helper == 0xd00c);
    case 0xd018: case 0xd01b:
        if (helper == 0xd018) argument = foot_x(state, result);
        if (!column_from_x(state, result, argument, true)) return false;
        value = state->aux[3];
        if (state->aux[0x43] == 0) {
            result->carry = 13 >= value;
            value = (uint8_t)(13u - value);
        }
        result_a(result, value);
        return true;
    case 0xd012: {
        unsigned sum = (unsigned)argument + 5u;
        result->x = (uint8_t)sum;
        result->carry = sum >= 256;
        if (result->x >= 0x30) return false;
        if (!pop_data_read(NULL, (uint16_t)(0x1260u + result->x), &value)) return false;
        result_a(result, value);
        return true;
    }
    case 0xd00f:
        result->x = 3;
        for (;;) {
            if (!pop_data_read(NULL, (uint16_t)(0x1275u + result->x), &value)) return false;
            result->carry = argument >= value;
            if (result->carry) break;
            --result->x;
            if ((result->x & 0x80u) != 0) break;
        }
        result_a(result, result->x);
        return true;
    case 0xd04b:
        value = pop_geometry_d684(argument);
        result->carry = value == 0;
        result_a(result, value);
        return true;
    case 0xd04e:
        value = pop_geometry_d69e(argument);
        result->carry = value != 0 || argument >= 20;
        result_a(result, value);
        return true;
    case 0xd0ae: {
        uint8_t actor = state->aux[0x41], other = state->aux[0x39d];
        result->carry = state->aux[0x4b] >= state->aux[0x3a7];
        if (state->aux[0x4b] != state->aux[0x3a7]) {
            result->x = 127;
            result_a(result, 127);
            return true;
        }
        if (other >= actor) {
            value = (uint8_t)(other - actor);
            if ((value & 0x80u) != 0) value = 127;
            result->carry = true;
        } else {
            value = (uint8_t)(actor - other);
            if ((value & 0x80u) != 0) value = 127;
            result->carry = value == 0;
            value = (uint8_t)(0u - value);
        }
        if ((state->aux[0x43] & 0x80u) != 0) {
            result->carry = value == 0;
            value = (uint8_t)(0u - value);
        }
        if (((state->aux[0x43] ^ state->aux[0x39f]) & 0x80u) != 0) {
            result->carry = value >= 114;
            if (!result->carry) value = (uint8_t)(value + 13u);
        }
        result->x = value;
        result_a(result, value);
        return true;
    }
    default:
        return false;
    }
}

bool pop_world_query(void *world, uint16_t helper_id, uint8_t argument,
                     PopControllerState *state, PopControllerQueryResult *result) {
    PopControllerState staged_state;
    PopControllerQueryResult staged_result;
    if (world == NULL || state == NULL || result == NULL) return false;
    staged_state = *state;
    staged_result = *result;
    if (!dispatch(world, helper_id, argument, &staged_state, &staged_result)) return false;
    *state = staged_state;
    *result = staged_result;
    return true;
}
