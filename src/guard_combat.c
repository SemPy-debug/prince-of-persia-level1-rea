#include "guard_combat.h"
#include "data.h"
#include <stddef.h>
#include <stdbool.h>
typedef struct Combat {
    PopControllerState *state;const PopControllerIO *io;PopControllerQueryResult result;
} Combat;
static bool query(Combat *c,uint16_t helper,uint8_t argument) {
    c->result.a=argument;
    if (c->io && c->io->query && c->io->query(c->io->context,helper,argument,c->state,&c->result)) return true;
    c->state->required_helper=helper;return false;
}
static bool read_value(Combat *c,uint16_t address,uint8_t *value) {
    if (pop_data_read(NULL,address,value) ||
        (c->io && c->io->read && c->io->read(c->io->context,address,value))) return true;
    c->state->required_helper=address;return false;
}
static void copy_actor(uint8_t *s,unsigned destination,unsigned source) {
    unsigned i;for (i=0;i<16;++i) s[destination+i]=s[source+i];
}
static void load_pair(uint8_t *s,bool guard) {
    copy_actor(s,0x40,guard?0x60:0x50);copy_actor(s,0x39c,guard?0x50:0x60);
}
static void save_pair(uint8_t *s,bool guard) {
    copy_actor(s,guard?0x60:0x50,0x40);copy_actor(s,guard?0x50:0x60,0x39c);
}
static bool animate(Combat *c,uint8_t sequence) {
    pop_controller_select_sequence(c->state,sequence);
    return query(c,0x450c,c->state->aux[0x4a]);
}
/* Original5886 runs once with each actor as the working actor. */
static bool sword_contact(Combat *c) {
    uint8_t *s=c->state->aux,distance,threshold;
    if (s[0x4e]!=2 || s[0x45]!=s[0x3a1] || (s[0x40]!=153 && s[0x40]!=154)) return true;
    if (!query(c,0xd0ae,0)) return false;
    distance=c->result.a;
    if (distance<29 && (s[0x39c]==161 || s[0x39c]==150)) {
        s[0x39c]=161;
        if (s[0x4d]) s[0x7e]=4;
        return animate(c,69);
    }
    if (s[0x40]!=154) return true;
    if (!query(c,0xd0ae,0)) return false;
    threshold=s[0x3aa]<2?8:12;
    if (c->result.a>=threshold && c->result.a<29) s[0x3a2]=99;
    return true;
}
static bool contact_pair(Combat *c) {
    uint8_t *s=c->state->aux;
    if (!s[0x50] || (s[0x50]>=219 && s[0x50]<229)) return true;
    load_pair(s,true);
    if (!sword_contact(c)) return false;
    save_pair(s,true);load_pair(s,false);
    if (!sword_contact(c)) return false;
    save_pair(s,false);return true;
}
/* F915/FABD: queued damage first, then source fatal/nonfatal sequence. */
static bool take_hit(Combat *c) {
    uint8_t *s=c->state->aux,sequence,displacement,height;bool fatal;
    if (!(s[0x4f]&128u)) return true;
    if (s[0x4e]!=2) {
        if (!query(c,0xf927,100)) return false;
        fatal=true;
    } else if (s[0x4d]==4) fatal=false;
    else {
        if (!query(c,0xf927,1)) return false;
        fatal=c->result.a==0;
    }
    if (fatal) {
        if (!query(c,0xd0c0,0)) return false;
        if (!c->result.a) {
            if (!query(c,0xd018,0)) return false;
            if (c->result.a>=4) {
                displacement=(uint8_t)(c->result.a-14u);
                if (s[0x43]&128u) displacement=(uint8_t)(0u-displacement);
                s[0x41]=(uint8_t)(s[0x41]+displacement);++s[0x45];
                pop_controller_select_sequence(c->state,81);
                return query(c,0xd909,5) && query(c,0x450c,s[0x4a]);
            }
        }
        sequence=85;
    } else sequence=74;
    pop_controller_select_sequence(c->state,sequence);
    if (!read_value(c,(uint16_t)(0x127fu+s[0x45]),&height)) return false;
    s[0x42]=height;s[0x48]=0;
    return query(c,0xd909,5) && query(c,0x450c,s[0x4a]);
}
static bool resolve_hits(Combat *c) {
    uint8_t *s=c->state->aux,delay;
    if (s[0x66]==99) {
        if (s[0x56]==99) s[0x56]=1;
        copy_actor(s,0x40,0x60);
        if (!take_hit(c)) return false;
        copy_actor(s,0x60,0x40);
        /*5862 loadsX=DD and tail-calls0436/0A4F, even for an inert hit. */
        s[0x9e]=(uint8_t)(5u*s[0x9e]+23u);
        if (!read_value(c,(uint16_t)(0x5458u+s[0xdd]),&delay)) return false;
        s[0xa1]=delay;
    }
    if (s[0x56]==99) {
        copy_actor(s,0x40,0x50);
        if (!take_hit(c)) return false;
        copy_actor(s,0x50,0x40);
    }
    return true;
}
static PopControllerResult run(PopControllerState *s,const PopControllerIO *io,bool (*decide)(Combat *)) {
    PopControllerState work;Combat c;
    if (!s) return POP_CONTROLLER_INVALID;
    work=*s;work.required_helper=0;work.selected_sequence=0;
    c=(Combat){&work,io,{0,0,0,false,false,false}};
    if (!decide(&c)) {s->required_helper=work.required_helper;return POP_CONTROLLER_HELPER_REQUIRED;}
    *s=work;return POP_CONTROLLER_COMPLETE;
}
PopControllerResult pop_guard_check_sword_contact(PopControllerState *s,const PopControllerIO *io) {
    return run(s,io,contact_pair);
}
PopControllerResult pop_guard_resolve_sword_hits(PopControllerState *s,const PopControllerIO *io) {
    return run(s,io,resolve_hits);
}
