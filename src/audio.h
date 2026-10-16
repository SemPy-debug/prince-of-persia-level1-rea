#ifndef POP_AUDIO_H
#define POP_AUDIO_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { POP_AUDIO_MAX_REQUESTS=32, POP_AUDIO_MAX_EDGES=2560 };
typedef struct PopAudioTone {
    uint8_t inner_limit,outer_limit,toggles;
} PopAudioTone;
typedef struct PopAudioCue {
    PopAudioTone tone[2];
    uint8_t compound,leading_extra_cycles;
} PopAudioCue;
typedef struct PopAudioEvents {
    uint32_t edges[POP_AUDIO_MAX_EDGES];
    uint32_t count,duration_cycles;
} PopAudioEvents;
/* Relative C030 toggle times and total originalEA47 duration, in original CPU
 * cycles. This is a native analytical event generator, with no opcode loop.
 * It preserves the request queue, including the original reverse order and
 * byte-wrapped dispatch aliases. It does not select host sample rate, model
 * analogue hardware, play sound or establish the game's frame cadence. */
bool pop_audio_events(const uint8_t *requests,size_t count,bool enabled,PopAudioEvents *out);
#endif
