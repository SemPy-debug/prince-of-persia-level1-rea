#include "assets.h"

/* Generated only from verified original snapshot tables. The offset sentinel
 * bounds each table independently; no inactive scene table is linked. */
extern const PopAsset pop_level1_asset_records[];
extern const unsigned pop_level1_asset_offsets[POP_ASSET_TABLE_COUNT + 1];

unsigned pop_asset_count(PopAssetTable table) {
    if ((unsigned)table >= POP_ASSET_TABLE_COUNT) {
        return 0;
    }
    return pop_level1_asset_offsets[(unsigned)table + 1] -
           pop_level1_asset_offsets[(unsigned)table];
}

bool pop_asset_get(PopAssetTable table, unsigned image_id, PopAsset *out) {
    const unsigned count = pop_asset_count(table);
    if (out == NULL || image_id == 0 || image_id > count) {
        return false;
    }
    *out = pop_level1_asset_records[pop_level1_asset_offsets[(unsigned)table] +
                                  image_id - 1];
    return true;
}

bool pop_asset_source_byte(PopAssetTable table, unsigned image_id,
                           unsigned column, unsigned bottom_row, uint8_t *out) {
    PopAsset asset;
    size_t index;
    if (out == NULL || !pop_asset_get(table, image_id, &asset) ||
        column >= asset.width_bytes || bottom_row >= asset.height_rows) {
        return false;
    }
    index = (size_t)bottom_row * asset.width_bytes + column;
    if (index >= asset.payload_length) {
        return false;
    }
    *out = asset.encoded_rows[index];
    return true;
}

bool pop_asset_shift7(uint8_t encoded, unsigned phase,
                      uint8_t *primary, uint8_t *carry) {
    unsigned dots;
    if (primary == NULL || carry == NULL || primary == carry ||
        (encoded & 0x80u) == 0 || phase >= 7) {
        return false;
    }
    dots = encoded & 0x7fu;
    *primary = (uint8_t)(0x80u | ((dots << phase) & 0x7fu));
    *carry = (uint8_t)(dots >> (7u - phase));
    return true;
}

bool pop_asset_mirror7(uint8_t encoded, uint8_t *out) {
    unsigned reversed = 0;
    unsigned dots;
    unsigned bit;
    if (out == NULL || (encoded & 0x80u) == 0) {
        return false;
    }
    dots = encoded & 0x7fu;
    for (bit = 0; bit < 7; ++bit) {
        reversed |= ((dots >> bit) & 1u) << (6u - bit);
    }
    *out = (uint8_t)(reversed | 0x80u);
    return true;
}

bool pop_asset_neighbor_mask7(uint8_t encoded, uint8_t *out) {
    unsigned dots;
    if (out == NULL || (encoded & 0x80u) == 0) {
        return false;
    }
    dots = encoded & 0x7fu;
    *out = (uint8_t)(0x80u | (~(dots | (dots << 1) | (dots >> 1)) & 0x7fu));
    return true;
}

bool pop_asset_edge_masks(unsigned phase, uint8_t *left, uint8_t *right) {
    unsigned left_dots;
    if (left == NULL || right == NULL || left == right || phase >= 7) {
        return false;
    }
    left_dots = (1u << phase) - 1u;
    *left = (uint8_t)(0x80u | left_dots);
    *right = (uint8_t)(0xffu ^ left_dots);
    return true;
}

bool pop_asset_hgr_row_address(unsigned row, uint16_t *out) {
    if (out == NULL || row >= 192) {
        return false;
    }
    *out = (uint16_t)(0x2000u + ((row & 7u) << 10) +
                      (((row >> 3) & 7u) << 7) + ((row >> 6) * 40u));
    return true;
}
