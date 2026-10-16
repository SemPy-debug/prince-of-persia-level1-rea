#include "audio_pcm.h"
#include <stddef.h>
bool pop_audio_pcm_reset(PopAudioPcmState *s,uint32_t clock_hz,uint32_t sample_rate) {
    if (!s || !clock_hz || !sample_rate) return false;
    *s=(PopAudioPcmState){clock_hz,sample_rate,0,0,false};return true;
}
bool pop_audio_pcm_render(PopAudioPcmState *s,const PopAudioEvents *e,
    uint32_t total_cycles,int16_t *samples,size_t capacity,size_t *written) {
    PopAudioPcmState work;uint64_t needed;uint32_t previous=0;size_t count=0;
    if (!s || !e || !written || !s->clock_hz || !s->sample_rate ||
        s->phase>=s->clock_hz || e->count>POP_AUDIO_MAX_EDGES ||
        e->duration_cycles>total_cycles ||
        s->integral>6000*(int64_t)s->phase ||
        s->integral< -6000*(int64_t)s->phase) return false;
    needed=((uint64_t)total_cycles*s->sample_rate+s->phase)/s->clock_hz;
    if (needed>capacity || (needed && !samples)) return false;
    for (uint32_t i=0;i<e->count;++i) {
        if (e->edges[i]<previous || e->edges[i]>e->duration_cycles) return false;
        previous=e->edges[i];
    }
    work=*s;previous=0;
    for (uint32_t i=0;i<=e->count;++i) {
        const uint32_t end=i<e->count?e->edges[i]:total_cycles;
        uint64_t units=(uint64_t)(end-previous)*work.sample_rate;
        while (units) {
            const uint32_t available=work.clock_hz-work.phase;
            const uint32_t take=units<available?(uint32_t)units:available;
            work.integral+=(work.high?6000:-6000)*(int64_t)take;
            work.phase+=take;units-=take;
            if (work.phase==work.clock_hz) {
                samples[count++]=(int16_t)(work.integral/work.clock_hz);
                work.phase=0;work.integral=0;
            }
        }
        previous=end;
        if (i<e->count) work.high=!work.high;
    }
    *s=work;*written=count;return true;
}
