#include "render.h"

#include <string.h>

static bool visible_column(int column, const PopRenderParams *params) {
    return column >= params->clip_left && column < params->clip_right;
}

bool pop_render_mode4(uint8_t *page, size_t page_bytes, PopAssetTable table,
                      unsigned image_id, const PopRenderParams *params) {
    PopAsset asset;
    uint8_t left_mask, right_mask;
    bool mirror;
    unsigned wrapped_left;
    int left;
    unsigned source_row;
    if (page == NULL || page_bytes < POP_HGR_PAGE_BYTES || params == NULL ||
        !pop_asset_get(table, image_id, &asset) || params->phase >= 7 ||
        (params->mode != 4 && params->mode != 0x84) ||
        params->clip_left > params->clip_right || params->clip_right > 40 ||
        params->clip_top > params->clip_bottom || params->clip_bottom > 192) {
        return false;
    }
    mirror = (params->mode & 0x80u) != 0;
    wrapped_left = ((unsigned)params->x_byte -
                    (mirror ? asset.width_bytes : 0u)) & 0xffu;
    left = wrapped_left < 128 ? (int)wrapped_left : (int)wrapped_left - 256;
    (void)pop_asset_edge_masks(params->phase, &left_mask, &right_mask);
    for (source_row = 0; source_row < asset.height_rows; ++source_row) {
        const int y = (int)params->bottom_y - (int)source_row;
        uint16_t row_address;
        size_t row_offset;
        uint8_t carried_dots = 0;
        uint8_t carried_mask = left_mask;
        unsigned output_column;
        if (y < params->clip_top || y >= params->clip_bottom) {
            continue;
        }
        (void)pop_asset_hgr_row_address((unsigned)y, &row_address);
        row_offset = (size_t)row_address - 0x2000u;
        if (visible_column(left, params)) {
            carried_dots = page[row_offset + (unsigned)left] & left_mask;
        }
        for (output_column = 0; output_column < asset.width_bytes; ++output_column) {
            const unsigned input_column = mirror ?
                asset.width_bytes - output_column - 1 : output_column;
            const int x = left + (int)output_column;
            uint8_t dots = asset.encoded_rows[(size_t)source_row *
                                             asset.width_bytes + input_column];
            uint8_t mask;
            uint8_t primary_dots, next_dots, primary_mask, next_mask;
            if (mirror) {
                (void)pop_asset_mirror7(dots, &dots);
            }
            (void)pop_asset_neighbor_mask7(dots, &mask);
            (void)pop_asset_shift7(dots, params->phase, &primary_dots, &next_dots);
            (void)pop_asset_shift7(mask, params->phase, &primary_mask, &next_mask);
            if (visible_column(x, params)) {
                uint8_t *dest = &page[row_offset + (unsigned)x];
                *dest = (uint8_t)((*dest & (primary_mask | carried_mask)) |
                                  primary_dots | carried_dots);
            }
            carried_dots = next_dots;
            carried_mask = next_mask;
        }
        if (visible_column(left + asset.width_bytes, params)) {
            uint8_t *dest = &page[row_offset + (unsigned)(left + asset.width_bytes)];
            *dest = (uint8_t)((*dest & (right_mask | carried_mask)) | carried_dots);
        }
    }
    return true;
}

bool pop_render_background_mode2(uint8_t *page, size_t page_bytes,
                                 PopAssetTable table, unsigned image_id,
                                 uint8_t x_byte, uint8_t bottom_y) {
    PopAsset asset;
    unsigned source_row;
    if (page == NULL || page_bytes < POP_HGR_PAGE_BYTES || bottom_y >= 192 ||
        (table != POP_ASSET_BACKGROUND_1 && table != POP_ASSET_BACKGROUND_2) ||
        !pop_asset_get(table, image_id, &asset) ||
        (unsigned)x_byte + asset.width_bytes > 40) {
        return false;
    }
    for (source_row = 0; source_row < asset.height_rows && source_row <= bottom_y;
         ++source_row) {
        uint16_t row_address;
        const unsigned y = (unsigned)bottom_y - source_row;
        (void)pop_asset_hgr_row_address(y, &row_address);
        memcpy(page + ((size_t)row_address - 0x2000u) + x_byte,
               asset.encoded_rows + (size_t)source_row * asset.width_bytes,
               asset.width_bytes);
    }
    return true;
}

