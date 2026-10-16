#include "geometry.h"
#include "assets.h"
#include "data.h"
#include <stddef.h>
#include <string.h>

static bool row_for_y(uint8_t y, uint8_t *row) {
    uint8_t threshold;
    int candidate;
    for (candidate = 3; candidate >= 0; --candidate) {
        if (!pop_data_read(NULL, (uint16_t)(0x1275u + (unsigned)candidate), &threshold)) return false;
        if (y >= threshold) break;
    }
    *row = (uint8_t)candidate;
    return true;
}

bool pop_geometry_bounds(uint8_t aux[256]) {
    /* AUX041E->0858/0845 selects these actual loaded character tables. */
    static const uint8_t table_high[5] = {0x60, 0x84, 0x08, 0x96, 0xa8};
    static const uint8_t table_bank[5] = {2, 2, 2, 3, 2};
    PopAsset image;
    uint8_t work[256], width_dots, left, right, top, row, edge;
    if (aux == NULL || aux[0x7a] >= 5 ||
        !pop_asset_get((PopAssetTable)(POP_ASSET_CHARACTER_0 + aux[0x7a]), aux[0x70] & 0x7fu, &image) ||
        !pop_data_read(NULL, (uint16_t)(0x1210u + image.width_bytes), &width_dots)) return false;
    memcpy(work, aux, sizeof(work));
    work[4] = work[0x70];
    work[7] = 0;
    work[8] = table_high[work[0x7a]];
    work[0x12] = table_bank[work[0x7a]];
    work[0xb5] = image.height_rows;
    work[0xb4] = (uint8_t)((uint8_t)(width_dots + 1u) >> 1);
    left = (uint8_t)(((unsigned)work[0x71] >> 1) | ((unsigned)(work[0x72] & 1u) << 7));
    left = (uint8_t)(left + 58u);
    if ((work[0x43] & 0x80u) == 0) left = (uint8_t)(left - work[0xb4]);
    right = (uint8_t)(left + work[0xb4]);
    top = (uint8_t)(work[0x73] - work[0xb5] + 1u);
    if (top >= 192) top = 0;
    work[0xb7] = left;
    work[0xb8] = right;
    work[0xb9] = top;
    if (!row_for_y(top, &row)) return false;
    work[0xbc] = row == 3 ? 0xff : row;
    if (!row_for_y(work[0x73], &work[0xbd]) ||
        !pop_data_read(NULL, (uint16_t)(0x1000u + left), &work[0xba]) ||
        !pop_data_read(NULL, (uint16_t)(0x1000u + right), &work[0xbb]) ||
        !pop_data_read(NULL, (uint16_t)(0x1100u + right), &work[3])) return false;
    edge = (work[0x85] & 0x20u) != 0 ? 3 : 0;
    work[0xe9] = edge;
    work[0xbe] = (uint8_t)(left + edge);
    work[0xbf] = (uint8_t)(right - edge);
    memcpy(aux, work, sizeof(work));
    return true;
}
