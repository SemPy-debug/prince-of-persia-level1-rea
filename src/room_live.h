#ifndef POP_ROOM_LIVE_H
#define POP_ROOM_LIVE_H

#include "world_state.h"
#include "controller.h"
#include "room_scene.h"

/* Owned live command/operator storage, separate from the legacy visual ABI.
 * After copying this structure, its room pointers must be rebound to its own
 * arrays. pop_room_live_build performs that binding on successful return. */
typedef struct PopRoomLiveScene {
    PopRoomScene room;
    PopRoomBackgroundDraw background[200];
    uint8_t foreground_preserve[POP_HGR_PAGE_BYTES];
    uint8_t foreground_xor[POP_HGR_PAGE_BYTES];
    PopSceneDraw middle[45];
} PopRoomLiveScene;

/* Source1EDE projection. Link timers change the visible plate geometry;
 * persistent world types/specs remain untouched. */
bool pop_room_live_cell(const PopWorldState *world,const uint8_t links[256],
                        unsigned index,uint8_t *type,uint8_t *spec);

/* Pure source-native40-group room production. The canonical initial room
 * supplies only room identity; all mutable tiles, link projection, actor
 * context and palette come from the current inputs. Failure is atomic. */
bool pop_room_live_build(const PopWorldState *world,const uint8_t links[256],
                         const PopControllerState *state,
                         const PopRoomScene *initial_room,
                         PopRoomLiveScene *out);

#endif
