#include "foreground.h"
#include "render.h"
#include <string.h>

bool pop_foreground_apply(uint8_t *page, size_t page_bytes,
                          const PopForegroundDraw *draws, size_t draw_count)
{
    uint8_t output[POP_HGR_PAGE_BYTES];
    if (page == NULL || page_bytes != POP_HGR_PAGE_BYTES || draw_count >= 100 ||
        (draw_count != 0 && draws == NULL)) return false;
    memcpy(output, page, sizeof(output));
    for (size_t i = 0; i < draw_count; ++i) {
        const PopForegroundDraw *draw = &draws[i];
        PopAsset asset;
        if ((draw->table != POP_ASSET_BACKGROUND_1 && draw->table != POP_ASSET_BACKGROUND_2) ||
            (draw->mode > 2 && draw->mode != 4) || draw->bottom_y >= 192 ||
            !pop_asset_get(draw->table, draw->image_id, &asset) ||
            (unsigned)draw->x_byte + asset.width_bytes > 40) return false;
        for (unsigned row = 0; row < asset.height_rows && row <= draw->bottom_y; ++row) {
            uint16_t address;
            (void)pop_asset_hgr_row_address((unsigned)draw->bottom_y - row, &address);
            for (unsigned column = 0; column < asset.width_bytes; ++column) {
                uint8_t *dest = &output[(size_t)address - 0x2000u + draw->x_byte + column];
                uint8_t value = asset.encoded_rows[(size_t)row * asset.width_bytes + column];
                if (draw->mode == 4 && !pop_asset_neighbor_mask7(value, &value)) return false;
                if (draw->mode == 0 || draw->mode == 4) *dest &= value;
                else if (draw->mode == 1) *dest |= value;
                else *dest = value;
            }
        }
    }
    memcpy(page, output, sizeof(output));
    return true;
}
