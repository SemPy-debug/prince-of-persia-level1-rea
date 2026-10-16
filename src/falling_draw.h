#ifndef POP_FALLING_DRAW_H
#define POP_FALLING_DRAW_H
#include "scene.h"

typedef struct PopFallingObject {
    bool visible;
    uint8_t depth;
    uint8_t bottom_y;
    PopSceneDraw draws[4];
} PopFallingObject;

/* Pure F773/F78B -> kind80/1E7C visual descriptor. Records are the six
 * source B660/B670/B680/B690/B6A0/B6B0 bytes, not machine instructions.
 * Source count/order is handled by the caller. Unsupported active states
 * are not invented; the original renderer skips nonzero state itself. */
bool pop_falling_draw(const uint8_t record[6], uint8_t display_room,
                      uint8_t below_room, PopFallingObject *out);
#endif
