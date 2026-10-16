#include "falling_draw.h"
#include "data.h"
#include <string.h>

extern const uint8_t pop_loose_face[11];
extern const uint8_t pop_loose_bottom[11];

bool pop_falling_draw(const uint8_t record[6], uint8_t display_room,
                      uint8_t below_room, PopFallingObject *out)
{
    if (record == NULL || out == NULL || display_room == 0 ||
        display_room > 24 || below_room > 24) return false;
    PopFallingObject object;
    memset(&object, 0, sizeof(object));
    if (record[4] != 0) { *out = object; return true; }
    uint8_t y = record[1];
    if (record[2] == display_room) {
        if (y >= 209) { *out = object; return true; }
    } else if (record[2] == below_room) {
        if (y >= 17 && y < 239) { *out = object; return true; }
        y = (uint8_t)(y + 192u);
    } else { *out = object; return true; }
    uint8_t row = 3, value;
    for (;;) {
        if (!pop_data_read(NULL, (uint16_t)(0x1275u + row), &value)) return false;
        if (y >= value) break;
        --row;
        if ((row & 128u) != 0) break;
    }
    const uint8_t column = record[0] >> 2;
    if ((row & 128u) != 0) object.depth = column;
    else if (row >= 3 || column >= 10) object.depth = 30;
    else {
        if (!pop_data_read(NULL, (uint16_t)(0x1200u + row), &value)) return false;
        object.depth = (uint8_t)(value + column);
    }
    object.visible = true;
    object.bottom_y = y;
    /* Source AUX129A data byte=3, 1445[10]=31,145B[10]=45;
     * the edge image27 is the immediate operand at1ECD. These are
     * immutable image identities, not a frozen framebuffer. */
    const unsigned images[4] = {3, pop_loose_face[10], pop_loose_bottom[10], 27};
    const uint8_t modes[4] = {0,1,2,1};
    const uint8_t ys[4] = {(uint8_t)(y - 3u), (uint8_t)(y - 3u), y, (uint8_t)(y - 4u)};
    for (unsigned i = 0; i < 4; ++i) {
        object.draws[i] = (PopSceneDraw){POP_ASSET_BACKGROUND_1, images[i],
            {(uint8_t)(record[0] + (i == 3 ? 4u : 0u)), ys[i], 0, modes[i],
             0, 0, 40, 192}};
    }
    *out = object;
    return true;
}
