#ifndef POP_AUDIO_PCM_H
#define POP_AUDIO_PCM_H
#include "audio.h"
typedef struct PopAudioPcmState {
    uint32_t clock_hz,sample_rate,phase;
    int64_t integral;
    bool high;
} PopAudioPcmState;
/* Host conversion of analytic speaker edges into signed mono PCM. Fractional
 * samples and speaker polarity persist across blocks; every sample is the
 * exact rectangular-wave area over its interval, rounded toward zero. This
 * is a digital display/audio boundary, not an analogue Apple speaker model. */
bool pop_audio_pcm_reset(PopAudioPcmState *state,uint32_t clock_hz,uint32_t sample_rate);
bool pop_audio_pcm_render(PopAudioPcmState *state,const PopAudioEvents *events,
    uint32_t total_cycles,int16_t *samples,size_t capacity,size_t *written);
#endif
