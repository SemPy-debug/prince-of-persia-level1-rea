#ifndef POP_OBJECT_DRAW_H
#define POP_OBJECT_DRAW_H
#include "scene.h"

typedef struct PopDrawableObject {
    uint8_t kind, depth, bottom_y;
    uint8_t draw_count;
    PopSceneDraw draws[4];
} PopDrawableObject;

/* Source1544/16F8 visits30,20..29,10..19,0..9,255. Within a cell, the
 * actual1F1A/1F44 performs one descending-Y adjacent pass from right to left;
 * its source F4 repeat flag is never changed. Later kind1 blocks a swap.
 * This intentionally reproduces that finite ordering, not a generic sort. */
bool pop_object_draw_order(const PopDrawableObject *objects, size_t object_count,
                           PopSceneDraw *draws, size_t capacity, size_t *count);
#endif
