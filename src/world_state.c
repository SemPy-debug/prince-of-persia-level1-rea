#include "world_state.h"
#include "data.h"
#include <stddef.h>

enum { CELLS = POP_WORLD_ROOM_COUNT * POP_WORLD_CELLS_PER_ROOM };

static bool valid_cell(unsigned room, unsigned cell) {
    return room >= 1 && room <= POP_WORLD_ROOM_COUNT && cell < POP_WORLD_CELLS_PER_ROOM;
}

bool pop_world_reset(PopWorldState *world) {
    if (world == NULL) return false;
    for (unsigned i = 0; i < CELLS; ++i) {
        (void)pop_world_initial_byte(i, &world->types[i]);
        (void)pop_world_initial_byte(CELLS + i, &world->specs[i]);
    }
    return true;
}
bool pop_world_cell(const PopWorldState *world, unsigned room, unsigned cell,
                    PopWorldCell *out) {
    unsigned index;
    if (world == NULL || out == NULL || !valid_cell(room, cell)) return false;
    index = (room - 1) * POP_WORLD_CELLS_PER_ROOM + cell;
    out->raw_type = world->types[index];
    out->spec = world->specs[index];
    return true;
}
bool pop_world_set_cell(PopWorldState *world, unsigned room, unsigned cell,
                        PopWorldCell value) {
    unsigned index;
    if (world == NULL || !valid_cell(room, cell)) return false;
    index = (room - 1) * POP_WORLD_CELLS_PER_ROOM + cell;
    world->types[index] = value.raw_type;
    world->specs[index] = value.spec;
    return true;
}
bool pop_world_read(void *world, uint16_t address, uint8_t *value) {
    const PopWorldState *state = world;
    if (value == NULL) return false;
    if (address < 0xb700 || address >= 0xc000) return pop_data_read(NULL, address, value);
    if (address < 0xbca0) {
        if (state == NULL) return false;
        if (address < 0xb9d0) *value = state->types[address - 0xb700u];
        else *value = state->specs[address - 0xb9d0u];
        return true;
    }
    return pop_world_initial_byte(address - 0xb700u, value);
}

bool pop_world_write(void *world, uint16_t address, uint8_t value) {
    PopWorldState *state = world;
    if (state == NULL || address < 0xb700 || address >= 0xbca0) return false;
    if (address < 0xb9d0) state->types[address - 0xb700u] = value;
    else state->specs[address - 0xb9d0u] = value;
    return true;
}
