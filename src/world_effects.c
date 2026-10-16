#include "world_effects.h"
#include "data.h"
#include "world_query.h"
#include <stddef.h>
#include <string.h>

bool pop_world_effect_memory_reset(PopWorldEffectMemory *memory) {
    PopWorldEffectMemory staged = {0};
    if (memory == NULL) return false;
    for (unsigned i = 0; i < 256; ++i)
        if (!pop_world_initial_byte(0x6a0u + i, &staged.link_map[i])) return false;
    *memory = staged;
    return true;
}
static uint8_t *effect_address(PopWorldEffectMemory *m, uint16_t address) {
    if (m == NULL) return NULL;
    if (address >= 0xbda0 && address < 0xbea0) return m->link_map + (address - 0xbda0);
    if (address >= 0x5e5a && address < 0x5e78) return m->full_tile + (address - 0x5e5a);
    if (address >= 0x5eb4 && address < 0x5ed2) return m->wipe_repeat + (address - 0x5eb4);
    if (address >= 0x5f0e && address < 0x5f2c) return m->wipe_height + (address - 0x5f0e);
    if (address >= 0x5f2c && address < 0x5f36) return m->top + (address - 0x5f2c);
    return NULL;
}
bool pop_world_effect_memory_read(void *memory, uint16_t address, uint8_t *value) {
    uint8_t *source = effect_address(memory, address);
    if (source == NULL || value == NULL) return false;
    *value = *source;
    return true;
}
bool pop_world_effect_memory_write(void *memory, uint16_t address, uint8_t value) {
    uint8_t *destination = effect_address(memory, address);
    if (destination == NULL) return false;
    *destination = value;
    return true;
}

static void value_a(PopControllerQueryResult *r, uint8_t value) {
    r->a = value;
    r->zero = value == 0;
    r->negative = (value & 0x80u) != 0;
}
static void compare(PopControllerQueryResult *r, uint8_t left, uint8_t right) {
    r->carry = left >= right;
    r->zero = left == right;
    r->negative = ((uint8_t)(left - right) & 0x80u) != 0;
}

/* AUX04CC/0C30, without the extra cell-read/scratch writes of D021. */
static bool room_pointers(uint8_t room, PopControllerState *s, PopControllerQueryResult *r) {
    uint8_t low, high;
    unsigned type, spec;
    if (room > POP_WORLD_ROOM_COUNT) return false;
    if (room == 0) {
        s->aux[0x24] = 0x0f; s->aux[0x25] = 0x96;
        s->aux[0x26] = 0x2d; s->aux[0x27] = 0x96;
        r->carry = true;
        value_a(r, 0x96);
        return true;
    }
    r->x = (uint8_t)(2u * (room - 1u));
    if (!pop_data_read(NULL, (uint16_t)(0x1220u + r->x), &low) ||
        !pop_data_read(NULL, (uint16_t)(0x1221u + r->x), &high)) return false;
    type = 0xb700u + low + 256u * high;
    spec = type + 720u;
    s->aux[0x24] = (uint8_t)type; s->aux[0x25] = (uint8_t)(type >> 8);
    s->aux[0x26] = (uint8_t)spec; s->aux[0x27] = (uint8_t)(spec >> 8);
    r->carry = false;
    value_a(r, (uint8_t)(spec >> 8));
    return true;
}

/* E2 EE45/EE5D: find the last matching cell+room, update phase, or append.
 * Slot0 is the source routine's scratch record. Full queues keep that scratch
 * and earlier world writes while refusing only the additional record. */
static void queue_record(PopWorldEffects *e, PopControllerState *s,
                         PopControllerQueryResult *r) {
    r->x = s->aux[0x95];
    while (r->x != 0) {
        value_a(r, e->cell[r->x]);
        compare(r, r->a, e->cell[0]);
        if (r->zero) {
            value_a(r, e->room[r->x]);
            compare(r, r->a, e->room[0]);
            if (r->zero) break;
        }
        --r->x;
    }
    compare(r, r->x, 0);
    if (r->x != 0) {
        value_a(r, e->phase[0]);
        e->phase[r->x] = r->a;
        return;
    }
    r->x = s->aux[0x95];
    compare(r, r->x, 31);
    if (r->zero) return;
    ++r->x;
    s->aux[0x95] = r->x;
    value_a(r, e->phase[0]); e->phase[r->x] = r->a;
    value_a(r, e->cell[0]); e->cell[r->x] = r->a;
    value_a(r, e->room[0]); e->room[r->x] = r->a;
}

