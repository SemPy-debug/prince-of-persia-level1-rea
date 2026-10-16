#include "startup_audio.h"
#include <string.h>
#include "../generated/startup_audio_data.h"

/* Source DB9F..DBA2 are amplitude values, not executable dispatch. */
static uint8_t pulse_amplitude(uint8_t wave,uint8_t amplitude){
    switch(wave){case 0:return 0;case 1:return 0;
    case 2:return (uint8_t)((amplitude>>1)+1u);
    case 3:return (uint8_t)(amplitude+1u);
    case 4:return (uint8_t)(amplitude+(amplitude>>1)+1u);
    case 5:return (uint8_t)(amplitude*2u+1u);default:return 0;}
}
static bool select_note(PopStartupAudio *s,unsigned note){
    uint8_t *z=s->work;unsigned at=6+note*2;
    if(note>3)return false;
    if(note==3)at=14;
    uint8_t first=pop_startup_score3[at],second=pop_startup_score3[at+1];
    const unsigned pitch=(first>>2),index=pitch-27u;
    if(pitch<28||pitch>30)return false;
    z[0]=(uint8_t)(0x48+at);z[1]=0xd1;z[3]=0;z[12]=0;
    z[13]=(uint8_t)pitch;z[10]=second&63u;z[7]=(uint8_t)((second&192u)>>4);
    z[11]=(note==3)?5:4;z[8]=pop_startup_period[index];z[9]=pop_startup_repeat[index];
    s->volume_shift=(note==3)?2:1;return true;
}
static void finish_score(PopStartupAudio *s){
    uint8_t *z=s->work;z[30]=0;z[0]=0;z[1]=0xd1;z[3]=0;z[4]=0;z[5]=0;
    z[10]=255;z[13]=0;z[8]=pop_startup_period[0];z[9]=pop_startup_repeat[0];
    z[12]=0;z[15]=0;z[24]=0;z[27]=0;s->instrument=0;s->secondary_instrument=0;s->volume_shift=0;
}
bool pop_startup_audio_begin(PopStartupAudio *state,uint8_t resource,PopAudioEvents *events){
    if(!state||!events||resource!=3)return false;
    PopStartupAudio s=*state;PopAudioEvents e={0};uint8_t *z=s.work;
    z[30]=resource;z[4]=0;z[5]=0;z[15]=0;z[24]=0;z[27]=0;
    s.instrument=11;s.secondary_instrument=0;s.initialized=true;
    if(!select_note(&s,0))return false;
    /* D400 prefix, DBD3 reset, instrument and volume controls, first note. */
    e.duration_cycles=9u+80u+119u+86u+161u;
    *state=s;*events=e;return true;
}
bool pop_startup_audio_next(PopStartupAudio *state,PopAudioEvents *events,bool *finished){
    if(!state||!events||!finished||!state->initialized||state->work[30]!=3||state->instrument!=11)return false;
    PopStartupAudio s=*state;PopAudioEvents e={0};uint8_t *z=s.work;uint32_t cursor=15;
    const uint8_t *envelope=s.volume_shift==1?pop_startup_envelope3:pop_startup_envelope4;
    const unsigned envelope_length=s.volume_shift==1?sizeof pop_startup_envelope3:sizeof pop_startup_envelope4;
    for(unsigned tries=0;;++tries){
        if(tries>2||z[12]>=envelope_length)return false;
        const uint8_t value=envelope[z[12]];
        if(value&128u){cursor+=10;break;}
        if(value==127){
            if(z[12]<z[10]){cursor+=23;break;}
            cursor+=27;++z[12];continue;
        }
        cursor+=25;z[11]=(uint8_t)(value<<s.volume_shift);++z[12];break;
    }
    cursor+=73;
    const unsigned periods=z[9]?z[9]:256,period=z[8]?z[8]:256;
    for(unsigned i=0;i<periods;++i){
        if(z[3]>=sizeof pop_startup_wave)return false;
        uint8_t value=pop_startup_wave[z[3]];
        cursor+=9; /* LDY, indexed waveform load, ordinary BMI. */
        if(value&128u){z[3]=value&127u;cursor+=7; /* taken BMI crosses DE03->DDFA */
            if(z[3]>=sizeof pop_startup_wave)return false;
            value=pop_startup_wave[z[3]];cursor+=9;
        }
        ++z[3];cursor+=11; /* waveform counter, TAY, amplitude load. */
        const uint8_t amplitude=pulse_amplitude(value,z[11]);z[6]=amplitude;
        cursor+=10; /* store pulse amplitude and establish first delay. */
        const unsigned head=(uint8_t)(z[8]-amplitude);
        cursor+=5u*(head?head:256u)-1u;
        cursor+=10u+5u*period;
        cursor+=5; /* LDY pulse width and ordinary BEQ. */
        if(amplitude){
            if(e.count+2>POP_AUDIO_MAX_EDGES)return false;
            e.edges[e.count++]=cursor+3;cursor+=4;
            cursor+=5u*amplitude-1u;e.edges[e.count++]=cursor+3;cursor+=4;
        }
        cursor+=i+1==periods?4u:6u; /* DEX; taken BNE crosses DE2E->DDFE. */
    }
    cursor+=8; /* DEC duration plus taken BNE, adjusted below on terminal note. */
    --z[10];
    if(z[10])cursor+=6;
    else{
        cursor+=2; /* untaken BNE then JMP score parser. */
        const uint16_t address=(uint16_t)(z[0]|((uint16_t)z[1]<<8));
        unsigned note=address==0xd14e?1u:address==0xd150?2u:address==0xd152?3u:4u;
        if(note<3){if(!select_note(&s,note))return false;cursor+=161;}
        else if(note==3){if(!select_note(&s,note))return false;cursor+=86+161;}
        else{
            finish_score(&s);
            /* Chain command1/80, zero restart marker, DBD3 resource0 reset,
             * then FF terminator. Its D0FE->D100 pointer wrap adds4 cycles. */
            cursor+=88u+50u+80u+94u;
        }
    }
    cursor+=9; /* DFF3 LDA resource index and RTS. */
    e.duration_cycles=cursor;*finished=z[30]==0;*state=s;*events=e;return true;
}
