#include "player_tick.h"
#include <stddef.h>
static bool tick_query(PopControllerState *s,const PopControllerIO *io,
                       PopControllerQueryResult *q,uint16_t helper,uint8_t argument) {
    q->a=argument;
    if (!io || !io->query || !io->query(io->context,helper,argument,s,q)) {
        s->required_helper=helper;return false;
    }
    return true;
}
static bool prefix_special_guard(PopControllerState *s,const PopControllerIO *io,
                                 PopControllerQueryResult *q) {
    uint8_t *a=s->aux;
    if (a[0x3f4]!=12 || (uint8_t)(a[0x3a9]|a[0x4d])!=1 ||
        !(a[0x4f]&128u) || (a[0x3ab]&128u)) return true;
    a[0x89]=255;a[0x88]=5;
    return tick_query(s,io,q,0xd909,5) && tick_query(s,io,q,0xf927,100);
}
static bool room0_response(PopControllerState *s,const PopControllerIO *io,
                           PopControllerQueryResult *q) {
    uint8_t *a=s->aux;
    if (!(a[0x4f]&128u) || a[0x4b]!=0) return true;
    if (!tick_query(s,io,q,0xd909,5) || !tick_query(s,io,q,0xf927,100)) return false;
    a[0xdb]=0;a[0x4f]=0;a[0x40]=185;return true;
}
PopControllerResult pop_player_prepare_tick(PopControllerState *s,const PopControllerIO *io) {
    PopControllerState work;PopControllerQueryResult q={0,0,0,false,false,false};int i;
    if (!s) return POP_CONTROLLER_INVALID;
    work=*s;work.required_helper=0;work.selected_sequence=0;
    for (i=15;i>=0;--i) {
        work.aux[0x40u+(unsigned)i]=work.aux[0x50u+(unsigned)i];
        work.aux[0x39cu+(unsigned)i]=work.aux[0x60u+(unsigned)i];
    }
    if (!tick_query(&work,io,&q,0x3a09,work.aux[0x39c]) ||
        !prefix_special_guard(&work,io,&q)) {
        s->required_helper=work.required_helper;return POP_CONTROLLER_HELPER_REQUIRED;
    }
    *s=work;return POP_CONTROLLER_COMPLETE;
}
PopControllerResult pop_player_sample_tick_input(PopControllerState *s,
    uint8_t h,uint8_t v,uint8_t b,const PopControllerIO *io) {
    PopControllerState work;PopControllerQueryResult q={0,0,0,false,false,false};
    PopControllerResult result;uint8_t *a,event,value;uint16_t transfer=0;
    if (!s) return POP_CONTROLLER_INVALID;
    work=*s;work.required_helper=0;work.selected_sequence=0;a=work.aux;
    /*258D executes before input/controller selection. */
    if (!room0_response(&work,io,&q)) {
        s->required_helper=work.required_helper;return POP_CONTROLLER_HELPER_REQUIRED;
    }
    result=pop_controller_sampled_player(&work,h,v,b,io);
    if (result!=POP_CONTROLLER_COMPLETE) {s->required_helper=work.required_helper;return result;}
    if ((a[0x4f]&128u) || (a[0x40]!=185 && a[0x40]!=177 && a[0x40]!=178)) {
        *s=work;return POP_CONTROLLER_COMPLETE;
    }
    if (a[0x4f]==0) {
        event=a[0x6d]==1?6:(a[0xd3]?2:1);q.x=255;
        if (!tick_query(&work,io,&q,0xd930,event)) {
            s->required_helper=work.required_helper;return POP_CONTROLLER_HELPER_REQUIRED;
        }
    }
    if (a[0x4f]<4) ++a[0x4f];
    else if (a[0x3f4]==0) transfer=0x25aa;
    else if (a[0x319]==0) {
        if ((uint8_t)(a[0x300]|a[0x302])==0) transfer=0x2503;
        else {
            value=a[0xdb];
            if (a[0x30b]!=2 || value==0) {a[0x30b]=2;a[0xdb]=255;value=255;}
            if (value==1) transfer=0x25aa;
            else if ((a[0x1a]|a[0x1b])&128u) transfer=0x20e2;
        }
    }
    if (transfer) {
        *s=work;
        return transfer==0x20e2?POP_CONTROLLER_RESTART_LEVEL:
               transfer==0x25aa?POP_CONTROLLER_GAME_OVER:POP_CONTROLLER_TIME_EXPIRED;
    }
    *s=work;return POP_CONTROLLER_COMPLETE;
}
void pop_player_tick_modifier(PopControllerState *s) {
    uint8_t timer;
    if (!s || s->aux[0xc5]==0) return;
    timer=(uint8_t)(s->aux[0xc5]-1u);s->aux[0xc5]=timer;
    s->aux[0x318]=timer==0?0:(timer<15?(uint8_t)(s->aux[0x318]^255u):255);
}