/* E2 EE1E -> EEE0. Numeric fields deliberately retain source identities. */
static bool set_phase(PopWorldEffects *e, PopWorldState *world, uint8_t argument,
                      PopControllerState *s, PopControllerQueryResult *r) {
    uint8_t spec;
    uint16_t address = (uint16_t)(s->aux[0x26] + 256u * s->aux[0x27] + r->y);
    s->aux[0xe8] = argument;
    if (!pop_world_read(world, address, &spec)) return false;
    value_a(r, spec);
    if (spec != 0) {
        compare(r, spec, 6);
        if (!r->carry) return true;
    }
    e->cell[0] = r->y;
    value_a(r, s->aux[0xe8]);
    if (!pop_world_write(world, address, r->a)) return false;
    value_a(r, s->aux[0xcb]); e->room[0] = r->a;
    value_a(r, 1); e->phase[0] = r->a;
    queue_record(e, s, r);
    return true;
}

/* E2 E01E/E0F4. E12D's self-modified loop bound is a local native value. */
static bool scan_row(PopWorldEffects *e, PopWorldState *world,
                     PopControllerState *s, PopControllerQueryResult *r) {
    uint8_t first, end, raw, spec;
    uint16_t type_pointer, spec_pointer;
    s->aux[0xf2] = 15;
    if (!room_pointers(s->aux[0x4b], s, r)) return false;
    r->y = s->aux[0x45];
    compare(r, r->y, 3);
    if (r->carry) return true;
    if (!pop_data_read(NULL, (uint16_t)(0x1200u + r->y), &first)) return false;
    r->y = first;
    end = (uint8_t)(first + 10u);
    type_pointer = (uint16_t)(s->aux[0x24] + 256u * s->aux[0x25]);
    spec_pointer = (uint16_t)(s->aux[0x26] + 256u * s->aux[0x27]);
    do {
        if (!pop_world_read(world, (uint16_t)(type_pointer + r->y), &raw)) return false;
        value_a(r, raw & 31u);
        if (r->a == 18) {
            if (!pop_world_read(world, (uint16_t)(spec_pointer + r->y), &spec)) return false;
            r->x = spec;
            value_a(r, spec & 127u);
            if (r->a == 0 || r->a >= 6) {
                value_a(r, (uint8_t)((spec & 128u) | s->aux[0xf2]));
                if (!set_phase(e, world, r->a, s, r)) return false;
                raw = (uint8_t)(s->aux[0xf2] - 3u);
                if (raw < 6) raw = (uint8_t)(raw + 10u);
                s->aux[0xf2] = raw;
                value_a(r, raw);
            }
        }
        ++r->y;
        compare(r, r->y, end);
    } while (!r->carry);
    return true;
}

/* E2 EE1B/F826/F849: scan source row for type11 and enqueue zero-state cells.
 * D024 is an independently reconstructed native query, including its room/row
 * normalization and scratch contract. Descending column order is significant. */
static bool scan_type11_cells(PopWorldEffects *e, PopWorldState *world,
                              PopControllerState *s, PopControllerQueryResult *r) {
    int column;
    uint8_t spec;
    uint16_t address;
    for (column = 9; column >= 0; --column) {
        s->aux[0x91] = (uint8_t)column;
        if (!pop_world_query(world, 0xd024, r->a, s, r)) return false;
        compare(r, r->a, 11);
        if (r->zero) {
            address = (uint16_t)(s->aux[0x26] + 256u * s->aux[0x27] + r->y);
            if (!pop_world_read(world, address, &spec)) return false;
            value_a(r, spec);
            if (spec == 0) {
                value_a(r, 128);
                if (!pop_world_write(world, address, r->a)) return false;
                e->cell[0] = r->y;
                value_a(r, s->aux[0x93]); e->room[0] = r->a;
                value_a(r, 1); e->phase[0] = r->a;
                queue_record(e, s, r);
            }
        }
        value_a(r, (uint8_t)column);
        r->x = (uint8_t)(column - 1);
        r->zero = r->x == 0;
        r->negative = (r->x & 128u) != 0;
    }
    return true;
}

