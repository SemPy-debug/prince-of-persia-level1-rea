#ifndef POP_INPUT_H
#define POP_INPUT_H
#include <stdint.h>

typedef struct PopInputAxes {
    int8_t horizontal;
    int8_t vertical;
} PopInputAxes;

/* Original cached C000/C010 bytes. This does not poll a host keyboard. */
PopInputAxes pop_input_apple_axes(uint8_t keyboard_data, uint8_t keyboard_status);
#endif
