#include "object_draw.h"
#include <string.h>

bool pop_object_draw_order(const PopDrawableObject *objects, size_t object_count,
                           PopSceneDraw *draws, size_t capacity, size_t *count)
{
    if ((object_count != 0 && objects == NULL) || object_count > 20 ||
        draws == NULL || count == NULL) return false;
    size_t required = 0;
    for (size_t i = 0; i < object_count; ++i) {
        if (objects[i].draw_count > 4) return false;
        required += objects[i].draw_count;
    }
    if (required > capacity) return false;
    size_t output_count = 0;
    for (unsigned visit = 0; visit < 32; ++visit) {
        const uint8_t depth = visit == 0 ? 30 : visit == 31 ? 255 :
            visit <= 10 ? (uint8_t)(19u + visit) : visit <= 20 ?
            (uint8_t)(visit - 1u) : (uint8_t)(visit - 21u);
        size_t indices[20], n = 0;
        for (size_t i = 0; i < object_count; ++i)
            if (objects[i].depth == depth) indices[n++] = i;
        for (size_t i = n; i > 1; --i) {
            const size_t later = indices[i - 1], earlier = indices[i - 2];
            if (objects[later].kind != 1 &&
                objects[later].bottom_y > objects[earlier].bottom_y) {
                indices[i - 2] = later;
                indices[i - 1] = earlier;
            }
        }
        for (size_t i = 0; i < n; ++i) {
            const PopDrawableObject *object = &objects[indices[i]];
            memcpy(draws + output_count, object->draws,
                   object->draw_count * sizeof(*draws));
            output_count += object->draw_count;
        }
    }
    *count = output_count;
    return true;
}