static bool scan_type11(PopWorldEffects *e, PopWorldState *world, uint8_t row,
                        PopControllerState *s, PopControllerQueryResult *r) {
    value_a(r, row); r->x = s->aux[0x3f4]; compare(r, r->x, 13);
    if (r->zero) return true;
    s->aux[0x92] = row; s->aux[0x93] = s->aux[0xcb];
    return scan_type11_cells(e, world, s, r);
}

/* E2 EE18/EF85 returns the source predicate's numeric0/1/2, with the
 * carry retained or replaced by the actual comparison on each branch. */
static bool trap_state(PopWorldState *world, PopControllerState *s,
                        PopControllerQueryResult *r) {
    uint8_t spec;
    uint16_t address = (uint16_t)(s->aux[0x26] + 256u * s->aux[0x27] + r->y);
    if (!pop_world_read(world, address, &spec)) return false;
    value_a(r, spec);
    if ((spec & 128u) != 0) {
        compare(r, spec, 255);
        value_a(r, spec == 255 ? 0 : 1);
    } else if (spec != 0) {
        compare(r, spec, 5);
        value_a(r, spec < 5 ? 2 : 0);
    }
    return true;
}

/* E2 F927/FC61 writes a negative damage request, not the current health.
 * Actor0 uses CE/CF, other IDs CC/D2. Later original consumers apply it. */
static void damage_request(uint8_t argument, PopControllerState *s,
                            PopControllerQueryResult *r) {
    uint8_t health, amount;
    unsigned destination;
    r->x = s->aux[0x4d];
    health = s->aux[r->x == 0 ? 0xce : 0xcc];
    destination = r->x == 0 ? 0xcf : 0xd2;
    compare(r, argument, health);
    if (r->carry) {
        amount = (uint8_t)(0u - health);
        r->carry = health == 0;
        s->aux[destination] = amount;
        value_a(r, 0);
    } else {
        amount = (uint8_t)(0u - argument);
        r->carry = argument == 0;
        s->aux[destination] = amount;
        value_a(r, amount);
    }
}

/* E2 EE21/EF41, EE2D/EF15 and EE33/EF2C. Shared source9E RNG is
 * deterministic byte arithmetic; the two CLCs discard multiplication carry. */
static bool room_record(PopWorldEffects *e, PopWorldState *world, uint16_t helper,
                        uint8_t room, PopControllerState *s, PopControllerQueryResult *r) {
    uint8_t old, mask = helper == 0xee21 ? 15 : helper == 0xee2d ? 7 : 31;
    unsigned sum = (uint8_t)(5u * s->aux[0x9e]) + 23u;
    uint16_t address = (uint16_t)(s->aux[0x26] + 256u * s->aux[0x27] + r->y);
    e->cell[0] = r->y; e->room[0] = room; e->phase[0] = 1;
    s->aux[0x9e] = (uint8_t)sum; r->carry = sum > 255u;
    value_a(r, s->aux[0x9e] & mask);
    if (helper == 0xee2d) {
        if (!pop_world_read(world, address, &old)) return false;
        value_a(r, r->a | old);
    }
    if (!pop_world_write(world, address, r->a)) return false;
    queue_record(e, s, r);
    return true;
}

static bool room_records(PopWorldEffects *e, PopWorldState *world,
                         PopControllerState *s, PopControllerQueryResult *r) {
    uint8_t raw;
    if (!room_pointers(s->aux[0xcb], s, r)) return false;
    for (int cell = 29; cell >= 0; --cell) {
        uint16_t helper = 0;
        r->y = (uint8_t)cell;
        if (!pop_world_read(world, (uint16_t)(s->aux[0x24] +
                256u * s->aux[0x25] + r->y), &raw)) return false;
        value_a(r, raw & 31u); compare(r, r->a, 19);
        if (r->zero) helper = 0xee21;
        else {
            compare(r, r->a, 10);
            if (r->zero) helper = 0xee2d;
            else { compare(r, r->a, 22); if (r->zero) helper = 0xee33; }
        }
        if (helper != 0) {
            if (!room_record(e, world, helper, s->aux[0xcb], s, r)) return false;
            value_a(r, (uint8_t)cell); r->y = (uint8_t)cell;
        }
        --r->y; r->zero = r->y == 0; r->negative = (r->y & 128u) != 0;
    }
    return true;
}

