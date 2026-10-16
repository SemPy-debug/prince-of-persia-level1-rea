#ifndef POP_WORLD_H
#define POP_WORLD_H

#include <stdbool.h>
#include <stdint.h>

enum {
    POP_WORLD_ROOM_COUNT = 24,
    POP_WORLD_CELLS_PER_ROOM = 30,
    POP_WORLD_LINK_COUNT = 256,
    POP_WORLD_INFO_BYTES = 256,
    POP_WORLD_BLUEPRINT_BYTES = 2304
};

typedef enum PopWorldDirection {
    POP_WORLD_LEFT = 0,
    POP_WORLD_RIGHT = 1,
    POP_WORLD_ABOVE = 2,
    POP_WORLD_BELOW = 3
} PopWorldDirection;

typedef struct PopWorldCell {
    uint8_t raw_type;
    uint8_t spec;
} PopWorldCell;

typedef struct PopWorldRawLink {
    uint8_t location;
    uint8_t map;
} PopWorldRawLink;

/* Immutable original initial bytes, not mutable gameplay state. Rooms are
 * numbered 1..24, cells 0..29. A stored neighbor of zero is a source sentinel,
 * not an accepted room argument. No reachability or reciprocal-link repair is
 * implied. Invalid arguments return false without changing the output. */
bool pop_world_initial_cell(unsigned room, unsigned cell, PopWorldCell *out);
bool pop_world_initial_type(unsigned room, unsigned cell, uint8_t *out);
bool pop_world_initial_neighbor(unsigned room, PopWorldDirection direction, uint8_t *out);
bool pop_world_initial_link(unsigned index, PopWorldRawLink *out);
bool pop_world_initial_info(unsigned offset, uint8_t *out);
bool pop_world_initial_byte(unsigned offset, uint8_t *out);
const char *pop_world_initial_sha256(void);

#endif
