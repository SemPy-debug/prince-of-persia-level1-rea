#include "audio.h"
#include <stddef.h>
#include <string.h>

extern const PopAudioCue pop_audio_cues[20];

/* EB0D's nested byte-counter loops reduce to this period. The terminal BCC
 * and BNE each take one fewer cycle; C030 is read three cycles into BIT.
 * These are arithmetic durations, not a CPU-instruction simulator. */
static bool tone_events(PopAudioEvents *events,uint32_t *cursor,PopAudioTone tone) {
    const unsigned inner=tone.inner_limit?tone.inner_limit:1;
    const unsigned outer=tone.outer_limit?tone.outer_limit:1;
    const unsigned toggles=tone.toggles?tone.toggles:256;
    const uint32_t period=outer*(9u*inner+10u)+12u;
    if(events->count+toggles>POP_AUDIO_MAX_EDGES)return false;
    for(unsigned i=0;i<toggles;++i)
        events->edges[events->count++]=*cursor+11u+i*period;
    *cursor+=toggles*period+13u;
    return true;
}

bool pop_audio_events(const uint8_t *requests,size_t count,bool enabled,PopAudioEvents *out) {
    PopAudioEvents work;
    uint32_t cursor;
    if(out==NULL || count>POP_AUDIO_MAX_REQUESTS || (count!=0 && requests==NULL))return false;
    memset(&work,0,sizeof work);
    if(!enabled || count==0) {
        work.duration_cycles=enabled?19u:13u;
        *out=work;
        return true;
    }
    cursor=12; /* OriginalEA47 enabled, nonempty queue prefix. */
    for(size_t remaining=count;remaining!=0;--remaining) {
        const uint8_t table_index=(uint8_t)(requests[remaining-1]*2u);
        cursor+=14; /* Fetch request, preserve queue index and callEA61. */
        if(table_index>=40) {
            cursor+=13; /* Wrapped ASL/CMP/BCS/RTS reject path. */
        } else {
            const PopAudioCue *cue=&pop_audio_cues[table_index/2];
            cursor+=27; /* Native selection replaces original patched JMP. */
            if(cue->compound) {
                cursor+=2; /* Original repeat count2. */
                for(unsigned repeat=0;repeat<2;++repeat) {
                    cursor+=15; /* Save repeat, load first tone and call. */
                    if(!tone_events(&work,&cursor,cue->tone[0]))return false;
                    cursor+=12; /* Load second tone and call. */
                    if(!tone_events(&work,&cursor,cue->tone[1]))return false;
                    cursor+=repeat==0?11u:16u;
                }
            } else {
                cursor+=9u+cue->leading_extra_cycles;
                if(!tone_events(&work,&cursor,cue->tone[0]))return false;
            }
        }
        cursor+=remaining==1?14u:9u;
    }
    work.duration_cycles=cursor;
    *out=work;
    return true;
}
