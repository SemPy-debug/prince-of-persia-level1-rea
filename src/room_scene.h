#ifndef POP_ROOM_SCENE_H
#define POP_ROOM_SCENE_H

#include "scene.h"

/* Families refer to named original image slots, not guessed tile-type IDs. */
typedef enum PopRoomDynamicFamily {
    POP_ROOM_STATIC = 0,
    POP_ROOM_FLAME_IMAGES = 1,
    POP_ROOM_GATE_IMAGES = 2,
    POP_ROOM_LOOSE_FLOOR_IMAGES = 3,
    POP_ROOM_SPIKE_IMAGES = 4
} PopRoomDynamicFamily;

typedef struct PopRoomBackgroundDraw {
    PopAssetTable table;
    unsigned image_id;
    uint8_t x_byte;
    uint8_t bottom_y;
    uint8_t mode;
    uint8_t producer_cell;
    uint8_t source_type;
    uint8_t previous_type;
    uint8_t dynamic_family;
} PopRoomBackgroundDraw;

typedef struct PopRoomCommandState {
    bool enabled;
    unsigned image_id;
    uint8_t x_byte;
    uint8_t bottom_y;
    uint8_t mode;
} PopRoomCommandState;

typedef struct PopRoomScene {
    unsigned room_id;
    const PopRoomBackgroundDraw *background;
    size_t background_count;
    const uint8_t *foreground_preserve;
    const uint8_t *foreground_xor;
    const PopSceneDraw *middle;
    size_t middle_count;
    size_t page_bytes;
} PopRoomScene;

/* Source-backed initial room visual state. Missing rooms return NULL.
 * Foreground covers all30 requested tile redraws for the
 * source state. Alternate moving-geometry foreground states require their own
 * measured data; this interface does not claim those controllers are complete. */
const PopRoomScene *pop_room_scene_initial(unsigned room);
bool pop_room_default_state(const PopRoomScene *room, size_t index,
                            PopRoomCommandState *out);

/* Retains original ordered background commands. Dynamic image operations are
 * explicit state, never baked pixels. NULL/0 uses recorded source defaults;
 * supplied state arrays must cover every command. Static commands stay fixed.
 * Middle visual commands (such as original potion bubbles) are explicit data;
 * callers can use a copied scene descriptor with an updated middle command
 * array for a separately established visual state. They are drawn before actors.
 * Visual state overrides are rendering inputs, not proof of gameplay legality.
 * Invalid requests leave the caller's destination unchanged. */
bool pop_room_background(uint8_t *page, size_t page_bytes,
                         const PopRoomScene *room,
                         const PopRoomCommandState *states, size_t state_count);
bool pop_room_foreground(uint8_t *page, size_t page_bytes,
                         const PopRoomScene *room);
bool pop_room_compose(uint8_t *page, size_t page_bytes,
                      const PopRoomScene *room,
                      const PopRoomCommandState *states, size_t state_count,
                      const PopSceneDraw *actors, size_t actor_count);

#endif
