#include "loop_state.h"
#include <stddef.h>
#include <string.h>

typedef struct Loop {
    PopControllerState *s;PopControllerQueryResult *r;
    const PopLoopHardware *hardware;const PopControllerIO *io;PopLoopWrite write;
} Loop;
static void nz(PopControllerQueryResult *r,uint8_t value) {r->zero=value==0;r->negative=(value&128u)!=0;}
static void a(PopControllerQueryResult *r,uint8_t value) {r->a=value;nz(r,value);}
static void x(PopControllerQueryResult *r,uint8_t value) {r->x=value;nz(r,value);}
static void cmp(PopControllerQueryResult *r,uint8_t first,uint8_t second) {r->carry=first>=second;nz(r,(uint8_t)(first-second));}
static bool need(Loop *l,uint16_t id) {l->s->required_helper=id;return false;}
static bool query(Loop *l,uint16_t id) {
    if (l->io&&l->io->query&&l->io->query(l->io->context,id,l->r->a,l->s,l->r)) return true;
    return need(l,id);
}
static bool write_value(Loop *l,uint16_t address,uint8_t value) {
    if(l->write&&l->io&&l->write(l->io->context,address,value))return true;
    return need(l,address);
}
static void random_step(Loop *l) {
    uint8_t old=l->s->aux[0x9e],four=(uint8_t)(old*4u);
    unsigned sum=(unsigned)four+old;
    a(l->r,(uint8_t)sum);sum=(unsigned)l->r->a+23u;
    l->r->carry=sum>255u;a(l->r,(uint8_t)sum);l->s->aux[0x9e]=l->r->a;
}
static bool before_frame(Loop *l) {
    uint8_t *s=l->s->aux;PopControllerQueryResult *r=l->r;
    a(r,s[0xdf]);
    if(!r->zero&&!r->negative) {
        --s[0xdf];nz(r,s[0xdf]);
        if(r->zero){--s[0xdf];nz(r,s[0xdf]);}
    }
    a(r,s[0x3f4]);cmp(r,r->a,8);if(!r->zero)return true;
    a(r,s[0x4b]);cmp(r,r->a,16);if(!r->zero)return true;
    a(r,s[0x86]);if(r->zero)return true;
    cmp(r,r->a,150);
    if(r->carry) {if(!r->zero)return true;if(!query(l,0xf912))return false;}
    ++s[0x86];nz(r,s[0x86]);return true;
}
static bool before_render(Loop *l) {
    uint8_t *s=l->s->aux;PopControllerQueryResult *r=l->r;
    a(r,s[0x88]);
    if(!r->zero){a(r,s[0x89]);if(!r->zero)return query(l,0xe003);}
    a(r,s[0xcf]);if(!r->negative)return true;
    a(r,17);return query(l,0xe003);
}
static bool after_audio(Loop *l) {
    uint8_t *s=l->s->aux;PopControllerQueryResult *r=l->r;
    a(r,s[0x88]);
    if(!r->zero) {--s[0x88];nz(r,s[0x88]);if(!r->negative)return query(l,0xe00f);}
    a(r,s[0xcf]);if(!r->negative)return true;
    return query(l,0xe00f);
}
static void pose_eligible(PopControllerQueryResult *r) {
    static const uint8_t values[]={0,15,229,109,171,166,185,177,178};
    unsigned i;
    for(i=0;i<sizeof(values);++i){cmp(r,r->a,values[i]);if(r->zero){a(r,0);return;}}
    a(r,1);
}
static bool message_eligibility(Loop *l,bool *ready) {
    uint8_t *s=l->s->aux,value;PopControllerQueryResult *r=l->r;
    *ready=false;x(r,s[0x319]);if(r->zero)return true;
    a(r,s[0x3f4]);if(r->zero)return true;
    a(r,s[0x308]);
    if(r->zero){a(r,0);s[0x319]=0;return true;}
    --s[0x308];nz(r,s[0x308]);
    a(r,s[0x50]);
    if(r->zero){a(r,s[0x31c]);cmp(r,r->a,s[0x3f4]);if(r->zero)return true;}
    a(r,s[0x50]);pose_eligible(r);if(!r->zero)return true;
    a(r,s[0x63]);cmp(r,r->a,86);
    if(!r->zero){a(r,s[0x6b]);cmp(r,r->a,s[0xcb]);if(r->zero){a(r,s[0x60]);pose_eligible(r);if(!r->zero)return true;}}
    if(!l->io||!l->io->read||!l->io->read(l->io->context,0xb6e0,&value))return need(l,0xb6e0);
    a(r,value);if(!r->zero)return true;
    a(r,s[0x30d]);if(!r->zero)return true;
    a(r,s[0x88]);if(!r->zero)return true;
    a(r,s[0xdf]);
    if(!r->negative){if(!r->zero)return true;a(r,(uint8_t)(s[0xcf]|s[0xd2]));if(!r->zero)return true;}
    *ready=true;return true;
}
static bool pending_message(Loop *l) {
    bool ready;
    if(!message_eligibility(l,&ready))return false;
    return ready?need(l,0xd921):true;
}
static void clear_latches(Loop *l) {
    uint8_t *s=l->s->aux;PopControllerQueryResult *r=l->r;
    x(r,4);
    do{a(r,s[0x30e + r->x]);s[0xd4+r->x]=r->a;x(r,(uint8_t)(r->x-1u));}while(!r->negative);
    a(r,0);s[0xd4]=0;s[0xd5]=0;s[0xd6]=0;s[0xd7]=0;
    x(r,4);
    do{a(r,s[0xd4+r->x]);s[0x30e + r->x]=r->a;x(r,(uint8_t)(r->x-1u));}while(!r->negative);
}
static bool demo_exit(Loop *l) {
    uint8_t *s=l->s->aux;PopControllerQueryResult *r=l->r;
    a(r,s[0x3f4]);if(!r->zero){a(r,0);return true;}
    if(!l->hardware)return need(l,0xc061);
    a(r,(uint8_t)(l->hardware->button0|l->hardware->button1));
    if(r->negative){a(r,255);return true;}
    a(r,s[0x20f]);if(!r->negative){a(r,0);return true;}
    cmp(r,r->a,0x9b);if(r->zero){a(r,0);return true;}
    cmp(r,r->a,0x93);a(r,r->zero?0:255);return true;
}
void pop_loop_clear_frame_flags(PopControllerState *s) {if(s){s->aux[0xcf]=0;s->aux[0xd2]=0;}}
static bool read_value(Loop *l,uint16_t address,uint8_t *value) {
    if(l->io&&l->io->read&&l->io->read(l->io->context,address,value))return true;
    return need(l,address);
}
static bool paused_objects(Loop *l,PopLoopModal *modal) {
    uint8_t *s=l->s->aux,lo,hi,raw,cell,column,row,height;unsigned pointer,spec;
    PopControllerQueryResult *r=l->r;
    a(r,0);s[0xe8]=0;a(r,s[0xcb]);
    if(r->a==0) {
        s[0x24]=0x0f;s[0x25]=0x96;s[0x26]=0x2d;s[0x27]=0x96;
        r->carry=true;a(r,0x96);
    }else{
        if(r->a>24)return need(l,0x04cc);
        x(r,(uint8_t)(2u*(r->a-1u)));
        if(!read_value(l,(uint16_t)(0x1220u+r->x),&lo)||!read_value(l,(uint16_t)(0x1221u+r->x),&hi))return false;
        pointer=(0xb700u+lo+256u*hi)&65535u;spec=(pointer+720u)&65535u;
        s[0x24]=(uint8_t)pointer;s[0x25]=(uint8_t)(pointer>>8);
        s[0x26]=(uint8_t)spec;s[0x27]=(uint8_t)(spec>>8);
        r->carry=false;a(r,s[0x27]);
    }
    r->y=29;nz(r,r->y);
    for(;;){
        pointer=(unsigned)s[0x24]+256u*s[0x25];
        if(!read_value(l,(uint16_t)(pointer+r->y),&raw))return false;
        a(r,(uint8_t)(raw&31u));cmp(r,r->a,19);
        if(r->zero){
            if(!read_value(l,(uint16_t)(0x5e79u+r->y),&raw))return false;
            a(r,raw);s[0x13]=raw;cell=r->y;column=(uint8_t)(cell%10u);row=(uint8_t)(cell/10u);
            /* The original's col9 rejection has a nonlocal stack effect;
             * this source-owned unit contract covers actual valid records. */
            if(column==9)return need(l,0xdc34);
            x(r,row);a(r,column);cmp(r,r->a,10);x(r,s[0xe8]);r->y=row;nz(r,row);
            if(!read_value(l,(uint16_t)(0x127au+row),&height))return false;
            a(r,height);r->carry=r->a>=3;a(r,(uint8_t)(r->a-3u));modal->paused_objects[9+r->x]=r->a;
            a(r,s[0x13]);modal->paused_objects[27+r->x]=r->a;
            a(r,(uint8_t)(column+1u));cmp(r,r->a,10);a(r,(uint8_t)(r->a<<2));r->carry=false;modal->paused_objects[r->x]=r->a;
            a(r,cell);r->y=r->a;nz(r,r->y);spec=(unsigned)s[0x26]+256u*s[0x27];
            if(!read_value(l,(uint16_t)(spec+r->y),&raw))return false;
            a(r,raw);modal->paused_objects[18+r->x]=raw;++s[0xe8];nz(r,s[0xe8]);
        }
        x(r,s[0xe8]);cmp(r,r->x,8);if(r->carry)break;
        --r->y;nz(r,r->y);if(r->negative){x(r,s[0xe8]);break;}
    }
    a(r,255);modal->paused_objects[r->x]=255;modal->paused_objects[36]=255;return true;
}
void pop_loop_modal_reset(PopLoopModal *modal) {if(modal)memset(modal,0,sizeof *modal);}
bool pop_loop_modal_torch_step(PopLoopModal *modal,PopControllerState *state,PopLoopPausedDraw *draw) {
    uint8_t index,old,next;PopLoopPausedDraw item={0};
    if(!modal||!state||!draw||!modal->active)return false;
    if(modal->paused_objects[0]&128u){*draw=item;return true;}
    index=modal->paused_objects[36];
    if(index!=255&&index>7)return false;
    index=(uint8_t)(index+1u);
    if(modal->paused_objects[index]&128u)index=0;
    old=modal->paused_objects[18u+index];
    next=(uint8_t)(state->aux[0x9e]*5u+23u);
    state->aux[0x9e]=next;state->aux[0xe8]=old;
    if(next==old||next>=18){next=(uint8_t)(old+1u);if(next>=18)next=0;}
    modal->paused_objects[36]=index;modal->paused_objects[18u+index]=next;
    item=(PopLoopPausedDraw){modal->paused_objects[index],modal->paused_objects[9u+index],
                            next,modal->paused_objects[27u+index],true};
    *draw=item;return true;
}
bool pop_loop_build_paused_objects(PopLoopModal *modal,PopControllerState *s,
    PopControllerQueryResult *r,const PopControllerIO *io) {
    PopLoopModal m;PopControllerState state;PopControllerQueryResult result;Loop l;
    if(!modal||!s||!r)return false;
    m=*modal;state=*s;result=*r;l=(Loop){&state,&result,NULL,io,NULL};
    if(!paused_objects(&l,&m)){s->required_helper=state.required_helper;return false;}
    *modal=m;*s=state;*r=result;return true;
}
PopLoopModalResult pop_loop_modal_begin(PopLoopModal *modal,PopControllerState *s,const PopControllerIO *io) {
    PopLoopModal m;PopControllerState state;PopControllerQueryResult r={0};Loop l;bool ready;
    if(!modal||!s)return POP_LOOP_MODAL_INVALID;
    if(modal->active)return POP_LOOP_MODAL_ACTIVE;
    m=*modal;state=*s;l=(Loop){&state,&r,NULL,io,NULL};
    if(!message_eligibility(&l,&ready)){s->required_helper=state.required_helper;return POP_LOOP_MODAL_REQUIRED;}
    if(!ready){*s=state;return POP_LOOP_MODAL_IDLE;}
    a(&r,(uint8_t)(state.aux[0]^32u));state.aux[0]=r.a;
    if(!paused_objects(&l,&m)){s->required_helper=state.required_helper;return POP_LOOP_MODAL_REQUIRED;}
    m.resource=state.aux[0x319];m.active=true;*s=state;*modal=m;return POP_LOOP_MODAL_OPENED;
}
bool pop_loop_modal_dismiss(PopLoopModal *modal,PopControllerState *s) {
    PopControllerQueryResult r={0};Loop l;
    if(!modal||!s||!modal->active)return false;
    s->aux[0x319]=0;s->aux[0]^=32u;l=(Loop){s,&r,NULL,NULL,NULL};clear_latches(&l);
    modal->active=false;return true;
}
bool pop_loop_visual_event(uint16_t helper,uint8_t argument,PopControllerState *s,PopLoopVisual *visual) {
    if(!s||!visual)return false;
    if(helper==0xe003){visual->fill_required=s->aux[0x31d]!=argument;s->aux[0x31d]=argument;visual->fill_byte=argument;visual->page=1;visual->low_resolution=true;return true;}
    if(helper==0xe00f){if(s->aux[0]==0)visual->page=2;visual->low_resolution=false;visual->fill_required=false;return true;}
    return false;
}
bool pop_loop_state_query(uint16_t helper,uint8_t argument,PopControllerState *s,
    PopControllerQueryResult *r,const PopLoopHardware *hardware,
    const PopControllerIO *io,PopLoopWrite write) {
    PopControllerState state;PopControllerQueryResult result;Loop l;bool ok;
    (void)argument;
    if(!s||!r)return false;
    state=*s;result=*r;l=(Loop){&state,&result,hardware,io,write};
    switch(helper){
    case 0x0436:case 0x0a4f:random_step(&l);ok=true;break;
    case 0x279f:ok=before_frame(&l);break;
    case 0x249c:ok=before_render(&l);break;
    case 0x24ad:ok=after_audio(&l);break;
    case 0x2641:ok=pending_message(&l);break;
    case 0x2765:pose_eligible(&result);ok=true;break;
    case 0x278f:clear_latches(&l);ok=true;break;
    case 0xd906:case 0xdbb1:a(&result,0);ok=write_value(&l,0xb6c0,0);break;
    case 0xd91e:case 0xdbcd:ok=demo_exit(&l);break;
    default:return false;
    }
    if(!ok){s->required_helper=state.required_helper;return false;}
    *s=state;*r=result;return true;
}
