#include "loop_input.h"
#include "../generated/frame_event_clock_data.h"
#include <stddef.h>
#include <string.h>
typedef struct Input {
    PopLoopInputState *input;const PopLoopInputHardware *hardware;
    PopControllerState *state;PopControllerQueryResult *result;
    PopLoopInputEvents *events;const PopControllerIO *io;PopLoopWrite write;
} Input;
static void nz(PopControllerQueryResult *r,uint8_t v){r->zero=v==0;r->negative=(v&128u)!=0;}
static void a(PopControllerQueryResult *r,uint8_t v){r->a=v;nz(r,v);}
static void x(PopControllerQueryResult *r,uint8_t v){r->x=v;nz(r,v);}
static void y(PopControllerQueryResult *r,uint8_t v){r->y=v;nz(r,v);}
static void compare(PopControllerQueryResult *r,uint8_t p,uint8_t q){r->carry=p>=q;nz(r,(uint8_t)(p-q));}
static void add(PopControllerQueryResult *r,uint8_t v){unsigned sum=(unsigned)r->a+v;r->carry=sum>255u;a(r,(uint8_t)sum);}
static void subtract(PopControllerQueryResult *r,uint8_t v){r->carry=r->a>=v;a(r,(uint8_t)(r->a-v));}
static bool write_byte(Input *i,uint16_t at,uint8_t value){
    if(i->write&&i->io&&i->write(i->io->context,at,value))return true;
    i->state->required_helper=at;return false;
}
/* Actual DA99 comparisons; retained C and optional X load also matter to
 * later source shortcuts, even though the emitted axes are only39/3A. */