bool pop_world_effect_query(PopWorldEffects *effects, PopWorldState *world,
    uint16_t helper_id, uint8_t argument, PopControllerState *state,
    PopControllerQueryResult *result) {
    PopWorldEffects staged_effects;
    PopWorldState staged_world;
    PopControllerState staged_state;
    PopControllerQueryResult staged_result;
    bool ok;
    if (effects == NULL || world == NULL || state == NULL || result == NULL ||
        state->aux[0x95] > 31) return false;
    staged_effects = *effects; staged_world = *world;
    staged_state = *state; staged_result = *result;
    if (helper_id == 0xe000)
        ok = room_records(&staged_effects, &staged_world, &staged_state, &staged_result);
    else if (helper_id == 0xee21 || helper_id == 0xee2d || helper_id == 0xee33)
        ok = room_record(&staged_effects, &staged_world, helper_id, argument,
                         &staged_state, &staged_result);
    else if (helper_id == 0xe01e)
        ok = scan_row(&staged_effects, &staged_world, &staged_state, &staged_result);
    else if (helper_id == 0xee1e)
        ok = set_phase(&staged_effects, &staged_world, argument, &staged_state, &staged_result);
    else if (helper_id == 0xee1b)
        ok = scan_type11(&staged_effects, &staged_world, argument, &staged_state, &staged_result);
    else if (helper_id == 0xf833)
        ok = scan_type11_cells(&staged_effects, &staged_world, &staged_state, &staged_result);
    else if (helper_id == 0xee18)
        ok = trap_state(&staged_world, &staged_state, &staged_result);
    else if (helper_id == 0xee27) {
        uint16_t address = (uint16_t)(staged_state.aux[0x26] +
                           256u * staged_state.aux[0x27] + staged_result.y);
        value_a(&staged_result, 86);
        ok = pop_world_write(&staged_world, address, staged_result.a);
    } else if (helper_id == 0xf927) {
        damage_request(argument, &staged_state, &staged_result);
        ok = true;
    }
    else return false;
    if (!ok) return false;
    *effects = staged_effects; *world = staged_world;
    *state = staged_state; *result = staged_result;
    return true;
}

/* E2 F450/F45D/F49A choose the two affected redraw positions. A carry
 * marks the ten-cell top buffer; normal rows use the thirty-cell arrays. */
static void dirty_position(const PopWorldEffects *e, PopControllerState *s,
                           PopControllerQueryResult *r, bool adjacent) {
    value_a(r, e->room[0]);
    compare(r, r->a, s->aux[0xcb]);
    if (r->zero) {
        r->y = e->cell[0];
        if (!adjacent) { compare(r, r->y, 30); return; }
        compare(r, r->y, 9);
        if (!r->zero) compare(r, r->y, 19);
        if (!r->zero) compare(r, r->y, 29);
        if (!r->zero) {
            ++r->y; r->zero = r->y == 0; r->negative = (r->y & 128u) != 0;
            r->carry = false; return;
        }
    } else if (adjacent) {
        compare(r, r->a, s->aux[0x31]);
        if (r->zero) {
            r->y = e->cell[0];
            compare(r, r->y, 9);
            if (!r->zero) compare(r, r->y, 19);
            if (!r->zero) compare(r, r->y, 29);
            if (r->zero) {
                value_a(r, (uint8_t)(r->y - 9u));
                r->y = r->a; r->carry = false; return;
            }
        } else {
            compare(r, r->a, s->aux[0x33]);
            if (r->zero) {
                value_a(r, (uint8_t)(e->cell[0] - 20u));
                r->y = r->a; r->carry = true; return;
            }
            return; /* F450 BNEF44F: bareRTS retains Y and comparison carry. */
        }
    } else {
        compare(r, r->a, s->aux[0x33]);
        if (r->zero) {
            value_a(r, (uint8_t)(e->cell[0] - 20u));
            r->y = r->a; r->carry = true; return;
        }
        return; /* Same F44F return, without the F44C LDY30. */
    }
    r->y = 30; r->zero = false; r->negative = false; r->carry = true;
}

