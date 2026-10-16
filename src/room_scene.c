#include "room_scene.h"

#include <string.h>

extern const PopRoomScene pop_initial_room_scenes[];
extern const size_t pop_initial_room_scene_count;

static bool valid_room(const PopRoomScene *room)
{
    return room != NULL && room->room_id >= 1 && room->room_id <= 24 &&
           room->page_bytes == POP_HGR_PAGE_BYTES && room->background != NULL &&
           room->background_count <= 200 && room->foreground_preserve != NULL &&
           room->foreground_xor != NULL && room->middle_count <= 45 &&
           (room->middle_count == 0 || room->middle != NULL);
}

static unsigned image_family(PopAssetTable table, unsigned image)
{
    if (table != POP_ASSET_BACKGROUND_1) return POP_ROOM_STATIC;
    if ((image >= 82 && image <= 86) || (image >= 97 && image <= 100))
        return POP_ROOM_FLAME_IMAGES;
    if ((image >= 11 && image <= 13) || (image >= 47 && image <= 62) ||
        image == 67 || image == 68) return POP_ROOM_GATE_IMAGES;
    if (image == 27 || image == 30 || image == 31 || image == 44 || image == 45)
        return POP_ROOM_LOOSE_FLOOR_IMAGES;
    if (image == 5 || image == 6 || (image >= 34 && image <= 43))
        return POP_ROOM_SPIKE_IMAGES;
    return POP_ROOM_STATIC;
}

const PopRoomScene *pop_room_scene_initial(unsigned room)
{
    for (size_t i = 0; i < pop_initial_room_scene_count; ++i) {
        if (pop_initial_room_scenes[i].room_id == room) return &pop_initial_room_scenes[i];
    }
    return NULL;
}

bool pop_room_default_state(const PopRoomScene *room, size_t index,
                            PopRoomCommandState *out)
{
    if (!valid_room(room) || index >= room->background_count || out == NULL) return false;
    const PopRoomBackgroundDraw *draw = &room->background[index];
    *out = (PopRoomCommandState){true, draw->image_id, draw->x_byte,
                                 draw->bottom_y, draw->mode};
    return true;
}

/* Original MAIN LC F650 selects E9AA[mode] as the indirect-Y opcode:
 * mode0 AND31, mode1 OR11, mode2 copy91. Source rows are bottom-first.
 * This bounded room interface admits horizontal40-byte page placements only. */
static bool background_draw(uint8_t *page, const PopRoomBackgroundDraw *source,
                             const PopRoomCommandState *state)
{
    PopAsset asset;
    if (!state->enabled) return true;
    if (state->mode > 2 || state->bottom_y >= 192 ||
        (source->table != POP_ASSET_BACKGROUND_1 && source->table != POP_ASSET_BACKGROUND_2) ||
        !pop_asset_get(source->table, state->image_id, &asset) ||
        (unsigned)state->x_byte + asset.width_bytes > 40) return false;
    if (state->mode == 2) {
        return pop_render_background_mode2(page, POP_HGR_PAGE_BYTES, source->table,
            state->image_id, state->x_byte, state->bottom_y);
    }
    for (unsigned row = 0; row < asset.height_rows && row <= state->bottom_y; ++row) {
        uint16_t address;
        (void)pop_asset_hgr_row_address((unsigned)state->bottom_y - row, &address);
        for (unsigned column = 0; column < asset.width_bytes; ++column) {
            uint8_t *dest = &page[(size_t)address - 0x2000u + state->x_byte + column];
            const uint8_t dots = asset.encoded_rows[(size_t)row * asset.width_bytes + column];
            *dest = state->mode == 0 ? (uint8_t)(*dest & dots) : (uint8_t)(*dest | dots);
        }
    }
    return true;
}

