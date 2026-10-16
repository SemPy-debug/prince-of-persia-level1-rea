#include "player_render.h"
#include "geometry.h"
#include "data.h"
#include <string.h>

uint32_t pop_player_render_delay_cycles(const PopControllerState *state)
{
    if (state == NULL || state->aux[0x5b] == 0 ||
        state->aux[0x5b] != state->aux[0xcb] || state->aux[0x50] != 0) return 0;
    /* E021 JMP E0C4;25 rounds of PHA/LDX0/256 DEX-BNE/PLA/SEC/SBC1.
     * Each non-final round1295 cycles; final branch saves1,RTS adds6,
     * and E021's trampoline adds3:25*1295-1+6+3=32383. */
    return 32383u;
}

static bool tile(PopWorldState *world, PopControllerState *state,
                  uint8_t column, uint8_t row, uint8_t *value)
{
    PopControllerQueryResult q = {0, column, row, false, false, false};
    if (!pop_world_query(world, 0xd021, state->aux[0x4b], state, &q)) return false;
    *value = q.a;
    return true;
}

static bool column(PopWorldState *world, PopControllerState *state,
                    uint8_t x, uint8_t *value)
{
    PopControllerQueryResult q = {0, 0, 0, false, false, false};
    if (!pop_world_query(world, 0xd009, x, state, &q)) return false;
    *value = q.a;
    return true;
}

/* D311/D06C source object-cell selection. Dirty-mark calls consume this cell
 * later, but a full-room renderer does not need their repeat counters. */
static bool object_cell(uint8_t *a)
{
    uint8_t offset;
    a[0x92] = a[0x46] == 1 ? a[0xbd] : a[0x45];
    a[0x91] = a[0x46] == 1 ? a[0xba] : a[0x44];
    if ((a[0x40] >= 0x87 && a[0x40] < 0x95) || a[0x40] == 2 ||
        a[0x40] == 3 || a[0x40] == 4 || a[0x40] == 6) --a[0x91];
    if ((a[0x92] & 0x80u) != 0) a[0x75] = a[0x91];
    else if (a[0x92] >= 3 || a[0x91] >= 10) a[0x75] = 30;
    else {
        if (!pop_data_read(NULL, (uint16_t)(0x1200u + a[0x92]), &offset)) return false;
        a[0x75] = (uint8_t)(a[0x91] + offset);
    }
    return true;
}

/* Actual D59C clip producer. Raw source tile IDs are used as predicates;
 * no generic solid/empty classification or default clipping is substituted. */
static bool clip(PopWorldState *world, PopControllerState *state, uint16_t *helper)
{
    uint8_t *a = state->aux, type, limit, index;
    bool vertical = true;
    if (a[0x40] >= 0xe0 && a[0x40] < 0xe5) {
        limit = (uint8_t)(a[0x215] + 2u);
        if (limit < a[0x73]) {
            a[0x76] = limit;
            return true;
        }
        if (helper != NULL) *helper = 0xfc00;
        return false;
    }
    if (!tile(world, state, a[0xba], a[0xbc], &type)) return false;
    if (type != 20 && pop_geometry_d684(type) == 0) vertical = false;
    if (vertical && !(a[0x46] == 0 && (a[0x40] == 0x4f || a[0x40] == 0x51))) {
        if (!tile(world, state, a[0xbb], a[0xbc], &type)) return false;
        if (type != 20 && pop_geometry_d684(type) == 0) vertical = false;
    }
    if (vertical) {
        index = (uint8_t)(a[0x45] + 1u);
        if (!pop_data_read(NULL, (uint16_t)(0x1274u + index), &limit)) return false;
        if (index == 1 || (limit < a[0x73] && (uint8_t)(limit - 15u) < a[0xb9])) {
            a[0x76] = a[0xb9] = limit;
        }
    }
    if (!column(world, state, a[0xbe], &a[0x8c]) ||
        !tile(world, state, a[0x8c], a[0x45], &type)) return false;
    if (type == 12 || type == 7) {
        bool edge = (a[0x43] & 0x80u) == 0 && a[0x46] == 2;
        if (!edge) {
            if (!tile(world, state, a[0x8c], a[0xbc], &type)) return false;
            edge = type == 20 || type == 12 || type == 7;
        }
        if (edge) {
            a[0x79] = (uint8_t)(4u * a[0x91] + 4u);
            return true;
        }
    }
    if (!column(world, state, a[0xbf], &a[0x8c]) ||
        !tile(world, state, a[0x8c], a[0x45], &type)) return false;
    if (type == 20) {
        if (!tile(world, state, a[0x8c], a[0xbc], &type)) return false;
        if (type == 20 && a[0x93] == a[0x4b]) a[0x79] = (uint8_t)(4u * a[0x91]);
    }
    return true;
}

