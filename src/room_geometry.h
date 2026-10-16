#ifndef POP_ROOM_GEOMETRY_H
#define POP_ROOM_GEOMETRY_H

#include "room_visual.h"

typedef struct PopGeometryOwner {
    uint16_t index;
    uint8_t original_type, replacement_type;
} PopGeometryOwner;

typedef struct PopGeometryVariant {
    uint32_t signature;
    uint16_t background_offset, background_count;
    uint16_t foreground_offset, foreground_count;
} PopGeometryVariant;

typedef struct PopGeometryGroup {
    uint32_t dependencies;
    uint16_t variant_offset;
    uint8_t variant_count;
} PopGeometryGroup;
/* Current/previous/below-left world cells consumed by the40 source groups.
 * A mutation outside this set cannot affect this room's visual program. */
bool pop_room_geometry_uses_world_cell(unsigned room,unsigned cell_index);

/* Pure source-local command selection for the17 first-level removable cells.
 * The caller validates unrelated world types/specs. This function admits only
 * original11->0 and original10/22->1, each with replacement spec0.
 * Source161E/1639 has40 ordered groups;1777 binds the below-left input.
 * Scratch commands hold owned data; the program borrows that caller storage. */
bool pop_room_geometry_program(const PopWorldState *world, unsigned room,
    PopRoomDrawTemplate *background, size_t background_capacity,
    PopForegroundDraw *foreground, size_t foreground_capacity,
    PopRoomVisualTemplate *program);

#endif