bool pop_room_background(uint8_t *page, size_t page_bytes,
                         const PopRoomScene *room,
                         const PopRoomCommandState *states, size_t state_count)
{
    uint8_t output[POP_HGR_PAGE_BYTES];
    if (page == NULL || page_bytes != POP_HGR_PAGE_BYTES || !valid_room(room) ||
        (states == NULL ? state_count != 0 : state_count != room->background_count)) return false;
    memset(output, 0x80, sizeof(output));
    for (size_t i = 0; i < room->background_count; ++i) {
        const PopRoomBackgroundDraw *draw = &room->background[i];
        PopRoomCommandState value;
        if (!pop_room_default_state(room, i, &value)) return false;
        if (states != NULL) {
            value = states[i];
            if (draw->dynamic_family == POP_ROOM_STATIC) {
                if (!value.enabled || value.image_id != draw->image_id ||
                    value.x_byte != draw->x_byte || value.bottom_y != draw->bottom_y ||
                    value.mode != draw->mode) return false;
            } else if (image_family(draw->table, value.image_id) != draw->dynamic_family) {
                return false;
            }
        }
        if (!background_draw(output, draw, &value)) return false;
    }
    memcpy(page, output, sizeof(output));
    return true;
}

bool pop_room_foreground(uint8_t *page, size_t page_bytes,
                         const PopRoomScene *room)
{
    if (page == NULL || page_bytes != POP_HGR_PAGE_BYTES || !valid_room(room)) return false;
    const PopSceneLayers layers = {page, room->foreground_preserve,
                                   room->foreground_xor, POP_HGR_PAGE_BYTES};
    return pop_scene_foreground(page, page_bytes, &layers);
}

/* Original EE06 mode2, reached by the room5 potion-bubble image with phase2.
 * F08A uses shifted source dots and preserves outside edge bits, without the
 * neighbor-dot mask of mode4. This implementation is bounded to normal copy
 * mode2 and its existing seven-bit shift primitives. */
static bool middle_copy(uint8_t *page, const PopSceneDraw *draw)
{
    const PopRenderParams *p = &draw->params;
    PopAsset asset;
    uint8_t left_mask, right_mask;
    if (p->mode != 2 || p->phase > 6 || p->clip_left > p->clip_right ||
        p->clip_right > 40 || p->clip_top > p->clip_bottom || p->clip_bottom > 192 ||
        !pop_asset_get(draw->table, draw->image_id, &asset)) return false;
    (void)pop_asset_edge_masks(p->phase, &left_mask, &right_mask);
    const int left = p->x_byte < 128 ? p->x_byte : (int)p->x_byte - 256;
    for (unsigned row = 0; row < asset.height_rows; ++row) {
        const int y = (int)p->bottom_y - (int)row;
        if (y < p->clip_top || y >= p->clip_bottom) continue;
        uint16_t address;
        (void)pop_asset_hgr_row_address((unsigned)y, &address);
        const size_t offset = (size_t)address - 0x2000u;
        uint8_t carry = 0;
        if (left >= p->clip_left && left < p->clip_right)
            carry = page[offset + (unsigned)left] & left_mask;
        for (unsigned col = 0; col < asset.width_bytes; ++col) {
            uint8_t primary, next;
            (void)pop_asset_shift7(asset.encoded_rows[(size_t)row * asset.width_bytes + col],
                                   p->phase, &primary, &next);
            const int x = left + (int)col;
            if (x >= p->clip_left && x < p->clip_right)
                page[offset + (unsigned)x] = (uint8_t)(primary | carry);
            carry = next;
        }
        const int tail = left + asset.width_bytes;
        if (tail >= p->clip_left && tail < p->clip_right) {
            uint8_t *dest = &page[offset + (unsigned)tail];
            *dest = (uint8_t)((*dest & right_mask) | carry);
        }
    }
    return true;
}

bool pop_room_compose(uint8_t *page, size_t page_bytes,
                      const PopRoomScene *room,
                      const PopRoomCommandState *states, size_t state_count,
                      const PopSceneDraw *actors, size_t actor_count)
{
    uint8_t background[POP_HGR_PAGE_BYTES];
    if (page == NULL || page_bytes != POP_HGR_PAGE_BYTES ||
        !pop_room_background(background, sizeof(background), room, states, state_count)) return false;
    for (size_t i = 0; i < room->middle_count; ++i) {
        const PopSceneDraw *draw = &room->middle[i];
        bool drawn = draw->params.mode == 2 ? middle_copy(background, draw) :
            pop_render_mode4(background, sizeof(background), draw->table,
                              draw->image_id, &draw->params);
        if (!drawn) return false;
    }
    const PopSceneLayers layers = {background, room->foreground_preserve,
                                   room->foreground_xor, POP_HGR_PAGE_BYTES};
    return pop_scene_compose(page, page_bytes, &layers, actors, actor_count);
}