static void keyboard_axes(Input *i){
    uint8_t *s=i->state->aux;PopControllerQueryResult *r=i->result;
    a(r,0);s[0x39]=0;s[0x3a]=0;a(r,s[0x20f]);
    if(!r->negative){x(r,s[0x210]);if(!r->negative)return;a(r,(uint8_t)(r->a|128u));}
    compare(r,r->a,0xea);if(!r->zero)compare(r,r->a,0xca);
    if(r->zero){a(r,255);s[0x39]=r->a;return;}
    compare(r,r->a,0xec);if(!r->zero)compare(r,r->a,0xcc);
    if(r->zero){a(r,1);s[0x39]=r->a;return;}
    compare(r,r->a,0xe9);if(!r->zero)compare(r,r->a,0xc9);
    if(r->zero){a(r,255);s[0x3a]=r->a;return;}
    compare(r,r->a,0xeb);if(!r->zero)compare(r,r->a,0xcb);
    if(r->zero){a(r,1);s[0x3a]=r->a;return;}
    compare(r,r->a,0xf5);if(!r->zero)compare(r,r->a,0xd5);
    if(r->zero){a(r,255);s[0x39]=r->a;s[0x3a]=r->a;return;}
    compare(r,r->a,0xef);if(!r->zero)compare(r,r->a,0xcf);
    if(r->zero){a(r,1);s[0x39]=r->a;a(r,255);s[0x3a]=r->a;}
}
static void remember_key(Input *i){
    uint8_t *s=i->state->aux;PopControllerQueryResult *r=i->result;
    x(r,s[0xca]);x(r,(uint8_t)(r->x+1u));compare(r,r->x,10);if(r->carry)x(r,0);
    s[0xca]=r->x;s[0x3acu+r->x]=r->a;
}
static bool pattern_match(Input *i){
    /* PhysicalD4DA94..DA98 DATA, not executable bytes: reverse key history. */
    static const uint8_t pattern[5]={0xd0,0xc9,0xcb,0xd3,0};
    uint8_t *s=i->state->aux;PopControllerQueryResult *r=i->result;
    x(r,s[0xca]);y(r,0);
    for(;;){
        a(r,pattern[r->y]);if(r->zero)return true;
        compare(r,r->a,s[0x3acu+r->x]);
        if(!r->zero){
            compare(r,r->a,0xc1);if(!r->carry){a(r,255);return false;}
            compare(r,r->a,0xdb);if(r->carry){a(r,255);return false;}
            a(r,(uint8_t)(r->a|32u));compare(r,r->a,s[0x3acu+r->x]);
            if(!r->zero){a(r,255);return false;}
        }
        y(r,(uint8_t)(r->y+1u));x(r,(uint8_t)(r->x-1u));if(r->negative)x(r,9);
    }
}
static void advance_clock_index(Input *i){
    uint8_t *s=i->state->aux;PopControllerQueryResult *r=i->result;
    y(r,s[0x301]);compare(r,r->y,20);if(r->carry)return;
    y(r,18);s[0x301]=r->y;a(r,pop_frame_event_clock_thresholds[r->y]);s[0x306]=r->a;
    a(r,pop_frame_event_clock_thresholds[r->y+1u]);s[0x307]=r->a;
}
static void beep(Input *i){
    /* Source0418/0A08/0A20 is an immediate50-toggle speaker event, not D909's
     * queued cue stream. The host consumes it; source function result is exact. */
    i->events->beep=true;y(i->result,15);x(i->result,1);i->result->carry=true;a(i->result,0);
}
static bool paddle_sample(Input *i){
    if(!i->hardware->paddle_sample_valid){i->state->required_helper=0x0981;return false;}
    i->state->aux[0x3b]=i->hardware->paddle_x;i->state->aux[0x3c]=i->hardware->paddle_y;
    /*0981's temporary A flags are consumed by the following LDA; only X and
     * unchanged Y/C reach the threshold/calibration code. */
    x(i->result,255);return true;
}
static bool calibrate(Input *i){
    uint8_t *s=i->state->aux;PopControllerQueryResult *r=i->result;
    a(r,255);s[0x20d]=r->a;a(r,0);s[0x20a]=0;s[0x20b]=0;s[0x20c]=0;
    if(!paddle_sample(i))return false;
    a(r,(uint8_t)(s[0x3b]|s[0x3c]));
    if(r->negative){a(r,0);s[0x20d]=0;return true;}
    a(r,s[0x3b]);subtract(r,10);s[0x206]=r->a;a(r,s[0x3b]);add(r,10);s[0x208]=r->a;
    a(r,s[0x3c]);subtract(r,15);s[0x207]=r->a;a(r,s[0x3c]);add(r,15);s[0x209]=r->a;return true;
}
static PopLoopInputResult transfer(Input *i,uint16_t target){
    i->events->transfer=target;
    return target==0x2003?POP_LOOP_INPUT_RESTART_LEVEL:(target==0x2012?POP_LOOP_INPUT_NEW_GAME:POP_LOOP_INPUT_PLATFORM_TRANSFER);
}
static PopLoopInputResult shortcuts(Input *i){
    uint8_t *s=i->state->aux;PopControllerQueryResult *r=i->result;
    a(r,s[0x20f]);compare(r,r->a,0x9b);
    if(r->zero){i->input->waiting_key=true;return POP_LOOP_INPUT_WAIT_KEY;}
    compare(r,r->a,0x92);if(r->zero)return transfer(i,0x2012);
    compare(r,r->a,0x81);if(r->zero)return transfer(i,0x2003);
    compare(r,r->a,0x8b);if(r->zero){a(r,0);s[0x20d]=0;beep(i);return POP_LOOP_INPUT_COMPLETE;}
    compare(r,r->a,0x8a);if(r->zero){if(!calibrate(i))return POP_LOOP_INPUT_HELPER_REQUIRED;beep(i);return POP_LOOP_INPUT_COMPLETE;}
    compare(r,r->a,0x98);if(r->zero){a(r,(uint8_t)(s[0x20b]^1u));s[0x20b]=r->a;if(!r->negative){beep(i);return POP_LOOP_INPUT_COMPLETE;}}
    compare(r,r->a,0x99);if(r->zero){a(r,(uint8_t)(s[0x20a]^1u));s[0x20a]=r->a;if(!r->negative){beep(i);return POP_LOOP_INPUT_COMPLETE;}}
    compare(r,r->a,0x93);
    if(r->zero){a(r,0);if(!write_byte(i,0xb6c0,0))return POP_LOOP_INPUT_HELPER_REQUIRED;a(r,(uint8_t)(s[0x203]^1u));s[0x203]=r->a;if(!r->zero)beep(i);return POP_LOOP_INPUT_COMPLETE;}
    compare(r,r->a,0x8e);if(r->zero){a(r,(uint8_t)(s[0x31a]^1u));s[0x31a]=r->a;if(!r->zero)beep(i);return POP_LOOP_INPUT_COMPLETE;}
    compare(r,r->a,0x96);if(r->zero)return transfer(i,0xf933);
    compare(r,r->a,0x87);if(r->zero){a(r,s[0x3f4]);if(!write_byte(i,0xb6f0,r->a))return POP_LOOP_INPUT_HELPER_REQUIRED;return transfer(i,0xf92a);}
    compare(r,r->a,0xa0);if(r->zero){a(r,3);s[0x31f]=3;}
    return POP_LOOP_INPUT_COMPLETE;
}
static void debug_return(Input *i){
    uint8_t *s=i->state->aux;PopControllerQueryResult *r=i->result;
    a(r,s[0x20e]);if(r->zero)return;
    /*DA65 and DA43 are actual RTS stubs. */
    a(r,s[0x20f]);compare(r,r->a,0x8d);if(r->zero){a(r,0);s[0x20e]=0;}
}
static PopLoopInputResult keyboard(Input *i){
    uint8_t *s=i->state->aux;PopControllerQueryResult *r=i->result;PopLoopInputResult flow;
    if(!i->input->waiting_key){a(r,s[0x21]);if(!r->zero)i->input->waiting_key=true;}
    if(i->input->waiting_key){
        if(!(i->hardware->keyboard_data&128u))return POP_LOOP_INPUT_WAIT_KEY;
        a(r,i->hardware->keyboard_data);compare(r,r->a,0x9b);i->events->acknowledge_key=true;i->input->waiting_key=false;
        if(r->zero){a(r,1);s[0x21]=1;s[0x20f]=1;return POP_LOOP_INPUT_COMPLETE;}
        x(r,0);s[0x21]=0;a(r,0);
    }else a(r,i->hardware->keyboard_data);
    s[0x20f]=r->a;a(r,i->hardware->keyboard_status);s[0x210]=r->a;i->events->acknowledge_key=true;
    keyboard_axes(i);a(r,s[0x20f]);if(!r->negative)return POP_LOOP_INPUT_COMPLETE;
    remember_key(i);
    if(pattern_match(i)){
        a(r,3);x(r,s[0x20e]);if(!r->zero)a(r,11);compare(r,r->a,s[0x3f4]);
        if(r->carry){++s[0x31c];nz(r,s[0x31c]);advance_clock_index(i);}
    }
    flow=shortcuts(i);if(flow!=POP_LOOP_INPUT_COMPLETE)return flow;
    debug_return(i);return POP_LOOP_INPUT_COMPLETE;
}
static bool sample_axes_buttons(Input *i){
    uint8_t *s=i->state->aux,sample;PopControllerQueryResult *r=i->result;unsigned axis;
    a(r,s[0x20d]);
    if(!r->zero){
        if(!paddle_sample(i))return false;
        x(r,0);
        for(axis=0;axis<2;++axis){
            a(r,s[0x3bu+axis]);sample=r->a;compare(r,sample,s[0x206u+axis]);
            if(!r->carry)a(r,255);
            else{compare(r,sample,s[0x208u+axis]);a(r,r->carry?1:0);}
            s[0x3bu+axis]=r->a;if(axis==0)x(r,1);
        }
        a(r,s[0x20a]);if(!r->zero){a(r,0);subtract(r,s[0x3c]);s[0x3c]=r->a;}
        a(r,s[0x20b]);if(!r->zero){a(r,0);subtract(r,s[0x3b]);s[0x3b]=r->a;}
    }
    a(r,s[0x20c]);
    if(r->zero){a(r,i->hardware->button0);x(r,i->hardware->button1);}
    else{x(r,i->hardware->button1);a(r,i->hardware->button0);}
    s[0x1a]=r->a;s[0x1b]=r->x;return true;
}
void pop_loop_input_reset(PopLoopInputState *input){if(input)memset(input,0,sizeof *input);}
bool pop_loop_input_cached(const PopControllerState *state,PopLoopCachedInput *cached,PopControllerQueryResult *result){
    const uint8_t *s;PopControllerQueryResult r={0};uint8_t first,second;
    if(!state||!cached)return false;
    s=state->aux;if(result)r=*result;a(&r,s[0x20d]);
    if(!r.zero){a(&r,s[0x3b]);cached->horizontal=r.a;a(&r,s[0x3c]);cached->vertical=r.a;first=s[0x1b];second=s[0x1a];}
    else{a(&r,s[0x39]);cached->horizontal=r.a;a(&r,s[0x3a]);cached->vertical=r.a;first=s[0x1a];second=s[0x1b];}
    a(&r,first);x(&r,s[0xde]);if(!r.negative)a(&r,(uint8_t)(r.a|second));cached->button=r.a;
    if(result)*result=r;
    return true;
}
PopLoopInputResult pop_loop_input_sample(PopLoopInputState *input,const PopLoopInputHardware *hardware,
    PopControllerState *state,PopControllerQueryResult *result,PopLoopInputEvents *events,
    const PopControllerIO *io,PopLoopWrite write){
    PopLoopInputState staged_input;PopControllerState staged_state;PopControllerQueryResult staged_result;
    PopLoopInputEvents staged_events={false,false,0};Input i;PopLoopInputResult flow;
    if(!input||!hardware||!state||!result||!events)return POP_LOOP_INPUT_INVALID;
    staged_input=*input;staged_state=*state;staged_result=*result;
    i=(Input){&staged_input,hardware,&staged_state,&staged_result,&staged_events,io,write};flow=keyboard(&i);
    if(flow==POP_LOOP_INPUT_COMPLETE&&!sample_axes_buttons(&i))flow=POP_LOOP_INPUT_HELPER_REQUIRED;
    if(flow==POP_LOOP_INPUT_HELPER_REQUIRED){state->required_helper=staged_state.required_helper;return flow;}
    *input=staged_input;*state=staged_state;*result=staged_result;*events=staged_events;return flow;
}