bool pop_render_raw012(uint8_t *page, size_t page_bytes, PopAssetTable table,
                       unsigned image_id, const PopRenderParams *params)
{
    PopAsset asset;
    if (page == NULL || page_bytes != POP_HGR_PAGE_BYTES || params == NULL ||
        params->phase != 0 || params->mode > 2 ||
        params->clip_left > params->clip_right || params->clip_right > 40 ||
        params->clip_top > params->clip_bottom || params->clip_bottom > 192 ||
        (table != POP_ASSET_BACKGROUND_1 && table != POP_ASSET_BACKGROUND_2) ||
        !pop_asset_get(table, image_id, &asset)) return false;
    const int left = params->x_byte < 128 ? params->x_byte :
                     (int)params->x_byte - 256;
    for (unsigned row = 0; row < asset.height_rows; ++row) {
        const int y = (int)params->bottom_y - (int)row;
        if (y < params->clip_top || y >= params->clip_bottom) continue;
        uint16_t address;
        (void)pop_asset_hgr_row_address((unsigned)y, &address);
        const size_t offset = (size_t)address - 0x2000u;
        for (unsigned column = 0; column < asset.width_bytes; ++column) {
            const int x = left + (int)column;
            if (x < params->clip_left || x >= params->clip_right) continue;
            uint8_t *dest = &page[offset + (unsigned)x];
            const uint8_t value = asset.encoded_rows[(size_t)row *
                                                      asset.width_bytes + column];
            if (params->mode == 0) *dest &= value;
            else if (params->mode == 1) *dest |= value;
            else *dest = value;
        }
    }
    return true;
}

bool pop_render_copy_or(uint8_t *page, size_t page_bytes, PopAssetTable table,
                        unsigned image_id, const PopRenderParams *params)
{
    PopAsset asset;
    uint8_t left_mask, right_mask;
    const unsigned mode = params == NULL ? 255u : params->mode & 127u;
    if (page == NULL || page_bytes != POP_HGR_PAGE_BYTES || params == NULL ||
        mode > 2 || params->phase >= 7 ||
        params->clip_left > params->clip_right || params->clip_right > 40 ||
        params->clip_top > params->clip_bottom || params->clip_bottom > 192 ||
        !pop_asset_get(table, image_id, &asset)) return false;
    (void)pop_asset_edge_masks(params->phase, &left_mask, &right_mask);
    const bool mirror = (params->mode & 128u) != 0;
    const uint8_t anchor = (uint8_t)(params->x_byte - (mirror ? asset.width_bytes : 0u));
    const int left = anchor < 128 ? anchor : (int)anchor - 256;
    for (unsigned row = 0; row < asset.height_rows; ++row) {
        const int y = (int)params->bottom_y - (int)row;
        if (y < params->clip_top || y >= params->clip_bottom) continue;
        uint16_t address;
        (void)pop_asset_hgr_row_address((unsigned)y, &address);
        const size_t offset = (size_t)address - 0x2000u;
        uint8_t carry = 0;
        if (mode == 0) carry = left_mask;
        if (mode == 2 && visible_column(left, params))
            carry = page[offset + (unsigned)left] & left_mask;
        for (unsigned col = 0; col < asset.width_bytes; ++col) {
            uint8_t primary, next;
            const unsigned input_column = mirror ? asset.width_bytes - col - 1u : col;
            uint8_t source = asset.encoded_rows[(size_t)row * asset.width_bytes + input_column];
            if (mirror) (void)pop_asset_mirror7(source,&source);
            (void)pop_asset_shift7(source, params->phase, &primary, &next);
            const int x = left + (int)col;
            if (visible_column(x, params)) {
                uint8_t *dest = &page[offset + (unsigned)x];
                const uint8_t value = primary | carry;
                if (mode == 0) *dest &= value;
                else if (mode == 1) *dest |= value;
                else *dest = value;
            }
            carry = next;
        }
        const int tail = left + asset.width_bytes;
        if (visible_column(tail, params)) {
            uint8_t *dest = &page[offset + (unsigned)tail];
            if (mode == 0) *dest &= (uint8_t)(right_mask | carry);
            else if (mode == 1) *dest |= carry;
            else *dest = (uint8_t)((*dest & right_mask) | carry);
        }
    }
    return true;
}
