#ifndef POP_FOREGROUND_H
#define POP_FOREGROUND_H
#include "assets.h"
typedef struct PopForegroundDraw {
    PopAssetTable table;
    unsigned image_id;
    uint8_t x_byte;
    uint8_t bottom_y;
    uint8_t mode;
} PopForegroundDraw;

/* Consume original unshifted foreground list commands. EE09 supplies raw
 * AND/OR/copy modes0/1/2; EE12 supplies F71E neighbor-clear-mask AND (mode4).
 * Top clipping is source-backed; this bounded API admits only horizontal
 * spans inside40 bytes. Invalid calls leave the page unchanged. */
bool pop_foreground_apply(uint8_t *page, size_t page_bytes,
                          const PopForegroundDraw *draws, size_t draw_count);
#endif
