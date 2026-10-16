/* Typed original tables only; no original instructions. */
#ifndef POP_TILE_DATA_H
#define POP_TILE_DATA_H
#include <stdint.h>
static const uint8_t tile_base[30] = {0, 3, 3, 3, 3, 3, 3, 3, 3, 0, 3, 3, 0, 3, 3, 3, 3, 0, 0, 3, 0, 3, 0, 3, 0, 3, 0, 0, 0, 0};
static const uint8_t tile_face[30] = {0, 1, 5, 7, 10, 1, 1, 10, 16, 0, 1, 0, 0, 20, 32, 75, 1, 0, 0, 1, 0, 151, 0, 1, 0, 167, 169, 170, 172, 173};
static const uint8_t tile_face_y[30] = {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 252, 252, 252};
static const uint8_t tile_below_overlay[30] = {0, 4, 4, 4, 4, 4, 4, 0, 4, 0, 4, 0, 0, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0, 4, 4, 0, 0, 0, 0};
static const uint8_t tile_previous_face[30] = {0, 2, 6, 8, 11, 27, 2, 158, 26, 28, 2, 0, 158, 74, 33, 27, 77, 78, 2, 81, 132, 152, 2, 145, 146, 2, 0, 0, 0, 0};
static const uint8_t tile_previous_y[30] = {0, 0, 0, 0, 0, 1, 0, 3, 0, 3, 0, 0, 3, 0, 0, 255, 0, 0, 0, 255, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t tile_previous_extra[30] = {0, 71, 71, 0, 0, 71, 71, 0, 0, 0, 71, 71, 0, 0, 71, 71, 0, 0, 71, 0, 0, 0, 71, 0, 0, 71, 0, 0, 0, 0};
static const uint8_t tile_below_image[30] = {0, 0, 0, 9, 12, 0, 0, 159, 0, 29, 0, 0, 159, 0, 0, 0, 79, 80, 0, 0, 133, 0, 0, 147, 148, 0, 0, 0, 0, 0};
static const uint8_t tile_bottom[30] = {0, 21, 21, 21, 21, 24, 25, 22, 21, 0, 21, 0, 23, 21, 46, 76, 21, 21, 21, 21, 134, 21, 21, 21, 21, 21, 171, 0, 0, 0};
static const uint8_t tile_foreground[30] = {0, 0, 0, 69, 70, 0, 0, 70, 72, 73, 135, 0, 70, 15, 19, 0, 0, 0, 0, 0, 131, 0, 0, 0, 0, 168, 0, 174, 174, 174};
static const uint8_t tile_foreground_y[30] = {0, 0, 0, 255, 0, 0, 0, 0, 255, 3, 253, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 0, 220, 220, 220};
static const uint8_t tile_foreground_x[30] = {0, 0, 0, 1, 3, 0, 0, 3, 1, 1, 2, 0, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0};
static const uint8_t tile_gate_remainder[8] = {47, 48, 49, 50, 51, 52, 53, 54};
static const uint8_t tile_gate_partial[9] = {54, 62, 61, 60, 59, 58, 57, 56, 55};
static const uint8_t tile_spike_current[10] = {0, 34, 36, 38, 40, 42, 40, 36, 34, 0};
static const uint8_t tile_spike_previous[10] = {0, 35, 37, 39, 41, 43, 41, 37, 35, 0};
static const uint8_t tile_loose_face[11] = {1, 30, 1, 31, 31, 1, 1, 1, 31, 31, 31};
static const uint8_t tile_loose_offset[11] = {0, 1, 0, 255, 255, 0, 0, 0, 255, 255, 255};
static const uint8_t tile_loose_bottom[11] = {21, 44, 21, 45, 45, 21, 21, 21, 45, 45, 45};
static const uint8_t tile_previous_variant[3] = {158, 154, 129};
static const uint8_t tile_below_variant[3] = {159, 155, 130};
static const uint8_t tile_empty_previous[4] = {0, 163, 165, 166};
static const uint8_t tile_empty_previous_y[4] = {0, 236, 236, 0};
static const uint8_t tile_floor_previous[4] = {2, 162, 164, 164};
static const uint8_t tile_floor_previous_y[4] = {0, 0, 0, 0};
static const uint8_t tile_wall_previous[2] = {132, 111};
static const uint8_t tile_wall_below[2] = {133, 133};
static const uint8_t tile_wall_bottom[2] = {134, 134};
static const uint8_t tile_wall_foreground[2] = {131, 131};
static const uint8_t tile_torch[18] = {82, 83, 84, 85, 86, 97, 98, 99, 100, 82, 84, 86, 99, 97, 85, 83, 100, 98};
static const uint8_t tile_potion[9] = {178, 175, 176, 177, 176, 175, 177, 176, 175};
#endif