/* D4 D75D/D769: the original top/normal dirty writes and maximum-height
 * retention. Invalid normal indices have no admitted native storage. */
static bool dirty_cell(PopWorldEffectMemory *m, PopControllerState *s,
                       PopControllerQueryResult *r) {
    value_a(r, 2);
    if (r->carry) {
        compare(r, r->y, 10);
        if (!r->carry) { m->top[r->y] = r->a; r->carry = true; }
        return true;
    }
    if (r->y >= 30) return false;
    m->full_tile[r->y] = r->a;
    if (m->wipe_repeat[r->y] == 0 || s->aux[4] >= m->wipe_height[r->y])
        m->wipe_height[r->y] = s->aux[4];
    m->wipe_repeat[r->y] = r->a;
    value_a(r, 2); r->carry = false;
    return true;
}

static bool dirty_pair(PopWorldEffects *e, PopWorldEffectMemory *m,
                       PopControllerState *s, PopControllerQueryResult *r,
                       const PopControllerIO *io, PopWorldEffectWrite write,
                       bool increment, uint8_t height) {
    uint8_t value;
    if (increment) {
        if (io == NULL || io->read == NULL || write == NULL ||
            !io->read(io->context, 0xb6e0, &value) ||
            !write(io->context, 0xb6e0, (uint8_t)(value + 1u))) return false;
    }
    s->aux[4] = height;
    dirty_position(e, s, r, false);
    if (!dirty_cell(m, s, r)) return false;
    dirty_position(e, s, r, true);
    return dirty_cell(m, s, r);
}

static bool call_effect(const PopControllerIO *io, uint16_t id, uint8_t argument,
                        PopControllerState *s, PopControllerQueryResult *r) {
    if (io == NULL || io->query == NULL) return false;
    value_a(r, argument);
    return io->query(io->context, id, argument, s, r);
}

/* EE03/EE2A: original spec mutation, queue update, shared redraw producer,
 * then actual native D909 request. This routine operates on caller-staged
 * runtime aliases so nested callbacks see the same updated objects. */
static bool spike_effect(PopWorldState *world, PopWorldEffects *e,
    PopWorldEffectMemory *m, bool forced, PopControllerState *s,
    PopControllerQueryResult *r, const PopControllerIO *io,
    PopWorldEffectWrite write) {
    uint8_t spec;
    uint16_t address = (uint16_t)(s->aux[0x26] + 256u * s->aux[0x27] + r->y);
    if (!forced) {
        if (!pop_world_read(world, address, &spec)) return false;
        value_a(r, spec);
        if (spec != 0) {
            if ((spec & 128u) == 0) return true;
            compare(r, spec, 255);
            if (r->zero) return true;
            value_a(r, 143);
            return pop_world_write(world, address, r->a);
        }
    } else {
        value_a(r, 255);
        if (!pop_world_write(world, address, r->a)) return false;
    }
    r->x = forced ? 255 : 1;
    e->phase[0] = r->x; e->cell[0] = r->y;
    value_a(r, s->aux[0x93]); e->room[0] = r->a;
    queue_record(e, s, r);
    if (!dirty_pair(e, m, s, r, io, write, true, 31)) return false;
    return call_effect(io, 0xd909, 2, s, r);
}

/* AUX445C, reached through3A0F. The row/column placement and sequence51
 * are source numeric contracts; damage remains a queued negative request. */
static bool falling_effect(PopWorldState *world, PopWorldEffects *e,
    PopWorldEffectMemory *m, PopControllerState *s, PopControllerQueryResult *r,
    const PopControllerIO *io, PopWorldEffectWrite write) {
    uint8_t value;
    if (!spike_effect(world, e, m, true, s, r, io, write)) return false;
    r->x = (uint8_t)(s->aux[0x45] + 1u);
    if (!pop_data_read(NULL, (uint16_t)(0x127eu + r->x), &value)) return false;
    s->aux[0x42] = value;
    if (!call_effect(io, 0xd012, s->aux[0x91], s, r)) return false;
    r->carry = false;
    { unsigned sum = r->a + 10u; r->carry = sum > 255u; value_a(r, (uint8_t)sum); }
    s->aux[0x41] = r->a;
    value_a(r, 8);
    /* D015/D1BC adds or subtracts the positive argument by facing. */
    if (s->aux[0x43] & 128u) {
        r->carry = s->aux[0x41] >= r->a;
        value_a(r, (uint8_t)(s->aux[0x41] - r->a));
    } else {
        unsigned sum = s->aux[0x41] + r->a;
        r->carry = sum > 255u; value_a(r, (uint8_t)sum);
    }
    s->aux[0x41] = r->a;
    s->aux[0x48] = 0;
    if (!call_effect(io, 0xd909, 14, s, r)) return false;
    value_a(r, 100); damage_request(100, s, r);
    pop_controller_select_sequence(s, 51);
    r->x = 100; /* D2EF byte index for sequence51. */
    value_a(r, s->aux[0x4a]);
    return call_effect(io, 0x450c, r->a, s, r);
}

