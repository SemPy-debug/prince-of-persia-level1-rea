#ifndef POP_MUSIC_H
#define POP_MUSIC_H
#include "audio.h"
typedef struct PopMusic {
    uint8_t work[32];
    uint8_t instrument,secondary_instrument,secondary_amplitude;
    uint8_t amplitude_operator[2],envelope[2];
    bool initialized;
} PopMusic;
/* Candidate general first-level score player. Separate from the accepted
 * startup_audio API until finite original paired-voice comparisons pass. */
bool pop_music_begin(PopMusic *state,uint8_t resource,PopAudioEvents *events);
bool pop_music_next(PopMusic *state,PopAudioEvents *events,bool *finished);
#endif
