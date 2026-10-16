#ifndef POP_STARTUP_AUDIO_H
#define POP_STARTUP_AUDIO_H
#include "audio.h"
/* Native score state. work mirrors the music driver's saved32-byte workspace,
 * not the game's working actor zero page. No original executable image exists
 * in this state. Other fields represent instruments/envelope operations. */
typedef struct PopStartupAudio {
    uint8_t work[32];
    uint8_t instrument,secondary_instrument,secondary_amplitude;
    uint8_t volume_shift;
    bool initialized;
} PopStartupAudio;
/* Begin the source startup cue. Unsupported resources are explicit false and
 * preserve both outputs. Each next() returns one original D403 music quantum;
 * finished becomes true when its resource index reaches0. The caller owns
 * source0203/031A gating, paused-world draws, keyboard polling and wall time. */
bool pop_startup_audio_begin(PopStartupAudio *state,uint8_t resource,PopAudioEvents *events);
bool pop_startup_audio_next(PopStartupAudio *state,PopAudioEvents *events,bool *finished);
#endif
