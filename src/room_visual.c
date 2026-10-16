#include "room_visual.h"
#include "room_geometry.h"
#include <string.h>

extern const uint8_t pop_torch_images[18];
extern const PopRoomVisualTemplate pop_room_visual_templates[24];
extern const uint8_t pop_loose_face[11];
extern const uint8_t pop_loose_y_offset[11];
extern const uint8_t pop_loose_bottom[11];
extern const uint8_t pop_gate_partial[9];

static bool append_background(PopRoomVisualScene *output, PopRoomBackgroundDraw draw)
{
    if (draw.bottom_y >= 192) return true;
    if (output->room.background_count >= 199) return false;
    output->background[output->room.background_count++] = draw;
    return true;
}

/* Actual1D63/1DB9 previous-type4 source producer. The underlay is a
 * separately ordered template block, admitted by the same branch below.
 * No original instructions are stored or interpreted. */
static bool gate_background(PopRoomVisualScene *output,
                             const PopRoomDrawTemplate *entry, uint8_t spec)
{
    const unsigned height = ((spec < 188 ? spec : 188u) >> 2) + 1u;
    const uint8_t end = (uint8_t)(entry->floor_y - height);
    const bool closed = (uint8_t)(end + 12u) >= entry->floor_y;
    PopRoomBackgroundDraw draw = entry->draw;
    draw.image_id = closed ? 68 : 67;
    draw.bottom_y = closed ? (uint8_t)(end - 2u) : end;
    draw.mode = closed ? 1 : 2;
    if (!append_background(output, draw)) return false;
    uint8_t y = (uint8_t)(end - 12u);
    draw.image_id = 55;
    draw.mode = 2;
    for (;;) {
        if (y >= 192) return true;
        if (y < 7 || (uint8_t)(y - 7u) < entry->clip_top) break;
        draw.bottom_y = y;
        if (!append_background(output, draw)) return false;
        const uint8_t next = (uint8_t)(y - 8u);
        if (next == 0) break; /* Original BNE falls through with0002 unchanged. */
        y = next;
    }
    const uint8_t partial = (uint8_t)(y - entry->clip_top + 1u);
    if (partial != 0 && partial < 9) {
        draw.image_id = pop_gate_partial[partial];
        draw.bottom_y = y;
        if (!append_background(output, draw)) return false;
    }
    return true;
}

