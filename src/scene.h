#ifndef POP_SCENE_H
#define POP_SCENE_H

#include "render.h"

typedef struct PopSceneLayers {
    const uint8_t *base;
    const uint8_t *foreground_preserve;
    const uint8_t *foreground_xor;
    size_t page_bytes;
} PopSceneLayers;

typedef struct PopSceneDraw {
    PopAssetTable table;
    unsigned image_id;
    PopRenderParams params;
} PopSceneDraw;

typedef struct PopSceneObservation {
    unsigned nominal_frame;
    uint8_t actor_record[16];
    PopSceneLayers layers;
    const PopSceneDraw *draws;
    size_t draw_count;
} PopSceneObservation;

/* This initial, naturally reached frame binds actor room1 and original AUX
 * layer boundaries0670/0673/0676. The foreground list was empty: its identity
 * transform does not establish general room foreground or dynamic mechanics. */
const PopSceneLayers *pop_scene_initial_layers(void);
const PopSceneDraw *pop_scene_initial_draws(size_t *count);

/* Explicit finite observation catalog from original right-hold/release input:
 * frames12/21/37, actor room1. The foreground data cover each frame's redraw
 * region. They are not interchangeable whole-room occlusion maps. */
size_t pop_scene_room1_observation_count(void);
const PopSceneObservation *pop_scene_room1_observation(size_t index);

/* Compose independently in native C: source-extracted pre-character base,
 * bounded mode04/84 masked and mode2 copy commands, then foreground transform.
 * A failed request leaves destination unchanged. The scene data representation
 * supports additional measured scenes; this function does not infer coverage. */
bool pop_scene_compose(uint8_t *page, size_t page_bytes,
                       const PopSceneLayers *layers,
                       const PopSceneDraw *draws, size_t draw_count);
bool pop_scene_foreground(uint8_t *page, size_t page_bytes,
                          const PopSceneLayers *layers);

#endif