static bool project_actor(const PopWorldState *world,
                                const PopControllerState *state,
                                PopControllerState *projection,
                                uint16_t *required_helper, unsigned record)
{
    PopControllerState work;
    PopWorldState world_copy;
    PopControllerQueryResult q = {0, 0, 0, false, false, false};
    uint16_t helper = 0;
    if (required_helper != NULL) *required_helper = 0;
    if (world == NULL || state == NULL || projection == NULL) return false;
    work = *state;
    world_copy = *world;
    memcpy(work.aux + 0x40, work.aux + record, 16); /* D05A/D835 or D05D/D849 */
    if (work.aux[0x40] == 0) helper = 0xe021;
    else if (!pop_geometry_load_frame(work.aux) || !pop_geometry_update_column(work.aux)) helper = 0x3a09;
    else if (!pop_geometry_project_actor(work.aux)) helper = 0xd066;
    else if (!pop_world_query(&world_copy, 0xd0ba, 0, &work, &q)) helper = 0xd084;
    else {
        if (q.a == 5) ++work.aux[0x42]; /* D8F4; projection73 was already formed. */
        if (!pop_geometry_bounds(work.aux)) helper = 0xd033;
        else if (!object_cell(work.aux)) helper = 0xd036;
        else {
            /* D561 expands the bounds for weapon poses before D59C. Its
             * D763 calls only write5E78 redraw counters; D4F1/D545 similarly
             * select dirty-counter writers. They do not change70..7A or
             * geometry bounds used by the following clip producer. */
            if (work.aux[0x4e] >= 2) {
                if ((work.aux[0x43] & 0x80u) != 0) --work.aux[0xba];
                else ++work.aux[0xbb];
            }
            if (!clip(&world_copy, &work, &helper) && helper == 0) helper = 0xd03c;
        }
    }
    if (helper != 0) {
        if (required_helper != NULL) *required_helper = helper;
        return false;
    }
    *projection = work;
    return true;
}

bool pop_player_render_project(const PopWorldState *world,
                                const PopControllerState *state,
                                PopControllerState *projection,
                                uint16_t *required_helper)
{
    return project_actor(world, state, projection, required_helper, 0x50);
}

bool pop_guard_render_project(const PopWorldState *world,
                               const PopControllerState *state,
                               PopControllerState *projection,
                               uint16_t *required_helper)
{
    if (state != NULL && state->aux[0x6d] != 2) {
        if (required_helper != NULL) *required_helper = 0xd054;
        return false;
    }
    return project_actor(world, state, projection, required_helper, 0x60);
}

PopActorDrawStatus pop_actor_attachment_draw(const PopControllerState *projection,
                                             PopSceneDraw *draw,
                                             uint16_t *required_helper)
{
    PopControllerState work;
    uint8_t image, dx, dy;
    if (required_helper != NULL) *required_helper = 0;
    if (projection == NULL || draw == NULL) return POP_ACTOR_DRAW_INVALID;
    work = *projection;
    uint8_t *a = work.aux;
    /* Original D391/D027 independently admits an attachment for a kind2
     * negative4F actor, posesE5..ED, or nonzero weapon4E. The D9 low6 bits
     * index a source three-byte image/dx/dy record at2F3A. */
    if (!((a[0x4d] == 2 && (a[0x4f] & 128u) != 0) ||
          (a[0x40] >= 0xe5 && a[0x40] < 0xee) || a[0x4e] != 0) ||
        (a[0xd9] & 63u) == 0) return POP_ACTOR_DRAW_SKIPPED;
    const uint16_t address = (uint16_t)(0x2f3au + 3u * ((a[0xd9] & 63u) - 1u));
    if (!pop_data_read(NULL, address, &image) ||
        !pop_data_read(NULL, (uint16_t)(address + 1u), &dx) ||
        !pop_data_read(NULL, (uint16_t)(address + 2u), &dy)) {
        if (required_helper != NULL) *required_helper = 0xd42a;
        return POP_ACTOR_DRAW_UNSUPPORTED;
    }
    if (image == 0) return POP_ACTOR_DRAW_SKIPPED;
    a[0x70] = image;
    a[0x7a] = 2; /* D473 attachment character-table selector. */
    int delta = dx < 128 ? (int)dx : (int)dx - 256;
    if ((a[0x74] & 128u) != 0) delta = -delta; /* Source D1C9. */
    const uint16_t pixel = (uint16_t)((unsigned)a[0x71] + 256u * a[0x72] + delta);
    a[0x71] = (uint8_t)pixel;
    a[0x72] = (uint8_t)(pixel >> 8);
    a[0x73] = (uint8_t)(a[0x73] + dy);
    return pop_actor_draw_projected(a, sizeof(work.aux), 3, draw);
}

PopActorDrawStatus pop_player_render_draw(const PopWorldState *world,
                                         const PopControllerState *state,
                                         PopSceneDraw *draw,
                                         uint16_t *required_helper)
{
    PopControllerState projected;
    if (required_helper != NULL) *required_helper = 0;
    if (world == NULL || state == NULL || draw == NULL) return POP_ACTOR_DRAW_INVALID;
    if (state->aux[0x5b] == 0 || state->aux[0x5b] != state->aux[0xcb])
        return POP_ACTOR_DRAW_SKIPPED;
    if (pop_player_render_delay_cycles(state) != 0)
        return POP_ACTOR_DRAW_SKIPPED;
    if (!pop_player_render_project(world, state, &projected, required_helper))
        return POP_ACTOR_DRAW_UNSUPPORTED;
    return pop_actor_draw_projected(projected.aux, sizeof(projected.aux), 0, draw);
}
