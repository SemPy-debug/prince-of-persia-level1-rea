/* Original source HUD data only; REA4FEC/50C1/506A consumers. */
#ifndef POP_HUD_DATA_H
#define POP_HUD_DATA_H
#include <stdint.h>
static const uint8_t pop_health_x[12] = {0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12};
static const uint8_t pop_health_phase[12] = {0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 3, 4};
static const uint8_t pop_guard_health_x[12] = {39, 38, 37, 36, 35, 34, 32, 31, 30, 29, 28, 27};
static const uint8_t pop_guard_health_phase[12] = {5, 4, 3, 2, 1, 0, 6, 5, 4, 3, 2, 1};
static const uint8_t pop_health_chunk[4] = {1, 137, 138, 139};
static const uint8_t pop_hud_descriptors[24] = {73, 13, 0, 124, 90, 15, 0, 123, 85, 16, 3, 122, 89, 13, 0, 126, 90, 11, 0, 125, 85, 14, 0, 127};
static const uint8_t pop_level_tens[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 113, 113, 113};
static const uint8_t pop_level_ones[13] = {112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 112, 113, 114};
#endif
