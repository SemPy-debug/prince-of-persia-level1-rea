#ifndef POP_WORLD_STATE_H
#define POP_WORLD_STATE_H

#include "world.h"

/* Persistent level cells. The original link/room/info records remain immutable.
 * This is gameplay data, not an emulated address space. Source addresses are
 * accepted by the reader solely for the reconstructed helpers' data contract. */
typedef struct PopWorldState {
    uint8_t types[POP_WORLD_ROOM_COUNT * POP_WORLD_CELLS_PER_ROOM];
    uint8_t specs[POP_WORLD_ROOM_COUNT * POP_WORLD_CELLS_PER_ROOM];
} PopWorldState;

bool pop_world_reset(PopWorldState *world);
bool pop_world_cell(const PopWorldState *world, unsigned room, unsigned cell,
                    PopWorldCell *out);
bool pop_world_set_cell(PopWorldState *world, unsigned room, unsigned cell,
                        PopWorldCell value);
bool pop_world_read(void *world, uint16_t source_address, uint8_t *value);
/* Collision/effect callback; only owned type/spec cells are writable. Stage a
 * world copy with AUX when a larger operation has fallible external effects. */
bool pop_world_write(void *world, uint16_t source_address, uint8_t value);

#endif