static bool loose_effect(PopWorldState *world, PopWorldEffects *e,
    PopWorldEffectMemory *m, PopControllerState *s, PopControllerQueryResult *r,
    const PopControllerIO *io, PopWorldEffectWrite write) {
    uint8_t value;
    uint16_t type = (uint16_t)(s->aux[0x24] + 256u * s->aux[0x25] + r->y);
    uint16_t spec = (uint16_t)(s->aux[0x26] + 256u * s->aux[0x27] + r->y);
    s->aux[0xe8] = 1;
    if (!pop_world_read(world, type, &value)) return false;
    value_a(r, value & 32u);
    if (r->a != 0) return true;
    if (!pop_world_read(world, spec, &value)) return false;
    value_a(r, value);
    if (value != 0 && (value & 128u) == 0) return true;
    value_a(r, s->aux[0xe8]);
    if (!pop_world_write(world, spec, r->a)) return false;
    e->cell[0] = r->y;
    value_a(r, s->aux[0x93]); e->room[0] = r->a;
    value_a(r, 0); e->phase[0] = r->a;
    queue_record(e, s, r);
    return dirty_pair(e, m, s, r, io, write, true, 31);
}

/* E2 F057/F066/F074: update the target record, retaining numeric types and
 * trigger identities. Unrecognized target types deliberately retain scratch
 * phase0, exactly as the original source does. */
static bool link_target(PopWorldState *world, PopWorldEffects *e,
    PopControllerState *s, PopControllerQueryResult *r) {
    uint8_t value;
    uint16_t spec = (uint16_t)(s->aux[0x26] + 256u * s->aux[0x27] + r->y);
    compare(r, r->a, 4);
    if (!r->zero) {
        compare(r, r->a, 16);
        if (!r->zero) return true;
        if (!pop_world_read(world, spec, &value)) return false;
        value_a(r, value == 0 ? 1 : 255);
        e->phase[0] = r->a;
        return true;
    }
    if (!pop_world_read(world, spec, &value)) return false;
    value_a(r, value); r->x = s->aux[0xec];
    compare(r, r->x, 15);
    if (r->zero) {
        r->x = 1; e->phase[0] = r->x;
        compare(r, r->a, 255);
        if (r->zero) { value_a(r, 255); e->phase[0] = r->a; return true; }
        compare(r, r->a, 188);
        if (!r->carry) return true;
        value_a(r, 238);
    } else {
        compare(r, r->x, 14);
        if (r->zero) {
            r->x = 2; e->phase[0] = r->x;
            compare(r, r->a, 188);
            if (!r->carry) return true;
            value_a(r, 255);
        } else {
            compare(r, r->a, 0);
            value_a(r, r->zero ? 255 : 3); e->phase[0] = r->a;
            return true;
        }
    }
    if (!pop_world_write(world, spec, r->a)) return false;
    value_a(r, 255); e->phase[0] = r->a;
    return true;
}

/* E2 F023/F545/F551/F54B: one bounded byte-indexed link list. BCA0 is
 * immutable original location data; BDA0 is this runtime's mutable timer map. */
