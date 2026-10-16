#include "scene.h"

#include <string.h>

extern const PopSceneLayers pop_initial_scene_layers;
extern const PopSceneDraw pop_initial_scene_draw_commands[];
extern const size_t pop_initial_scene_draw_count;

static bool valid_layers(const PopSceneLayers *layers)
{
    return layers != NULL && layers->page_bytes == POP_HGR_PAGE_BYTES &&
           layers->base != NULL && layers->foreground_preserve != NULL &&
           layers->foreground_xor != NULL;
}

const PopSceneLayers *pop_scene_initial_layers(void)
{
    return &pop_initial_scene_layers;
}

const PopSceneDraw *pop_scene_initial_draws(size_t *count)
{
    if (count != NULL) {
        *count = pop_initial_scene_draw_count;
    }
    return pop_initial_scene_draw_commands;
}

bool pop_scene_foreground(uint8_t *page, size_t page_bytes,
                          const PopSceneLayers *layers)
{
    if (page == NULL || page_bytes != POP_HGR_PAGE_BYTES || !valid_layers(layers)) {
        return false;
    }
    for (size_t i = 0; i < POP_HGR_PAGE_BYTES; ++i) {
        page[i] = (uint8_t)((page[i] & layers->foreground_preserve[i]) ^
                            layers->foreground_xor[i]);
    }
    return true;
}

bool pop_scene_compose(uint8_t *page, size_t page_bytes,
                       const PopSceneLayers *layers,
                       const PopSceneDraw *draws, size_t draw_count)
{
    uint8_t composed[POP_HGR_PAGE_BYTES];
    if (page == NULL || page_bytes != POP_HGR_PAGE_BYTES || !valid_layers(layers) ||
        (draw_count != 0 && draws == NULL) || draw_count > 256) {
        return false;
    }
    memcpy(composed, layers->base, sizeof(composed));
    for (size_t i = 0; i < draw_count; ++i) {
        bool drawn;
        if (draws[i].params.mode <= 2 && draws[i].params.phase == 0 &&
            draws[i].params.clip_bottom != 0) {
            drawn = pop_render_raw012(composed, sizeof(composed), draws[i].table,
                draws[i].image_id, &draws[i].params);
        } else if (draws[i].params.mode == 2) {
            drawn = pop_render_background_mode2(composed, sizeof(composed),
                draws[i].table, draws[i].image_id, draws[i].params.x_byte,
                draws[i].params.bottom_y);
        } else {
            drawn = pop_render_mode4(composed, sizeof(composed), draws[i].table,
                draws[i].image_id, &draws[i].params);
        }
        if (!drawn) {
            return false;
        }
    }
    if (!pop_scene_foreground(composed, sizeof(composed), layers)) {
        return false;
    }
    memcpy(page, composed, sizeof(composed));
    return true;
}
