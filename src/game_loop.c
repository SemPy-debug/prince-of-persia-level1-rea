#include "game_loop.h"
#include <stddef.h>
#include <string.h>

typedef struct LoopContext {PopGameLoop *loop;PopGame *game;} LoopContext;
static bool read_value(void *context,uint16_t address,uint8_t *value){
    LoopContext *c=context;return pop_runtime_read(&c->game->runtime,address,value);
}
static bool write_value(void *context,uint16_t address,uint8_t value){
    LoopContext *c=context;return pop_runtime_write(&c->game->runtime,address,value);
}
static bool query_value(void *context,uint16_t helper,uint8_t argument,
    PopControllerState *state,PopControllerQueryResult *result){
    LoopContext *c=context;
    if(helper==0xe003||helper==0xe00f)
        return pop_loop_visual_event(helper,argument,state,&c->loop->visual);
    return pop_runtime_query(&c->game->runtime,helper,argument,state,result);
}
static bool helper(LoopContext *c,uint16_t id,PopControllerQueryResult *r){
    PopControllerIO io={query_value,read_value,c};
    return pop_loop_state_query(id,r->a,&c->game->state,r,NULL,&io,write_value);
}
static PopGameLoopResult unmet(PopGame *target,const PopGame *work,uint16_t fallback){
    uint16_t id=work->required_helper?work->required_helper:
        (work->runtime.missing_helper?work->runtime.missing_helper:
         (work->state.required_helper?work->state.required_helper:fallback));
    target->required_helper=id;target->runtime.missing_helper=id;
    return POP_LOOP_HELPER_REQUIRED;
}
static PopGameLoopResult before(PopGameLoop *loop,PopGame *game,
    PopGameInput input,const PopLoopInputHardware *hardware,bool use_source_cache);