static bool walk_links(PopWorldState *world, PopWorldEffects *e,
    PopWorldEffectMemory *m, PopControllerState *s, PopControllerQueryResult *r) {
    uint8_t location, raw;
    for (unsigned visited = 0; visited < 256; ++visited) {
        r->x = s->aux[0xeb];
        if (!pop_world_initial_byte(0x5a0u + r->x, &location)) return false;
        value_a(r, location); compare(r, r->a, 255);
        if (r->zero) return true;
        value_a(r, location & 31u); e->cell[0] = r->a;
        s->aux[0xe9] = (location & 96u) >> 2;
        value_a(r, (uint8_t)(((m->link_map[r->x] & 224u) + s->aux[0xe9]) >> 3));
        r->carry = false; e->room[0] = r->a;
        if (!room_pointers(r->a, s, r)) return false;
        r->y = e->cell[0];
        if (!pop_world_read(world, (uint16_t)(s->aux[0x24] +
                256u * s->aux[0x25] + r->y), &raw)) return false;
        value_a(r, raw & 31u);
        if (!link_target(world, e, s, r)) return false;
        value_a(r, e->phase[0]);
        if ((r->a & 128u) == 0) queue_record(e, s, r);
        r->x = s->aux[0xeb]; ++s->aux[0xeb];
        value_a(r, location & 128u);
        if (!r->zero) return true;
    }
    return false; /* Corrupt/nonterminating link data is an explicit boundary. */
}

static bool link_effect(PopWorldState *world, PopWorldEffects *e,
    PopWorldEffectMemory *m, bool replace, PopControllerState *s,
    PopControllerQueryResult *r, const PopControllerIO *io,
    PopWorldEffectWrite write) {
    uint8_t raw;
    uint16_t type = (uint16_t)(s->aux[0x24] + 256u * s->aux[0x25] + r->y);
    uint16_t spec = (uint16_t)(s->aux[0x26] + 256u * s->aux[0x27] + r->y);
    if (!pop_world_read(world, type, &raw)) return false;
    value_a(r, raw & 31u); s->aux[0xec] = r->a;
    if (replace) {
        compare(r, r->a, 6);
        value_a(r, r->zero ? 5 : 1);
        if (!pop_world_write(world, type, r->a)) return false;
        if (r->a == 1) {
            value_a(r, 0);
            if (!pop_world_write(world, spec, r->a)) return false;
            s->aux[0xec] = 14;
        }
    }
    if (!pop_world_read(world, spec, &raw)) return false;
    s->aux[0xeb] = raw; r->x = raw;
    value_a(r, m->link_map[r->x] & 31u);
    compare(r, r->a, 31);
    if (r->zero) return true;
    compare(r, r->a, 2);
    raw = r->a;
    s->aux[0xe9] = 5;
    value_a(r, (m->link_map[r->x] & 224u) | 5u);
    m->link_map[r->x] = r->a;
    if (raw < 2) {
        e->cell[0] = r->y;
        value_a(r, s->aux[0x93]); e->room[0] = r->a;
        value_a(r, 1); e->phase[0] = r->a;
        queue_record(e, s, r);
        if (!dirty_pair(e, m, s, r, io, write, false, 16)) return false;
        value_a(r, 1); s->aux[0xe4] = r->a;
        if (!call_effect(io, 0xd909, 0, s, r)) return false;
    }
    return walk_links(world, e, m, s, r);
}

bool pop_world_effect_query_io(PopWorldState *world, PopWorldEffects *effects,
    PopWorldEffectMemory *memory, uint16_t helper_id, uint8_t argument,
    PopControllerState *state, PopControllerQueryResult *result,
    const PopControllerIO *io, PopWorldEffectWrite write) {
    if (world == NULL || effects == NULL || memory == NULL || state == NULL ||
        result == NULL || state->aux[0x95] > 31) return false;
    if (helper_id == 0xee03 || helper_id == 0xee2a)
        return spike_effect(world, effects, memory, helper_id == 0xee2a,
                            state, result, io, write);
    if (helper_id == 0x3a0f)
        return falling_effect(world, effects, memory, state, result, io, write);
    if (helper_id == 0xee0c)
        return loose_effect(world, effects, memory, state, result, io, write);
    if (helper_id == 0xee06 || helper_id == 0xee36)
        return link_effect(world, effects, memory, helper_id == 0xee36,
                           state, result, io, write);
    if (helper_id == 0xf429 || helper_id == 0xf40c || helper_id == 0xf445)
        return dirty_pair(effects, memory, state, result, io, write,
                          helper_id != 0xf445, helper_id == 0xf445 ? 16 : 31);
    return pop_world_effect_query(effects, world, helper_id, argument, state, result);
}
