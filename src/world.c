#include "world.h"

/* These are generated directly from the immutable fresh original capture. */
extern const uint8_t pop_world_level1_blueprint_data[POP_WORLD_BLUEPRINT_BYTES];
extern const char pop_world_level1_blueprint_sha256[];

enum {
    BLUE_TYPE = 0,
    BLUE_SPEC = 0x2d0,
    LINK_LOC = 0x5a0,
    LINK_MAP = 0x6a0,
    ROOM_MAP = 0x7a0,
    INFO = 0x800
};

static bool valid_cell(unsigned room, unsigned cell) {
    return room >= 1 && room <= POP_WORLD_ROOM_COUNT && cell < POP_WORLD_CELLS_PER_ROOM;
}

bool pop_world_initial_cell(unsigned room, unsigned cell, PopWorldCell *out) {
    unsigned offset;
    if (!out || !valid_cell(room, cell)) return false;
    offset = (room - 1) * POP_WORLD_CELLS_PER_ROOM + cell;
    out->raw_type = pop_world_level1_blueprint_data[BLUE_TYPE + offset];
    out->spec = pop_world_level1_blueprint_data[BLUE_SPEC + offset];
    return true;
}
bool pop_world_initial_type(unsigned room, unsigned cell, uint8_t *out) {
    PopWorldCell source;
    if (!out || !pop_world_initial_cell(room, cell, &source)) return false;
    /* Actual original AUX 1F69 LDA ($24),Y; 1F6B AND #$1F. */
    *out = (uint8_t)(source.raw_type & 0x1fu);
    return true;
}
bool pop_world_initial_neighbor(unsigned room, PopWorldDirection direction, uint8_t *out) {
    if (!out || room < 1 || room > POP_WORLD_ROOM_COUNT || (unsigned)direction >= 4) return false;
    /* Observed one-based screen lookup is BE9C + 4*room + direction. */
    *out = pop_world_level1_blueprint_data[ROOM_MAP + (room - 1) * 4 + (unsigned)direction];
    return true;
}
bool pop_world_initial_link(unsigned index, PopWorldRawLink *out) {
    if (!out || index >= POP_WORLD_LINK_COUNT) return false;
    out->location = pop_world_level1_blueprint_data[LINK_LOC + index];
    out->map = pop_world_level1_blueprint_data[LINK_MAP + index];
    return true;
}
bool pop_world_initial_info(unsigned offset, uint8_t *out) {
    if (!out || offset >= POP_WORLD_INFO_BYTES) return false;
    *out = pop_world_level1_blueprint_data[INFO + offset];
    return true;
}
bool pop_world_initial_byte(unsigned offset, uint8_t *out) {
    if (!out || offset >= POP_WORLD_BLUEPRINT_BYTES) return false;
    *out = pop_world_level1_blueprint_data[offset];
    return true;
}
const char *pop_world_initial_sha256(void) { return pop_world_level1_blueprint_sha256; }
