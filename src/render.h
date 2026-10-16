#ifndef POP_RENDER_H
#define POP_RENDER_H

#include "assets.h"

enum { POP_HGR_PAGE_BYTES = 8192 };

typedef struct PopRenderParams {
    uint8_t x_byte;
    uint8_t bottom_y;
    uint8_t phase;
    uint8_t mode;
    uint8_t clip_top;
    uint8_t clip_left;
    uint8_t clip_right;
    uint8_t clip_bottom;
} PopRenderParams;

/* Source-backed masked HGR byte composition for original modes04/84 only.
 * The supplied page is the caller-selected8KB MAIN HGR page. Mode84 uses
 * x_byte as the right-exclusive byte anchor and mirrors bytes plus seven dots.
 * Clip right/bottom are exclusive. Invisible valid draws return true; invalid
 * calls return false without writing. This does not define RGB colors,
 * background mode2 drawing, list order, wipes or a complete frame renderer. */
bool pop_render_mode4(uint8_t *page, size_t page_bytes, PopAssetTable table,
                      unsigned image_id, const PopRenderParams *params);

/* Original EE09 mode2 raw background copy from the two AUX background tables.
 * Rows above scanline0 are clipped. This bounded native interface admits only
 * caller placements wholly inside the40-byte horizontal page and bottom<192;
 * it does not invent semantics for original writes outside the visible page. */
bool pop_render_background_mode2(uint8_t *page, size_t page_bytes,
                                 PopAssetTable table, unsigned image_id,
                                 uint8_t x_byte, uint8_t bottom_y);

/* EE06 phase0 raw AND/OR/copy used by the original kind80 falling-floor
 * object. Clip bounds are explicit; offscreen rows/columns are skipped.
 * Shifted or mirrored raw modes remain outside this entry point's contract. */
bool pop_render_raw012(uint8_t *page, size_t page_bytes, PopAssetTable table,
                       unsigned image_id, const PopRenderParams *params);
/* EE06 shifted AND/OR/copy and mirrored variants, used by explicit HUD glyph
 * commands. This bounded interface admits phases0..6 and modes0/1/2/80/81/82. */
bool pop_render_copy_or(uint8_t *page, size_t page_bytes, PopAssetTable table,
                        unsigned image_id, const PopRenderParams *params);

#endif
