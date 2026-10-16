#include "controller.h"
#include "data.h"
#include <stddef.h>
#include <string.h>

static bool negative(uint8_t v) { return (v & 0x80u) != 0; }
static void latch(uint8_t *v, bool pressed) {
    if (negative(*v)) return;
    if (!pressed) *v = 0;
    else if (*v == 0) *v = 0xff;
}
void pop_controller_update_latches(PopControllerState *s) {
    if (!s) return;
    latch(&s->aux[0xd4], negative(s->aux[0x18]));
    latch(&s->aux[0xd5], s->aux[0x18] == 1);
    latch(&s->aux[0xd6], negative(s->aux[0x19]));
    latch(&s->aux[0xd7], s->aux[0x19] == 1);
    latch(&s->aux[0xd8], negative(s->aux[0x3d]));
}
void pop_controller_mirror_horizontal(PopControllerState *s) {
    uint8_t temporary;
    if (!s) return;
    s->aux[0x18] = (uint8_t)(0u - s->aux[0x18]);
    temporary = s->aux[0xd4]; s->aux[0xd4] = s->aux[0xd5]; s->aux[0xd5] = temporary;
}
void pop_controller_save_latches(PopControllerState *s) {
    if (s) memcpy(s->aux + 0x30e, s->aux + 0xd4, 5);
}
void pop_controller_restore_latches(PopControllerState *s) {
    if (s) memcpy(s->aux + 0xd4, s->aux + 0x30e, 5);
}
void pop_controller_select_sequence(PopControllerState *s, uint8_t id) {
    uint8_t index = (uint8_t)((id - 1u) * 2u);
    if (!s) return;
    /* The immutable data reader guarantees the entire original3000 read window.
     * Byte-index wrapping is the source D2EF contract, not ID validation. */
    (void)pop_data_read(NULL, (uint16_t)(0x3000u + index), &s->aux[0x49]);
    (void)pop_data_read(NULL, (uint16_t)(0x3001u + index), &s->aux[0x4a]);
    s->selected_sequence = id;
}
typedef struct Decision {
    PopControllerState *state;
    const PopControllerIO *io;
    PopControllerQueryResult result;
} Decision;
/* 3D70 jumps to 3D3C: clear four directional latches, return consumed value1. */
static void clear_direction_latches(Decision *d) { memset(d->state->aux + 0xd4, 0, 4); }
static bool select_sequence(Decision *d, uint8_t id) {
    pop_controller_select_sequence(d->state, id); return true;
}
static bool query(Decision *d, uint16_t helper, uint8_t argument) {
    d->result.a = argument;
    if (!d->io || !d->io->query ||
        !d->io->query(d->io->context, helper, argument, d->state, &d->result)) {
        d->state->required_helper = helper; return false;
    }
    return true;
}
static bool next_query(Decision *d, uint16_t helper) { return query(d, helper, d->result.a); }
static bool spec_byte(Decision *d, uint8_t *value) {
    uint16_t base = (uint16_t)(d->state->aux[0x26] | ((uint16_t)d->state->aux[0x27] << 8));
    uint16_t address = (uint16_t)(base + d->result.y);
    if (!d->io || !d->io->read || !d->io->read(d->io->context, address, value)) {
        d->state->required_helper = 0x0026; return false;
    }
    return true;
}
static bool data_byte(Decision *d, uint16_t address, uint8_t *value) {
    if (!d->io || !d->io->read || !d->io->read(d->io->context, address, value)) {
        d->state->required_helper = address; return false;
    }
    return true;
}
static uint8_t relative_x(Decision *d, uint8_t displacement) {
    if (negative(d->state->aux[0x43])) displacement = (uint8_t)(0u-displacement);
    return (uint8_t)(d->state->aux[0x41] + displacement);
}
static bool forward_jump(Decision *d) {
    d->state->aux[0xd6] = d->state->aux[0xd4] = 1; return select_sequence(d, 3);
}
static bool crouch(Decision *d) {
    select_sequence(d, 50); clear_direction_latches(d); d->state->aux[0xd7] = 1; return true;
}
static bool draw_sword(Decision *d) {
    uint8_t *s = d->state->aux;
    clear_direction_latches(d); s[0xd4] = s[0xd8] = 1; s[0x4e] = 2;
    if (s[0x4d] == 0) s[0x8a] = 0;
    return select_sequence(d, s[0x4d] >= 2 ? 90 : 55);
}
static bool turn(Decision *d) {
    uint8_t *s = d->state->aux;
    clear_direction_latches(d); s[0xd5] = 1;
    if (s[0x30a] != 0 && s[0xd1] >= 2) {
        if (!next_query(d, 0xd0ae)) return false;
        if (d->result.negative) {
            if (!next_query(d, 0xd018)) return false;
            if (d->result.a >= 2) { s[0x4e] = 2; s[0x8a] = 0; return select_sequence(d, 89); }
        }
    }
    return select_sequence(d, 5);
}
static bool careful_step(Decision *d) {
    uint8_t *s = d->state->aux;
    s[0xd4] = s[0xd8] = 1;
    if (!next_query(d, 0x4506)) return false;
    if (d->result.a != 0) {
        s[0x4c] = d->result.a; return select_sequence(d, (uint8_t)(d->result.a + 28u));
    }
    if (d->result.x == 1 || s[0x4c] == 0) { s[0x4c] = 11; return select_sequence(d, 39); }
    s[0x4c] = 0; return select_sequence(d, 44);
}
static bool run_forward(Decision *d) {
    if (!next_query(d, 0x4506)) return false;
    if (d->result.x == 1 && d->result.y != 18) {
        if (!next_query(d, 0x4506)) return false;
        if (d->result.a < 8) return negative(d->state->aux[0xd4]) ? careful_step(d) : true;
    }
    return select_sequence(d, 1);
}
static bool pickup(Decision *d, bool *picked) {
    uint8_t *s = d->state->aux; *picked = false;
    if (!next_query(d, 0xd0ba)) return false;
    if (d->result.a == 10 || d->result.a == 22) {
        if (!next_query(d, 0xd0c0) || !next_query(d, 0xd04b)) return false;
        if (d->result.zero) return true;
        s[0x41] = relative_x(d, 0xf2); if (!next_query(d, 0x3a09)) return false;
    }
    if (!next_query(d, 0xd0bd)) return false;
    if (d->result.a == 10 || d->result.a == 22) {
        /* Original44C1 reaches crouch first, then a second press while in
         * pose109 selects the item animation and requests the E030 effect. */
        if (s[0x40] != 109) {
            if (!next_query(d, 0x4506)) return false;
            if (d->result.x != 2) s[0x41] = relative_x(d, d->result.a);
            if (!negative(s[0x43])) s[0x41] = relative_x(d, 0xfe);
            crouch(d);
        } else if (d->result.a == 22) {
            if (!query(d, 0xe030, 0xff)) return false;
            select_sequence(d, 91);
        } else {
            uint8_t spec;
            if (!spec_byte(d, &spec) || !query(d, 0xe030, (uint8_t)(spec >> 5))) return false;
            select_sequence(d, 78);
        }
        *picked = true;
    }
    return true;
}
static bool blocked_vertical_jump(Decision *d) {
    uint8_t *s = d->state->aux, value;
    clear_direction_latches(d); s[0xd6] = 1;
    if (!query(d, 0x4506, 1)) return false;
    if (d->result.a < 4 && d->result.x == 1)
        s[0x41] = relative_x(d, (uint8_t)(d->result.a - 3u));
    if (!query(d, 0xd09f, 0)) return false;
    s[0xf0] = d->result.a;
    if (!next_query(d, 0xd006)) return false;
    value = (uint8_t)(d->result.a + 250u + s[0xf0]);
    if (!query(d, 0xd009, value)) return false;
    d->result.x = d->result.a;
    d->result.y = (uint8_t)(s[0x45] - 1u);
    if (!query(d, 0xd021, s[0x4b])) return false;
    if (d->result.a != 20) {
        if (!next_query(d, 0xd04b)) return false;
        if (d->result.zero) return select_sequence(d, 28);
    }
    return select_sequence(d, 14);
}
static bool running_jump(Decision *d) {
    uint8_t *s = d->state->aux, delta, distance, adjustment;
    if (s[0x40] < 7) return true;
    s[0xc1] = 0;
    s[0xf0] = relative_x(d, 4);
    if (!query(d, 0xd00c, s[0xf0])) return false;
    s[0x8c] = d->result.a;
    if (s[0x43] == 255) delta = 255;
    else if (s[0x43] == 0) delta = 1;
    else if (!data_byte(d, (uint16_t)(0x3a18u + (uint8_t)(s[0x43]+1u)), &delta)) return false;
    for (;;) { /* Source 42FC: at most two neighboring block predicates. */
        s[0x8c] = (uint8_t)(s[0x8c] + delta);
        d->result.x = s[0x8c]; d->result.y = s[0x45];
        if (!query(d, 0xd021, s[0x4b])) return false;
        if (d->result.a == 2) break;
        if (!next_query(d, 0xd04b)) return false;
        if (d->result.zero) break;
        ++s[0xc1];
        if (s[0xc1] >= 2) goto select_running_jump;
    }
    if (!query(d, 0xd01b, s[0xf0]) ||
        !data_byte(d, (uint16_t)(0x1210u + s[0xc1]), &adjustment)) return false;
    distance = (uint8_t)(d->result.a + adjustment + adjustment - 14u);
    if (!(distance >= 248 || distance < 2)) {
        if (distance < 128) return true;
        distance = 253;
    }
    s[0x41] = relative_x(d, (uint8_t)(distance + 4u));
select_running_jump:
    clear_direction_latches(d); s[0xd6] = 1;
    return select_sequence(d, 4);
}
static bool vertical_jump(Decision *d) {
    uint8_t *s = d->state->aux;
    clear_direction_latches(d); s[0xd6] = 1;
    if (!next_query(d, 0xd0c3)) return false;
    s[0x8b] = d->result.a;
    if (!next_query(d, 0xd0c6) || !next_query(d, 0xd090)) return false;
    if (d->result.zero) {
        if (!next_query(d, 0xd01e)) return false;
        s[0x8b] = d->result.a;
        if (!next_query(d, 0xd0c3) || !next_query(d, 0xd090)) return false;
        if (d->result.zero) return blocked_vertical_jump(d);
        if (!next_query(d, 0xd018)) return false;
        if (d->result.a < 6) return blocked_vertical_jump(d);
        if (!next_query(d, 0xd0c0) || !next_query(d, 0xd04b)) return false;
        if (!d->result.zero) {
            if (!next_query(d, 0xd018)) return false;
            s[0x41] = relative_x(d, (uint8_t)(d->result.a - 14u));
            if (!next_query(d, 0x3a09)) return false;
        } else {
            if (!next_query(d, 0xd0c3) || !next_query(d, 0xd018)) return false;
            s[0x41] = relative_x(d, (uint8_t)(d->result.a - 10u));
            return select_sequence(d, 16);
        }
    }
    if (!next_query(d, 0xd0c6) || !next_query(d, 0xd018)) return false;
    s[0xf4] = d->result.a;
    if (s[0xf4] < 4) {
        if (!next_query(d, 0x4506)) return false;
        if (!(d->result.a < 4 && d->result.x == 1)) {
            s[0x41] = relative_x(d, s[0xf4]); return select_sequence(d, 8);
        }
    }
    s[0x41] = relative_x(d, (uint8_t)(s[0xf4] - 4u)); return select_sequence(d, 24);
}
static bool up(Decision *d) {
    uint8_t spec;
    if (!next_query(d, 0xd0ba)) return false;
    if (d->result.a != 16) {
        if (!next_query(d, 0xd0c0)) return false;
        if (d->result.a != 16 && !next_query(d, 0xd0bd)) return false;
    }
    if (d->result.a == 16) {
        if (!spec_byte(d, &spec)) return false;
        if ((spec >> 2) >= 30) {
            if (!query(d, 0xd012, d->state->aux[0x91])) return false;
            d->state->aux[0x41] = (uint8_t)(d->result.a + 10u);
            d->state->aux[0x43] = 0xff; return select_sequence(d, 70);
        }
    }
    return negative(d->state->aux[0x18]) ? forward_jump(d) : vertical_jump(d);
}
static bool down(Decision *d) {
    uint8_t *s = d->state->aux, spec;
    s[0xd7] = 1;
    if (!next_query(d, 0xd0bd) || !next_query(d, 0xd04b)) return false;
    if (d->result.zero) {
        if (!next_query(d, 0xd018)) return false;
        if (d->result.a < 3) { s[0x41] = relative_x(d, 5); return next_query(d, 0x3a09); }
    }
    if (!next_query(d, 0xd0c0) || !next_query(d, 0xd04b)) return false;
    if (!d->result.zero) return crouch(d);
    if (!next_query(d, 0xd018)) return false;
    if (d->result.a < 8) return crouch(d);
    if (!next_query(d, 0xd0c0)) return false;
    s[0x8b] = d->result.a;
    if (!next_query(d, 0xd0ba) || !next_query(d, 0xd090)) return false;
    if (d->result.zero) return crouch(d);
    if (negative(s[0x43])) {
        if (!next_query(d, 0xd0ba)) return false;
        if (d->result.a == 4) {
            if (!spec_byte(d, &spec)) return false;
            if ((spec >> 2) < 6) return crouch(d);
        }
    }
    if (!next_query(d, 0xd018)) return false;
    s[0x41] = relative_x(d, (uint8_t)(d->result.a - 9u)); return select_sequence(d, 68);
}
static bool hang(Decision *d) {
    uint8_t *s = d->state->aux, object, spec;
    if (s[0xe6] == 0 && negative(s[0x19])) {
        clear_direction_latches(d); s[0xd6] = s[0xd8] = 1;
        if (!next_query(d, 0xd0c3)) return false;
        object = d->result.a;
        if ((object == 13 || object == 18) && s[0x43] == 0) return select_sequence(d, 73);
        if (object == 4 && s[0x43] != 0) {
            if (!spec_byte(d, &spec)) return false;
            if ((spec >> 2) < 6) return select_sequence(d, 73);
        }
        return select_sequence(d, 10);
    }
    if (negative(s[0x3d])) {
        if (s[0x46] != 6) {
            if (!next_query(d, 0xd0ba)) return false;
            object = d->result.a;
            if (object == 20 || (s[0x43] == 255 && (object == 7 || object == 12)))
                return select_sequence(d, 25);
        }
        if (!next_query(d, 0xd0c3) || !next_query(d, 0xd04b)) return false;
        if (!d->result.zero) return true;
    }
    clear_direction_latches(d); s[0xd7] = 1;
    if (!next_query(d, 0xd0c0) || !next_query(d, 0xd04b)) return false;
    if (d->result.zero) {
        if (!next_query(d, 0xd0ba) || !next_query(d, 0xd04b)) return false;
        if (d->result.zero) return select_sequence(d, 23);
    }
    if (!next_query(d, 0xd0ba)) return false;
    object = d->result.a;
    if (object == 20 || (negative(s[0x43]) && (object == 7 || object == 12)))
        s[0x41] = relative_x(d, 0xf9);
    return select_sequence(d, 11);
}
static bool standing(Decision *d) {
    uint8_t *s = d->state->aux;
    bool picked;
    if (negative(s[0xd8]) && negative(s[0x3d])) {
        if (!pickup(d, &picked)) return false;
        if (picked) return true;
    }
    if (s[0x4d] != 0) {
        if (negative(s[0xd7]) && negative(s[0xd4])) return draw_sword(d);
    } else if (s[0x30a] != 0) {
        if (s[0x8a] == 0 || negative(s[0x3d])) {
            if (s[0xd1] >= 2) {
                if (!next_query(d, 0xd0ae)) return false;
                if (d->result.a >= 246 || d->result.a < 90) {
                    s[0xd3] = 1;
                    if (d->result.a >= 250) return turn(d);
                    if (!(s[0x3a9] == 1 && (s[0x3a2] == 3 ||
                        (s[0x39c] >= 107 && s[0x39c] < 118)))) return draw_sword(d);
                }
            }
            s[0x8a] = 0;
        } else {
            if (negative(s[0xd4])) return run_forward(d);
            if (negative(s[0xd5])) return turn(d);
            if (negative(s[0xd6])) return up(d);
            if (negative(s[0xd7])) return down(d);
            return negative(s[0x18]) ? run_forward(d) : true;
        }
    }
    if (negative(s[0x3d])) {
        if (negative(s[0xd5])) return turn(d);
        if (negative(s[0xd6])) return up(d);
        if (negative(s[0xd7])) return down(d);
        return negative(s[0x18]) && negative(s[0xd4]) ? careful_step(d) : true;
    }
    if (negative(s[0xd4])) return run_forward(d);
    if (negative(s[0xd5])) return turn(d);
    if (negative(s[0xd6])) return up(d);
    if (negative(s[0xd7])) return down(d);
    return negative(s[0x18]) ? run_forward(d) : true;
}
static bool sword_step(Decision *d, bool forward) {
    uint8_t *s = d->state->aux, pose = s[0x40];
    if (pose != 158 && pose != 170 && pose != 171) return true;
    s[forward ? 0xd4 : 0xd5] = 1;
    return select_sequence(d, forward ? (s[0x4d] == 0 ? 86 : 56) : 57);
}
static bool sword_up(Decision *d) {
    uint8_t *s = d->state->aux, pose = s[0x40];
    if (pose == 167) { s[0xd6] = 1; return select_sequence(d, 61); }
    if (pose != 158 && pose != 170 && pose != 171 && pose != 168 && pose != 165) return true;
    if (!next_query(d, 0xd0ae)) return false;
    if (d->result.a < 32) {
        if (s[0x4d] != 0) {
            if (s[0x39c] != 152) return true;
        } else {
            if (s[0x39c] == 168) return true;
            if (s[0x39c] == 153) {
                s[0xd6] = 1; select_sequence(d, 62);
                return next_query(d, 0x450c);
            }
            if (s[0x39c] != 151 && s[0x39c] != 152 && s[0x39c] != 162) {
                s[0xd6] = 1; return select_sequence(d, 62);
            }
        }
    } else if (s[0x4d] != 0) return sword_step(d, false);
    s[0xd6] = 1; return select_sequence(d, 62);
}
static bool sword_attack(Decision *d) {
    uint8_t *s = d->state->aux, pose = s[0x40];
    if (pose == 157 || pose == 158 || pose == 170 || pose == 171 || pose == 165) {
        s[0xd8] = 1; return select_sequence(d, s[0x4d] == 0 ? 75 : 58);
    }
    if (pose == 150 || pose == 161) { s[0xd8] = 1; return select_sequence(d, 66); }
    return true;
}
static bool sword_controls(Decision *d) {
    uint8_t *s = d->state->aux, pose = s[0x40];
    if (pose == 161 && !negative(s[0xd8])) return select_sequence(d, 57);
    if (negative(s[0xd8])) {
        if (s[0x4d] == 0) s[0x7f] = 15;
        if (!sword_attack(d)) return false;
        if (s[0xd8] == 1) return true;
    }
    if (negative(s[0xd7])) {
        if (pose != 158 && pose != 170 && pose != 171) return true;
        s[0xd7] = 1; s[0x4e] = 0;
        if (s[0x4d] == 0) {
            s[0x8a] = 1; s[0xa1] = 9; s[0xd3] = 0;
            return select_sequence(d, 93);
        }
        return select_sequence(d, s[0x4d] == 1 ? 92 : 87);
    }
    if (negative(s[0xd6])) return sword_up(d);
    if (negative(s[0xd4])) return sword_step(d, true);
    if (negative(s[0xd5])) return sword_step(d, false);
    return true;
}
static bool sword_decision(Decision *d) {
    uint8_t *s = d->state->aux;
    if (s[0x46] >= 2) return true;
    if (!next_query(d, 0xd0ba)) return false;
    if (d->result.a == 11 || s[0xd1] >= 2) {
        if (!next_query(d, 0xd0ae)) return false;
        if (d->result.a < 90 || d->result.a >= 252) return sword_controls(d);
        if (d->result.a >= 128) return select_sequence(d, 60);
    }
    if (s[0x4d] == 0) s[0xd3] = 0;
    else if (s[0x4d] >= 2) return sword_controls(d);
    if (s[0x40] != 171) return true;
    s[0x4e] = 0; return select_sequence(d, 92);
}
static bool decision(Decision *d) {
    uint8_t *s = d->state->aux, pose = s[0x40]; bool picked;
    if (!negative(s[0x4f])) {
        if (pose == 15 || pose == 166 || pose == 158 || pose == 171) return select_sequence(d, 71);
        return true;
    }
    if (s[0x46] == 4 || s[0x46] == 5) { clear_direction_latches(d); return true; }
    if (s[0x4e] == 2) return sword_decision(d);
    if (s[0x4d] >= 2) {
        if (pose != 166 || !negative(s[0xd7])) return true;
        if (negative(s[0xd4])) return draw_sword(d);
        s[0xd7] = 1; return select_sequence(d, 80);
    }
    if (pose == 15 || (pose >= 50 && pose < 53)) return standing(d);
    if (pose == 48) {
        if (!negative(s[0x3d]) && negative(s[0x18]) && !negative(s[0x19])) return select_sequence(d, 43);
        return true;
    }
    if (pose < 4) return negative(s[0x19]) && negative(s[0x18]) ? forward_jump(d) : true;
    if (pose >= 67 && pose < 70) return negative(s[0x18]) || negative(s[0xd4]) ? forward_jump(d) : true;
    if (pose < 15) {
        if (s[0x18] == 0) {
            if (pose != 7 && pose != 11) return true;
            clear_direction_latches(d); s[0xd4] = 1; return select_sequence(d, 13);
        }
        if (!negative(s[0x18])) {
            clear_direction_latches(d); s[0xd5] = 1; return select_sequence(d, 6);
        }
        if (negative(s[0x19])) return negative(s[0xd6]) ? running_jump(d) : true;
        if (!negative(s[0xd7])) return true;
        s[0xd7] = 1; return select_sequence(d, 26);
    }
    if (pose >= 87 && pose < 100) return hang(d);
    if (pose == 109) {
        if (negative(s[0xd8])) {
            if (!pickup(d, &picked)) return false;
            if (picked) return true;
        }
        if (s[0x19] != 1) return select_sequence(d, 49);
        if (negative(s[0xd4])) { s[0xd4] = 1; return select_sequence(d, 79); }
    }
    return true;
}
PopControllerResult pop_controller_decide(PopControllerState *state, const PopControllerIO *io) {
    PopControllerState work; Decision d;
    if (!state) return POP_CONTROLLER_INVALID;
    work = *state; work.required_helper = 0; work.selected_sequence = 0;
    d = (Decision){ &work, io, {0, 0, 0, false, false, false} };
    if (!decision(&d)) {
        state->required_helper = work.required_helper; state->selected_sequence = 0;
        return POP_CONTROLLER_HELPER_REQUIRED;
    }
    *state = work; return POP_CONTROLLER_COMPLETE;
}
PopControllerResult pop_controller_sampled_player(PopControllerState *state,
    uint8_t horizontal, uint8_t vertical, uint8_t button, const PopControllerIO *io) {
    PopControllerState work; PopControllerResult result; bool mirrored;
    if (!state) return POP_CONTROLLER_INVALID;
    work = *state;
    if (negative(work.aux[0x4f]) && work.aux[0xce] == 0) work.aux[0x4f] = 0;
    if (work.aux[0xe6] != 0) --work.aux[0xe6];
    if (work.aux[0x3f4] == 0) { state->required_helper = 0x3d08; return POP_CONTROLLER_HELPER_REQUIRED; }
    pop_controller_restore_latches(&work);
    work.aux[0x18] = horizontal; work.aux[0x19] = vertical; work.aux[0x3d] = button;
    pop_controller_update_latches(&work);
    mirrored = !negative(work.aux[0x43]);
    if (mirrored) pop_controller_mirror_horizontal(&work);
    result = pop_controller_decide(&work, io);
    if (result != POP_CONTROLLER_COMPLETE) {
        state->required_helper = work.required_helper; state->selected_sequence = 0; return result;
    }
    if (mirrored) pop_controller_mirror_horizontal(&work);
    pop_controller_save_latches(&work); *state = work; return POP_CONTROLLER_COMPLETE;
}
