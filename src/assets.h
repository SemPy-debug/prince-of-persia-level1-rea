#ifndef POP_ASSETS_H
#define POP_ASSETS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum PopAssetTable {
    POP_ASSET_BACKGROUND_1 = 0,
    POP_ASSET_BACKGROUND_2 = 1,
    POP_ASSET_CHARACTER_0 = 2,
    POP_ASSET_CHARACTER_1 = 3,
    POP_ASSET_CHARACTER_2 = 4,
    POP_ASSET_CHARACTER_3 = 5,
    POP_ASSET_CHARACTER_4 = 6,
    POP_ASSET_TABLE_COUNT = 7
} PopAssetTable;

typedef struct PopAsset {
    uint8_t width_bytes;
    uint8_t height_rows;
    uint16_t original_address;
    size_t payload_length;
    const uint8_t *encoded_rows;
} PopAsset;

/* Original immutable source dots, one-based image IDs. Rows are bottom first;
 * each byte is encoded as bit7 set plus seven source dots, bit0 leftmost.
 * Invalid calls return false without changing outputs. These data/primitives
 * do not define final opacity, color, draw order, clipping or a compositor. */
unsigned pop_asset_count(PopAssetTable table);
bool pop_asset_get(PopAssetTable table, unsigned image_id, PopAsset *out);
bool pop_asset_source_byte(PopAssetTable table, unsigned image_id,
                           unsigned column, unsigned bottom_row, uint8_t *out);

bool pop_asset_shift7(uint8_t encoded, unsigned phase,
                      uint8_t *primary, uint8_t *carry);
bool pop_asset_mirror7(uint8_t encoded, uint8_t *out);
bool pop_asset_neighbor_mask7(uint8_t encoded, uint8_t *out);
bool pop_asset_edge_masks(unsigned phase, uint8_t *left, uint8_t *right);
/* Page1 HGR address for the leftmost byte on a scanline0..191. */
bool pop_asset_hgr_row_address(unsigned row, uint16_t *out);

#endif
