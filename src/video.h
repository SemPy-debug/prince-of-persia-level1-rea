#ifndef POP_VIDEO_H
#define POP_VIDEO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { POP_VIDEO_WIDTH = 560, POP_VIDEO_HEIGHT = 192,
       POP_VIDEO_RGBA_BYTES = POP_VIDEO_WIDTH * POP_VIDEO_HEIGHT * 4 };

/* Decode the native game's standard color HGR page into opaque RGBA, two
 * horizontal samples per source dot. This is a display converter only; it
 * does not emulate a CPU, device, alternate Apple video mode or CRT waveform.
 * Palette/neighbor behavior is compared with the original reference emulator.
 * Output is contiguous 560x192 RGBA. Input/output spans must not overlap.
 * Invalid spans return false without writes. */
bool pop_video_hgr_rgba(const uint8_t *page, size_t page_bytes,
                        uint8_t *rgba, size_t rgba_bytes);
/* Source E003 fills the40x24 text-page cells with one byte in lores mode.
 * Its low/high nibbles select alternating four-scanline color rows. This
 * typed fill converter has no device memory or CPU execution dependency. */
bool pop_video_lores_fill_rgba(uint8_t fill_byte,uint8_t *rgba,size_t rgba_bytes);

#endif
