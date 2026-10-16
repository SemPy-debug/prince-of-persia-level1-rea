#ifndef POP_ROOM_VISUAL_H
#define POP_ROOM_VISUAL_H

#include "room_scene.h"
#include "foreground.h"
#include "world_state.h"

typedef struct PopRoomDrawTemplate {
    PopRoomBackgroundDraw draw;
    uint8_t operation; /*0fixed,1face,2bottom,3looseedge,4torch,5gatebody,6gateunderlay*/
    uint8_t owner_room;
    uint8_t owner_cell;
    uint8_t floor_y;
    uint8_t clip_top;
} PopRoomDrawTemplate;

typedef struct PopRoomVisualTemplate {
    unsigned room_id;
    const PopRoomDrawTemplate *draws;
    size_t draw_count;
    const PopForegroundDraw *foreground;
    size_t foreground_count;
} PopRoomVisualTemplate;

typedef struct PopRoomVisualScene {
    PopRoomScene room;
    PopRoomBackgroundDraw background[200];
    uint8_t foreground_preserve[POP_HGR_PAGE_BYTES];
    uint8_t foreground_xor[POP_HGR_PAGE_BYTES];
} PopRoomVisualScene;

/* Translate the live source torch-spec state to explicit room commands.
 * 18AE/1C09 uses the previous tile (left neighbor across a room edge), and
 * 51B0 selects the18-entry image table. All41 flame commands in the24 measured
 * room programs have corroborated ownership. Current type geometry and all
 * non-torch specs must still match the measured initial catalog. Unknown
 * changes fail without partially writing states. Timer progression is owned
 * by simulation; this function only maps the current live state to images. */
bool pop_room_visual_states(const PopWorldState *world, const PopRoomScene *room,
                            PopRoomCommandState *states, size_t state_count);

/* Rebuild accepted ordered commands from original raw append attempts. This
 * admits newly visible loose-floor edge draws at the top of a room, which
 * cannot be represented by changing a fixed-size accepted-command array.
 * The returned scene owns its command array; do not memcpy it and retain the
 * old internal pointers. Foreground commands are independently executed on
 * identity/zero inputs to form exact bitwise operators, rather than using a
 * frozen reference framebuffer. Unknown geometry/specs fail atomically. */
bool pop_room_visual_build(const PopWorldState *world, const PopRoomScene *room,
                           PopRoomVisualScene *out);

#endif
