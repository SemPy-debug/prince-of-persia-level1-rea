#include "data.h"
#include <stddef.h>

extern const uint8_t pop_data_lookup[656];
extern const uint8_t pop_data_animation[4608];
extern const uint8_t pop_data_wall_offsets[10];
extern const uint8_t pop_data_guard_parameters[124];
extern const uint8_t pop_data_sentinel_room_pointer[2];
extern const uint8_t pop_data_sentinel_tile_types[30];
extern const uint8_t pop_data_sentinel_tile_specs[30];

bool pop_data_read(void *context, uint16_t address, uint8_t *value)
{
    const uint8_t *source;
    (void)context;
    if (value == NULL) {
        return false;
    }
    if (address >= 0x1000 && address < 0x1290) {
        source = pop_data_lookup + (address - 0x1000);
    } else if (address >= 0x2800 && address < 0x3a00) {
        source = pop_data_animation + (address - 0x2800);
    } else if (address >= 0x4521 && address < 0x452b) {
        source = pop_data_wall_offsets + (address - 0x4521);
    } else if (address >= 0x541c && address < 0x5498) {
        source = pop_data_guard_parameters + (address - 0x541c);
    } else if (address >= 0x131c && address < 0x131e) {
        source = pop_data_sentinel_room_pointer + (address - 0x131c);
    } else if (address >= 0x5500 && address < 0x551e) {
        source = pop_data_sentinel_tile_types + (address - 0x5500);
    } else if (address >= 0x57d0 && address < 0x57ee) {
        source = pop_data_sentinel_tile_specs + (address - 0x57d0);
    } else {
        return false;
    }
    *value = *source;
    return true;
}