bool pop_game_loop_reset(PopGameLoop *loop,PopGame *game){
    if(!loop||!game)return false;
    if(!pop_game_reset(game))return false;
    memset(loop,0,sizeof *loop);
    loop->visual.page=game->state.aux[0]==0?2:1;
    return true;
}
PopGameLoopResult pop_game_loop_before_presentation(PopGameLoop *loop,
    PopGame *game,PopGameInput input){
    PopLoopInputHardware hardware={0};
    if(!game)return POP_LOOP_INVALID;
    hardware.keyboard_data=(uint8_t)(game->state.aux[0x20f]&127u);
    hardware.keyboard_status=(uint8_t)(game->state.aux[0x210]&127u);
    hardware.button0=input.action;
    return pop_game_loop_before_presentation_with_hardware(loop,game,input,&hardware);
}
PopGameLoopResult pop_game_loop_before_presentation_with_hardware(PopGameLoop *loop,
    PopGame *game,PopGameInput input,const PopLoopInputHardware *hardware){
    return before(loop,game,input,hardware,false);
}
PopGameLoopResult pop_game_loop_before_source_presentation(PopGameLoop *loop,
    PopGame *game,const PopLoopInputHardware *hardware){
    return before(loop,game,(PopGameInput){0},hardware,true);
}
static PopGameLoopResult before(PopGameLoop *loop,PopGame *game,
    PopGameInput input,const PopLoopInputHardware *hardware,bool use_source_cache){
    PopGame work;PopGameLoop stage;PopControllerQueryResult r={0};LoopContext c;
    PopLoopInputResult sampled;PopGameLoopResult boundary=POP_LOOP_FRAME_READY;
    PopGameResult frame;
    if(!loop||!game||!hardware||!game->initialized||loop->awaiting_presentation)return POP_LOOP_INVALID;
    if(loop->modal.active)return POP_LOOP_MESSAGE;
    if(loop->complete)return POP_LOOP_LEVEL_COMPLETE;
    work=*game;stage=*loop;work.required_helper=0;work.state.required_helper=0;
    work.runtime.missing_helper=0;c=(LoopContext){&stage,&work};
    if(!stage.input.waiting_key){
        if(!helper(&c,0x0436,&r))return unmet(game,&work,0x0436);
        pop_loop_clear_frame_flags(&work.state);
    }
    PopControllerIO io={query_value,read_value,&c};
    sampled=pop_loop_input_sample(&stage.input,hardware,&work.state,&r,
                                  &stage.input_events,&io,write_value);
    switch(sampled){
    case POP_LOOP_INPUT_COMPLETE:break;
    case POP_LOOP_INPUT_WAIT_KEY:boundary=POP_LOOP_WAIT_KEY;break;
    case POP_LOOP_INPUT_RESTART_LEVEL:boundary=POP_LOOP_RESTART_LEVEL;break;
    case POP_LOOP_INPUT_NEW_GAME:boundary=POP_LOOP_NEW_GAME;break;
    case POP_LOOP_INPUT_PLATFORM_TRANSFER:boundary=POP_LOOP_PLATFORM_TRANSFER;break;
    default:return unmet(game,&work,0xd93f);
    }
    if(boundary!=POP_LOOP_FRAME_READY){*game=work;*loop=stage;return boundary;}
    if(use_source_cache){
        PopLoopCachedInput cached;
        if(!pop_loop_input_cached(&work.state,&cached,NULL))return unmet(game,&work,0x047e);
        input=(PopGameInput){cached.horizontal,cached.vertical,cached.button};
    }
    if(!helper(&c,0xd91e,&r))return unmet(game,&work,0xd91e);
    if(r.a!=0)return unmet(game,&work,0x2015);
    if(!helper(&c,0x279f,&r))return unmet(game,&work,0x279f);
    frame=pop_game_frame(&work,input);
    if(frame==POP_GAME_RESTART_LEVEL||frame==POP_GAME_GAME_OVER||frame==POP_GAME_TIME_EXPIRED){
        boundary=frame==POP_GAME_RESTART_LEVEL?POP_LOOP_RESTART_LEVEL:
            (frame==POP_GAME_GAME_OVER?POP_LOOP_GAME_OVER:POP_LOOP_TIME_EXPIRED);
        *game=work;*loop=stage;return boundary;
    }
    if(frame!=POP_GAME_UPDATED)return unmet(game,&work,0x224e);
    if(!helper(&c,0x249c,&r))return unmet(game,&work,0x249c);
    stage.awaiting_presentation=true;*game=work;*loop=stage;return POP_LOOP_FRAME_READY;
}
PopGameLoopResult pop_game_loop_after_presentation(PopGameLoop *loop,PopGame *game){
    PopGame work;PopGameLoop stage;PopControllerQueryResult r={0};LoopContext c;
    PopLoopModalResult modal;PopGameLoopResult outcome=POP_LOOP_FRAME_READY;
    if(!loop||!game||!game->initialized||!loop->awaiting_presentation)return POP_LOOP_INVALID;
    work=*game;stage=*loop;work.required_helper=0;work.state.required_helper=0;
    work.runtime.missing_helper=0;c=(LoopContext){&stage,&work};
    PopControllerIO io={query_value,read_value,&c};
    if(!helper(&c,0xd906,&r))return unmet(game,&work,0xd906);
    if(!helper(&c,0x24ad,&r))return unmet(game,&work,0x24ad);
    modal=pop_loop_modal_begin(&stage.modal,&work.state,&io);
    if(modal==POP_LOOP_MODAL_REQUIRED||modal==POP_LOOP_MODAL_INVALID)return unmet(game,&work,0x2641);
    if(modal==POP_LOOP_MODAL_OPENED||modal==POP_LOOP_MODAL_ACTIVE)outcome=POP_LOOP_MESSAGE;
    /*2641 does not return to21BA until its modal has been dismissed. */
    if(!stage.modal.active&&work.state.aux[0x31c]!=work.state.aux[0x3f4]){
        if(work.state.aux[0x3f4]!=1||work.state.aux[0x31c]!=2)return unmet(game,&work,0x21c5);
        stage.complete=true;outcome=POP_LOOP_LEVEL_COMPLETE;
    }
    stage.awaiting_presentation=false;*loop=stage;*game=work;return outcome;
}
bool pop_game_loop_dismiss_message(PopGameLoop *loop,PopGame *game){
    PopGame work;PopGameLoop stage;
    if(!loop||!game||!game->initialized||loop->awaiting_presentation)return false;
    work=*game;stage=*loop;
    if(!pop_loop_modal_dismiss(&stage.modal,&work.state))return false;
    if(work.state.aux[0x31c]!=work.state.aux[0x3f4]){
        if(work.state.aux[0x3f4]!=1||work.state.aux[0x31c]!=2){
            (void)unmet(game,&work,0x21c5);return false;
        }
        stage.complete=true;
    }
    *loop=stage;*game=work;return true;
}
