#include "music.h"
#include <string.h>
#include "../generated/music_data.h"

static uint16_t word(const uint8_t *z,unsigned p){return (uint16_t)(z[p]|((uint16_t)z[p+1]<<8));}
static void setword(uint8_t *z,unsigned p,uint16_t v){z[p]=(uint8_t)v;z[p+1]=(uint8_t)(v>>8);}
static bool supported(uint8_t r){return r==0||r==1||r==2||r==3||r==4||r==6||r==7||r==8||r==9||r==12;}
static bool pair(uint16_t at,uint8_t *first,uint8_t *second){
    if(at<0xd000||at>=0xd3ff)return false;
    *first=pop_music_score[at-0xd000];*second=pop_music_score[at-0xd000+1];return true;
}
static bool reset_score(PopMusic *s,uint32_t *cycles){
    uint8_t *z=s->work,r=z[30];if(!supported(r))return false;
    s->amplitude_operator[0]=3;s->amplitude_operator[1]=3;
    z[5]=0;s->instrument=0;s->secondary_instrument=0;z[4]=0;
    setword(z,0,(uint16_t)(pop_music_score[r]|((uint16_t)pop_music_score[0x68+r]<<8)));
    z[15]=0;z[24]=0;z[27]=0;*cycles+=80;return true;
}
static bool parse(PopMusic *s,bool second,uint32_t *cycles){
    uint8_t *z=s->work;unsigned ptr=second?22:0,wave=second?2:3,env=second?16:12,
        pitch=second?17:13,duration=second?18:10,period=second?19:8,repeats=second?20:9,
        octave=second?5:4,loop=second?27:24,saved=second?28:25;
    for(unsigned budget=0;budget<256;++budget){
        uint16_t before=word(z,ptr),at=(uint16_t)(before+2);setword(z,ptr,at);z[wave]=0;z[env]=0;
        *cycles+=27u+((before&255u)==254u?4u:0u);
        uint8_t first,value;if(!pair(at,&first,&value))return false;
        if(value==0){
            *cycles+=17;
            if(first==0){
                if(second){z[15]=0;*cycles+=16;return true;}
                *cycles+=6;if(!reset_score(s,cycles))return false;continue;
            }
            if(first==254){
                *cycles+=second?8:8;
                if(z[loop]){setword(z,ptr,word(z,saved));z[loop]=0;*cycles+=25;}
                else *cycles+=9;
                continue;
            }
            if(first==255){
                *cycles+=second?41:46;z[duration]=255;z[pitch]=0;
                z[period]=pop_music_period[0];z[repeats]=pop_music_repeat[0];return true;
            }
            if(first<9){
                if(first<1||first>5)return false;
                s->amplitude_operator[second?1:0]=first;*cycles+=second?37:42;continue;
            }
            unsigned index=first-9;z[octave]=0;
            if(index>=32){index&=31;z[octave]=12;*cycles+=10;}
            if(index!=11&&index!=31)return false;
            if(second)s->secondary_instrument=(uint8_t)index;else s->instrument=(uint8_t)index;
            *cycles+=second?70:75;continue;
        }
        *cycles+=16;z[pitch]=first&252u;
        if(z[pitch]){z[pitch]=(uint8_t)((first>>2)+z[octave]);*cycles+=z[pitch]<25?31:26;}
        else *cycles+=10;
        *cycles+=second?8:10;
        const uint8_t command=value&192u;
        if(value&63u){
            *cycles+=second?81:82; /* DEE7->DF27 taken branch crosses a page. */
            z[duration]=value&63u;z[7]=(uint8_t)(command>>4);
            const unsigned index=(first&3u)|z[7];s->envelope[second?1:0]=(uint8_t)index;
            if(second)s->secondary_amplitude=pop_music_level[index];else z[11]=pop_music_level[index];
            if(z[pitch]>=sizeof pop_music_period)return false;
            z[period]=pop_music_period[z[pitch]];z[repeats]=pop_music_repeat[z[pitch]];return true;
        }
        *cycles+=second?6:7;
        if(!second&&command==128){
            if(first==0||first>=128)return false;
            z[30]=(uint8_t)(first-1);*cycles+=18;continue;
        }
        if(!second&&command==64){
            z[15]=1;z[21]=1;setword(z,22,at);setword(z,0,(uint16_t)(at+2u*first));
            *cycles+=59u+((first&128u)?7u:0u);
            if(!parse(s,true,cycles))return false;
            *cycles+=3;continue;
        }
        if(command==192){
            /* Selected first-level scores use positive offsets only. The
             * source negative secondary branch is a distinct unsupported
             * format path, not ordinary signed subtraction. */
            if(first&128u)return false;
            if(z[loop]){setword(z,ptr,word(z,saved));z[loop]=0;*cycles+=second?28:42;}
            else{z[loop]=1;setword(z,saved,at);setword(z,ptr,(uint16_t)(at-2u*first));*cycles+=second?55:71;if(first&128u)*cycles+=5;}
            continue;
        }
        return false;
    }
    return false;
}
static uint8_t transform(uint8_t value,uint8_t op){
    switch(op){case 1:return value>>2;case 2:return value>>1;case 3:return value;case 4:return (uint8_t)(value*2u);case 5:return (uint8_t)(value*4u);default:return value;}
}
static bool envelope(PopMusic *s,bool second,uint32_t *cursor){
    uint8_t *z=s->work;unsigned index=second?16:12,remaining=second?18:10,id=s->envelope[second?1:0];
    if(id>=16||s->amplitude_operator[second?1:0]<1||s->amplitude_operator[second?1:0]>5)return false;
    uint16_t base=(uint16_t)(pop_music_envelope_low[id]|((uint16_t)pop_music_envelope_high[id]<<8));
    for(unsigned budget=0;budget<4;++budget){
        uint16_t at=(uint16_t)(base+z[index]);if(at<0xd49e||at>=0xd58a)return false;
        uint8_t value=pop_music_envelope[at-0xd49e];
        if(value&128u){*cursor+=10;return true;}
        if(value==127){if(z[index]<z[remaining]){*cursor+=23;return true;}*cursor+=27;++z[index];continue;}
        value=transform(value,s->amplitude_operator[second?1:0]);
        if(second)s->secondary_amplitude=value;else z[11]=value;
        ++z[index];*cursor+=25;return true;
    }return false;
}
static uint8_t amplitude(uint8_t wave,uint8_t level,bool rest){
    if(rest)return 0;
    switch(wave){case 1:return 0;case 2:return (uint8_t)((level>>1)+1u);case 3:return (uint8_t)(level+1u);case 4:return (uint8_t)(level+(level>>1)+1u);case 5:return (uint8_t)(level*2u+1u);default:return 0;}
}
bool pop_music_begin(PopMusic *state,uint8_t resource,PopAudioEvents *events){
    if(!state||!events||!supported(resource)||resource==0)return false;
    PopMusic s=*state;PopAudioEvents e={0};uint32_t cursor=9;s.work[30]=resource;s.initialized=true;
    if(!reset_score(&s,&cursor)||!parse(&s,false,&cursor))return false;
    e.duration_cycles=cursor;*state=s;*events=e;return true;
}
bool pop_music_next(PopMusic *state,PopAudioEvents *events,bool *finished){
    if(!state||!events||!finished||!state->initialized||!supported(state->work[30])||state->work[30]==0)return false;
    PopMusic s=*state;PopAudioEvents e={0};uint8_t *z=s.work;bool second=false;uint32_t cursor=9;
    if(z[15]){z[21]^=1;second=z[21]==0;cursor+=second?18:16;}else cursor+=6;
    if(!envelope(&s,second,&cursor))return false;
    unsigned instrument=second?s.secondary_instrument:s.instrument,wave=second?2:3,pitch=second?17:13,
        period=second?19:8,repeats=second?20:9,duration=second?18:10;
    if(instrument!=0&&instrument!=11&&instrument!=31)return false;
    const uint8_t *pattern=instrument==0?pop_music_wave0:instrument==11?pop_music_wave11:pop_music_wave31;
    if(instrument==0){z[wave]=0;cursor+=4;}
    cursor+=73+(z[pitch]==0?15:0);
    const unsigned count=z[repeats]?z[repeats]:256,p=z[period]?z[period]:256;
    const unsigned delay=z[pitch]>0&&z[pitch]<25?7:1;
    for(unsigned i=0;i<count;++i){
        if(z[wave]>=3)return false;
        uint8_t value=pattern[z[wave]];cursor+=9;
        if(value&128u){z[wave]=value&127u;cursor+=second?6:7;if(z[wave]>=3)return false;value=pattern[z[wave]];cursor+=9;}
        ++z[wave];cursor+=11;
        const uint8_t pulse=amplitude(value,second?s.secondary_amplitude:z[11],z[pitch]==0);z[6]=pulse;cursor+=10;
        unsigned head=(uint8_t)(z[period]-pulse);cursor+=5u*(head?head:256u)-1u;
        cursor+=1u+delay*(5u*p+9u);cursor+=pulse?5:6;
        if(pulse){if(e.count+2>POP_AUDIO_MAX_EDGES)return false;e.edges[e.count++]=cursor+3;cursor+=4+5u*pulse-1u;e.edges[e.count++]=cursor+3;cursor+=4;}
        cursor+=i+1==count?4u:(second?5u:6u);
    }
    cursor+=8;--z[duration];if(z[duration])cursor+=6;else{cursor+=2;if(!parse(&s,second,&cursor))return false;}
    cursor+=9;e.duration_cycles=cursor;*finished=z[30]==0;*state=s;*events=e;return true;
}
