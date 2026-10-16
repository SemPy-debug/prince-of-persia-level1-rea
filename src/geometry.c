#include "geometry.h"
#include "data.h"
#include <stddef.h>

static uint8_t relative_x(const uint8_t aux[256], uint8_t delta)
{
    if ((aux[0x43] & 0x80u) != 0) {
        delta = (uint8_t)(0u - (unsigned)delta);
    }
    return (uint8_t)((unsigned)aux[0x41] + delta);
}

bool pop_geometry_load_frame(uint8_t aux[256])
{
    unsigned base = 0x2800;
    uint8_t index, pose, character, bytes[5];
    uint16_t address;
    if (aux == NULL) {
        return false;
    }
    pose = aux[0x40];
    character = aux[0x4d];
    index = (uint8_t)(pose - 1u);
    if (character != 0 && character != 0x18) {
        if (character >= 5) {
            base = 0x2d78;
        } else {
            uint8_t mapped_pose = pose;
            if (character >= 2 && mapped_pose >= 0x66 && mapped_pose < 0x6b) {
                mapped_pose = (uint8_t)(mapped_pose + 0x46u);
            }
            if (mapped_pose >= 0x96 && mapped_pose < 0xbe) {
                base = 0x2cb0;
                index = (uint8_t)(mapped_pose - 0x96u);
            }
        }
    }
    address = (uint16_t)(base + 5u * index);
    for (unsigned i = 0; i < 5; ++i) {
        if (!pop_data_read(NULL, (uint16_t)(address + i), &bytes[i])) {
            return false;
        }
    }
    aux[0xe9] = index;
    aux[0xea] = 0;
    aux[0x80] = (uint8_t)address;
    aux[0x81] = (uint8_t)(address >> 8);
    aux[0x82] = bytes[0];
    aux[0xd9] = bytes[1];
    aux[0x83] = bytes[2];
    aux[0x84] = bytes[3];
    aux[0x85] = bytes[4];
    return true;
}

bool pop_geometry_update_column(uint8_t aux[256])
{
    uint8_t remainder, column, index;
    if (aux == NULL) {
        return false;
    }
    index = (uint8_t)(relative_x(aux, (uint8_t)(aux[0x83] - (aux[0x85] & 0x1fu))) - 7u);
    if (!pop_data_read(NULL, (uint16_t)(0x1100u + index), &remainder) ||
        !pop_data_read(NULL, (uint16_t)(0x1000u + index), &column)) {
        return false;
    }
    aux[3] = remainder;
    aux[0x44] = column;
    return true;
}

bool pop_geometry_project_actor(uint8_t aux[256])
{
    uint8_t x, low, high;
    if (aux == NULL || !pop_geometry_load_frame(aux)) {
        return false;
    }
    aux[0x76] = aux[0x78] = 0;
    aux[0x79] = 40;
    aux[0x77] = 192;
    aux[0x74] = aux[0x43];
    aux[0xe9] = aux[0x82] & 0x80u;
    aux[0x7a] = (uint8_t)(((aux[0xd9] & 0xc0u) / 2u + aux[0xe9]) >> 5);
    aux[0x70] = (aux[0x82] & 0x7fu) | aux[0x7d];
    x = (uint8_t)(relative_x(aux, aux[0x83]) - 0x3au);
    low = (uint8_t)((unsigned)x * 2u);
    high = (uint8_t)(x >> 7);
    if (high != 0 && low >= 0xf0) {
        high = 0xff;
    }
    aux[0x73] = (uint8_t)((unsigned)aux[0x84] + aux[0x42]);
    if (((aux[0x85] ^ aux[0x74]) & 0x80u) == 0) {
        ++low;
        if (low == 0) {
            ++high;
        }
    }
    aux[0x71] = low;
    aux[0x72] = high;
    return true;
}

uint8_t pop_geometry_d684(uint8_t type)
{
    return (uint8_t)(type != 0 && type != 9 && type != 12 && type != 20 && type < 26);
}

uint8_t pop_geometry_d69e(uint8_t type)
{
    switch (type) {
    case 7: case 12: case 4: return 1;
    case 13: case 18: return 3;
    case 20: return 4;
    default: return 0;
    }
}
