#include "input.h"

PopInputAxes pop_input_apple_axes(uint8_t keyboard_data, uint8_t keyboard_status) {
    PopInputAxes axes = {0, 0};
    /* Original DA99 accepts a new latched key even if it was released before
     * sampling. With no new strobe, the hardware any-key bit gates reuse.
     * REA ev_718c69f...; natural original cases in keyboard_original.csv. */
    if (!(keyboard_data & 0x80u) && !(keyboard_status & 0x80u)) return axes;
    switch (keyboard_data & 0x7fu) {
    case 'J': case 'j': axes.horizontal = -1; break;
    case 'L': case 'l': axes.horizontal = 1; break;
    case 'I': case 'i': axes.vertical = -1; break;
    case 'K': case 'k': axes.vertical = 1; break;
    case 'U': case 'u': axes.horizontal = -1; axes.vertical = -1; break;
    case 'O': case 'o': axes.horizontal = 1; axes.vertical = -1; break;
    default: break;
    }
    return axes;
}