bool pop_room_visual_build(const PopWorldState *world, const PopRoomScene *room,
                           PopRoomVisualScene *out)
{
    PopRoomVisualScene output;
    bool removed_geometry = false;
    if (world == NULL || room == NULL || out == NULL ||
        pop_room_scene_initial(room->room_id) != room) return false;
    for (unsigned i = 0; i < POP_WORLD_ROOM_COUNT * POP_WORLD_CELLS_PER_ROOM; ++i) {
        uint8_t type, spec;
        if(!pop_room_geometry_uses_world_cell(room->room_id,i))continue;
        if (!pop_world_initial_byte(i, &type) ||
            !pop_world_initial_byte(720u + i, &spec)) return false;
        if (world->types[i] != type) {
            const uint8_t original = type & 31u;
            const uint8_t replacement = original == 11 ? 0 : 1;
            if ((original != 11 && original != 10 && original != 22) ||
                world->types[i] != replacement || world->specs[i] != 0) return false;
            removed_geometry = true;
            continue;
        }
        if ((type & 31u) == 19) {
            if (world->specs[i] >= 18) return false;
        } else if ((type & 31u) == 4) {
            /* Source1D63 clamps the full unsigned spec to188 for drawing. */
        } else if ((type & 31u) == 11) {
            /* 1BBE uses a direct positive index, or masks negative phase
             * state and clamps an out-of-range masked index to one. */
            if (world->specs[i] < 128 && world->specs[i] >= 11) return false;
        } else if (world->specs[i] != spec) return false;
    }
    const PopRoomVisualTemplate *source = &pop_room_visual_templates[room->room_id - 1u];
    PopRoomVisualTemplate replacement_program;
    PopRoomDrawTemplate geometry_background[200];
    PopForegroundDraw geometry_foreground[100];
    if (removed_geometry) {
        if (!pop_room_geometry_program(world, room->room_id,
            geometry_background, 200, geometry_foreground, 100,
            &replacement_program)) return false;
        source = &replacement_program;
    }
    if (source->room_id != room->room_id) return false;
    memset(&output, 0, sizeof(output));
    output.room = *room;
    /* Original1ABA creates a potion-bubble object only while its source tile
     * remains type10. Each measured first-level potion room has one such
     * middle command. Preserve the original bubble if no local pickup occurred. */
    if (output.room.middle_count != 0) {
        bool potion_present = false;
        for (unsigned cell = 0; cell < 30; ++cell)
            if ((world->types[(room->room_id - 1u) * 30u + cell] & 31u) == 10)
                potion_present = true;
        if (!potion_present) output.room.middle_count = 0;
    }
    output.room.background_count = 0;
    for (size_t i = 0; i < source->draw_count; ++i) {
        const PopRoomDrawTemplate *entry = &source->draws[i];
        PopRoomBackgroundDraw draw = entry->draw;
        if (entry->operation != 0) {
            PopWorldCell owner;
            if (!pop_world_cell(world, entry->owner_room, entry->owner_cell, &owner)) return false;
            if (entry->operation == 5 || entry->operation == 6) {
                if ((owner.raw_type & 31u) != 4) return false;
                if (entry->operation == 5) {
                    if (!gate_background(&output, entry, owner.spec)) return false;
                    continue;
                }
                const unsigned height = ((owner.spec < 188 ? owner.spec : 188u) >> 2) + 1u;
                const uint8_t end = (uint8_t)(entry->floor_y - height);
                if ((uint8_t)(end + 12u) < entry->floor_y) continue;
            } else if (entry->operation == 4) {
                if ((owner.raw_type & 31u) != 19 || owner.spec >= 18) return false;
                draw.image_id = pop_torch_images[owner.spec];
            } else {
                uint8_t phase = owner.spec;
                if ((owner.raw_type & 31u) != 11) return false;
                if ((phase & 128u) != 0) {
                    phase &= 127u;
                    if (phase >= 11) phase = 1;
                }
                if (phase >= 11) return false;
                if (entry->operation == 1) draw.image_id = pop_loose_face[phase];
                else if (entry->operation == 2) draw.image_id = pop_loose_bottom[phase];
                else if (entry->operation == 3)
                    draw.bottom_y = (uint8_t)(draw.bottom_y + pop_loose_y_offset[phase]);
                else return false;
            }
        }
        /* Original0503 rejects a draw after the producer's 8-bit Y math.
         * In particular, an above-room edge at255 can wrap to visible0. */
        if (!append_background(&output, draw)) return false;
    }
    memset(output.foreground_preserve, 255, sizeof(output.foreground_preserve));
    if (!pop_foreground_apply(output.foreground_xor, POP_HGR_PAGE_BYTES,
        source->foreground, source->foreground_count) ||
        !pop_foreground_apply(output.foreground_preserve, POP_HGR_PAGE_BYTES,
        source->foreground, source->foreground_count)) return false;
    for (size_t i = 0; i < POP_HGR_PAGE_BYTES; ++i)
        output.foreground_preserve[i] ^= output.foreground_xor[i];
    *out = output;
    out->room.background = out->background;
    out->room.foreground_preserve = out->foreground_preserve;
    out->room.foreground_xor = out->foreground_xor;
    return true;
}

bool pop_room_visual_states(const PopWorldState *world, const PopRoomScene *room,
                            PopRoomCommandState *states, size_t state_count)
{
    PopRoomCommandState output[200];
    if (world == NULL || room == NULL || states == NULL || state_count == 0 ||
        state_count > 200 || state_count != room->background_count ||
        pop_room_scene_initial(room->room_id) != room) return false;
    for (unsigned i = 0; i < POP_WORLD_ROOM_COUNT * POP_WORLD_CELLS_PER_ROOM; ++i) {
        uint8_t type, spec;
        if(!pop_room_geometry_uses_world_cell(room->room_id,i))continue;
        if (!pop_world_initial_byte(i, &type) ||
            !pop_world_initial_byte(720u + i, &spec) || world->types[i] != type) return false;
        if ((type & 31u) == 19) {
            if (world->specs[i] >= 18) return false;
        } else if (world->specs[i] != spec) return false;
    }
    for (size_t i = 0; i < state_count; ++i) {
        const PopRoomBackgroundDraw *source = &room->background[i];
        if (!pop_room_default_state(room, i, &output[i])) return false;
        if (source->dynamic_family == POP_ROOM_FLAME_IMAGES) {
            unsigned owner_room = room->room_id, owner_cell;
            uint8_t neighbor;
            PopWorldCell owner;
            if (source->producer_cell >= 30 || source->previous_type != 19) return false;
            if (source->producer_cell % 10 == 0) {
                if (!pop_world_initial_neighbor(owner_room, POP_WORLD_LEFT, &neighbor) || neighbor == 0) return false;
                owner_room = neighbor;
                owner_cell = source->producer_cell + 9u;
            } else owner_cell = source->producer_cell - 1u;
            if (!pop_world_cell(world, owner_room, owner_cell, &owner) ||
                (owner.raw_type & 31u) != 19 || owner.spec >= 18) return false;
            output[i].image_id = pop_torch_images[owner.spec];
        }
    }
    memcpy(states, output, state_count * sizeof(*states));
    return true;
}
