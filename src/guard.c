#include "guard.h"
#include "world.h"
#include "data.h"
#include <stddef.h>
bool pop_guard_memory_reset(PopGuardMemory *m) {
    PopGuardMemory work;unsigned i;
    if (!m) return false;
    for (i=0;i<POP_GUARD_RECORD_BYTES;++i)
        if (!pop_world_initial_info(0x47u+i,&work.bytes[i])) return false;
    *m=work;return true;
}
bool pop_guard_memory_read(void *context,uint16_t address,uint8_t *value) {
    const PopGuardMemory *m=context;
    if (!m || !value || address<0xbf47u || address>=0xbfd7u) return false;
    *value=m->bytes[address-0xbf47u];return true;
}
bool pop_guard_memory_write(void *context,uint16_t address,uint8_t value) {
    PopGuardMemory *m=context;
    if (!m || address<0xbf47u || address>=0xbfd7u) return false;
    m->bytes[address-0xbf47u]=value;return true;
}
typedef struct GuardDecision {
    PopControllerState *state;const PopControllerIO *io;PopControllerQueryResult result;
} GuardDecision;
static bool g_query(GuardDecision *g,uint16_t helper,uint8_t argument) {
    g->result.a=argument;
    if (!g->io || !g->io->query ||
        !g->io->query(g->io->context,helper,argument,g->state,&g->result)) {
        g->state->required_helper=helper;return false;
    }
    return true;
}
static bool g_next(GuardDecision *g,uint16_t helper) {return g_query(g,helper,g->result.a);}
static bool g_read(GuardDecision *g,uint16_t address,uint8_t *value) {
    if (pop_data_read(NULL,address,value) ||
        (g->io && g->io->read && g->io->read(g->io->context,address,value))) return true;
    g->state->required_helper=address;return false;
}
void pop_guard_dismiss(PopControllerState *s) {
    if (!s) return;
    s->aux[0x43]=86;s->aux[0x46]=0;s->aux[0x4f]=0;
    s->aux[0xd2]=(uint8_t)(0u-s->aux[0xcc]);
}
static bool notify_death(GuardDecision *g) {
    uint8_t *s=g->state->aux;
    if (s[0x3f4]==0) {s[0x212]=1;s[0x309]=0;s[0xa0]=0;return true;}
    if (s[0x3f4]!=13 && s[0x4d]==1) return true;
    g->result.x=25;
    if (!g_query(g,0xd930,s[0x3f4]==13?9:7)) return false;
    if (s[0x3f4]!=13) return true;
    s[0x89]=255;s[0x88]=10;s[0x86]=1;s[0x31f]=4;
    g->result.x=0;g->result.y=0;
    return g_query(g,0xd021,24) && g_next(g,0xee06);
}
PopControllerResult pop_guard_notify_death(PopControllerState *s,const PopControllerIO *io) {
    PopControllerState work;GuardDecision g;
    if (!s) return POP_CONTROLLER_INVALID;
    work=*s;work.required_helper=0;
    g=(GuardDecision){&work,io,{0,0,0,false,false,false}};
    if (!notify_death(&g)) {s->required_helper=work.required_helper;return POP_CONTROLLER_HELPER_REQUIRED;}
    *s=work;return POP_CONTROLLER_COMPLETE;
}
static void advance(uint8_t *s) {s[0xd4]=255;s[0x18]=255;}
static void retreat(uint8_t *s) {s[0xd5]=255;s[0x18]=1;}
static void down(uint8_t *s) {s[0xd7]=255;s[0x19]=1;}
static void parry(uint8_t *s) {s[0xd6]=255;s[0x19]=255;}
static void attack(uint8_t *s) {s[0xd8]=255;s[0x3d]=255;}
/*5862→0436→AUX0A4F. Original ASL/ADC sequence discards each high bit. */
static uint8_t random_byte(uint8_t *s) {
    s[0x9e]=(uint8_t)((unsigned)s[0x9e]*5u+23u);return s[0x9e];
}
static bool random_below(GuardDecision *g,uint16_t table,bool *yes) {
    uint8_t value=random_byte(g->state->aux),threshold;
    if (!g_read(g,(uint16_t)(table+g->state->aux[0xdd]),&threshold)) return false;
    *yes=value<threshold;return true;
}
static bool distance(GuardDecision *g,uint8_t *value) {
    if (!g_next(g,0xd0ae)) return false;
    *value=g->result.a;return true;
}
/*5718: pursue an opponent one row below only across a supported clear edge. */
static bool chase(GuardDecision *g) {
    uint8_t *s=g->state->aux;
    if (s[0x3a2]==2 || s[0x3a2]==6) return true;
    if (!g_next(g,0xd0bd)) return false;
    s[0xf0]=g->result.a;
    if (!g_next(g,0xd04e)) return false;
    if (g->result.zero) {
        if (!g_query(g,0xd04b,s[0xf0])) return false;
        if (!g->result.zero) {advance(s);return true;}
        if (!g_next(g,0xd0bd)) return false;
        ++s[0x92];if (!g_next(g,0xd024)) return false;s[0xf0]=g->result.a;
        if (g->result.a!=2 && g->result.a!=11) {
            if (!g_next(g,0xd04e)) return false;
            if (g->result.zero) {
                if (!g_query(g,0xd04b,s[0xf0])) return false;
                if (!g->result.zero && (uint8_t)(s[0x45]+1u)==s[0x3a1]) {advance(s);return true;}
            }
        }
    }
    s[0xe7]=0;retreat(s);return true;
}
static bool attempt_parry(GuardDecision *g) {
    uint8_t *s=g->state->aux;bool yes;
    if (s[0x39c]!=152 && s[0x39c]!=153 && s[0x39c]!=162) return true;
    if (!random_below(g,s[0x7e]?0x5440:0x5434,&yes)) return false;
    if (yes) parry(s);
    return true;
}
static bool attempt_strike(GuardDecision *g) {
    uint8_t *s=g->state->aux;bool yes;
    if (s[0x39c]==169 || s[0x39c]==151) return true;
    if (!random_below(g,(s[0x40]==161 || s[0x40]==150)?0x5428:0x541c,&yes)) return false;
    if (yes) attack(s);
    return true;
}
static bool approach_probability(GuardDecision *g) {
    uint8_t *s=g->state->aux;bool yes;
    if (s[0xdd] && s[0x7f]) return true;
    if (!random_below(g,0x544c,&yes)) return false;
    if (yes) advance(s);
    return true;
}
static bool melee(GuardDecision *g) {
    uint8_t *s=g->state->aux,d;
    if (!distance(g,&d)) return false;
    if (d>=10 && d<29) {
        if (!attempt_parry(g)) return false;
        if (s[0xa1]) return true;
        if (!distance(g,&d)) return false;
        if (d>=12 && d<29) return attempt_strike(g);
    }
    return approach_probability(g);
}
static bool armed(GuardDecision *g) {
    uint8_t *s=g->state->aux,d,pose;
    if (s[0x40]==166 || s[0x40]<150) return true;
    if (s[0xd1]<2) {
        if (s[0xd1]==1) return true;
        if (s[0xe7]) return chase(g);
        if (s[0x4d]==4) return true;
        s[0xd7]=255;retreat(s);return true;
    }
    if (!distance(g,&d)) return false;
    if (!(d&128u) && d>=12 && s[0x39c]>=102 && s[0x39c]<118 && s[0x3a2]==5) return true;
    if (!distance(g,&d)) return false;
    if (d<35) {
        if (d<(s[0x4e]>=2?12u:8u)) {
            if (s[0x43]==s[0x39f]) retreat(s);else advance(s);
            return true;
        }
        if (s[0x3aa]==2) return melee(g);
        if (s[0xa1]) return true;
        if (!distance(g,&d)) return false;
        if (d>=29) advance(s);else attack(s);return true;
    }
    if (s[0xa1]) return true;
    if (s[0x43]!=s[0x39f]) {
        pose=s[0x39c];
        if (pose>=7 && pose<15) {
            if (!distance(g,&d)) return false;
            if (d<40) attack(s);
            return true;
        }
        if (pose>=34 && pose<44) {
            if (!distance(g,&d)) return false;
            if (d<50) attack(s);
            return true;
        }
    }
    if (!g_next(g,0xd0bd) || !g_next(g,0xd04b)) return false;
    if (!g->result.zero) {
        if (!g_next(g,0xd093) || !g_next(g,0xd04b)) return false;
        if (!g->result.zero) {advance(s);return true;}
    }
    retreat(s);return true;
}
static bool ordinary_ai(GuardDecision *g) {
    uint8_t *s=g->state->aux,d;bool notice=false;
    if (s[0x4e]>=2) return armed(g);
    if (!(s[0x5f]&128u)) return true;
    if (!distance(g,&d)) return false;
    if (s[0x3a1]==s[0x45] && d>=248) notice=true;
    else {
        if (s[0xe4]) {
            s[0xe4]=0;
            if (d<128) notice=true;
            else if (d<252) {down(s);return true;}
        }
        if (!notice && d>=128) return true;
        notice=true;
    }
    if (notice && s[0xd1] && !(s[0x3f4]==13 && s[0x319])) {s[0xd7]=255;advance(s);}
    return true;
}
static bool build_inputs(GuardDecision *g) {
    uint8_t *s=g->state->aux;unsigned i;
    for (i=0;i<5;++i) s[0xd4u+i]=0;
    s[0x18]=0;s[0x19]=0;s[0x3d]=0;
    if (s[0x4d]) {
        if (s[0x7e]) --s[0x7e];
        if (s[0x7f]) --s[0x7f];
        if (s[0xa1]) --s[0xa1];
        if (s[0x4d]==24) {
            if (s[0x43]==86) return true;
            if (s[0x46]==0) {if (s[0x41]>=200) pop_guard_dismiss(g->state);return true;}
            if (s[0x41]>=166) return true;
            pop_controller_select_sequence(g->state,107);return g_query(g,0x450c,s[0x4a]);
        }
        if (s[0x4d]==4) s[0x4e]=2;
        else if (s[0x4d]==1) {
            if (s[0x3f4]!=4 && s[0x3f4]!=5 && s[0x3f4]!=6 && s[0x3f4]!=12) return true;
            g->state->required_helper=0x54fa;return false; /* Special non-level1 actor. */
        }
    }
    return ordinary_ai(g);
}
PopControllerResult pop_guard_build_inputs(PopControllerState *s,const PopControllerIO *io) {
    PopControllerState work;GuardDecision g;
    if (!s) return POP_CONTROLLER_INVALID;
    work=*s;work.required_helper=0;work.selected_sequence=0;
    g=(GuardDecision){&work,io,{0,0,0,false,false,false}};
    if (!build_inputs(&g)) {s->required_helper=work.required_helper;return POP_CONTROLLER_HELPER_REQUIRED;}
    *s=work;return POP_CONTROLLER_COMPLETE;
}
PopControllerResult pop_guard_decide(PopControllerState *s,const PopControllerIO *io) {
    PopControllerState work;GuardDecision g;PopControllerResult result;
    if (!s) return POP_CONTROLLER_INVALID;
    work=*s;work.required_helper=0;work.selected_sequence=0;
    g=(GuardDecision){&work,io,{0,0,0,false,false,false}};
    if (work.aux[0x4d]!=24) {
        if (!(work.aux[0x4f]&128u) || work.aux[0xcc]==0) {
            if (work.aux[0x4f]&128u) {
                work.aux[0x4f]=0;
                if (!notify_death(&g)) {s->required_helper=work.required_helper;return POP_CONTROLLER_HELPER_REQUIRED;}
            }
            if (work.aux[0x4d]==1) {
                pop_guard_dismiss(&work);
                *s=work;return POP_CONTROLLER_COMPLETE;
            }
        }
        if (work.aux[0xde]) {s->required_helper=0x0481;return POP_CONTROLLER_HELPER_REQUIRED;}
    }
    if (!build_inputs(&g)) {s->required_helper=work.required_helper;return POP_CONTROLLER_HELPER_REQUIRED;}
    if (work.aux[0x4d]!=24) {
        result=pop_controller_decide(&work,io);
        if (result!=POP_CONTROLLER_COMPLETE) {s->required_helper=work.required_helper;return result;}
    }
    *s=work;return POP_CONTROLLER_COMPLETE;
}
PopControllerResult pop_guard_load_room(PopControllerState *s,const PopControllerIO *io) {
    PopControllerState work;GuardDecision g;uint8_t *a,cell,row,high,low,skill,base,color;
    unsigned i;uint16_t address;
    if (!s) return POP_CONTROLLER_INVALID;
    work=*s;work.required_helper=0;work.selected_sequence=0;a=work.aux;
    g=(GuardDecision){&work,io,{0,0,0,false,false,false}};
    a[0x8a]=0;
    if (a[0x3f4]==5 || a[0x3f4]==6 || a[0x3f4]==12) {s->required_helper=0x5c80;return POP_CONTROLLER_HELPER_REQUIRED;}
    address=(uint16_t)(0xbf46u+a[0xcb]);if (!g_read(&g,address,&cell)) goto missing;
    if (cell>=30) {*s=work;return POP_CONTROLLER_COMPLETE;}
    a[0x4b]=a[0xcb];row=(uint8_t)(cell/10u);a[0x45]=row;
    if (!g_read(&g,(uint16_t)(0x127fu+row),&a[0x42]) ||
        !g_read(&g,(uint16_t)(0xbf76u+a[0xcb]),&a[0x41]) ||
        !g_query(&g,0xd00c,a[0x41])) goto missing;
    a[0x44]=g.result.a;
    if (!g_read(&g,(uint16_t)(0xbf5eu+a[0xcb]),&a[0x43])) goto missing;
    a[0x4d]=a[0x3f4]==3?4:2;
    if (!g_read(&g,(uint16_t)(0xbfbeu+a[0xcb]),&high)) goto missing;
    if (high==0) {
        a[0x4e]=a[0x4d]==4?2:0;
        pop_controller_select_sequence(&work,a[0x4d]==4?63:77);
    } else {
        a[0x4a]=high;
        if (!g_read(&g,(uint16_t)(0xbf8eu+a[0xcb]),&a[0x49])) goto missing;
    }
    if (!g_query(&g,0x450c,a[0x4a])) goto missing;
    if (a[0x40]==185 || a[0x40]==177 || a[0x40]==178) {a[0x4f]=1;a[0xcc]=0;}
    else {
        a[0x4f]=255;a[0xe4]=0;a[0xa1]=0;a[0x7e]=0;
        /*5DAF uses the prior DD; the room's retained skill is read later. */
        if (!g_read(&g,(uint16_t)(0x547cu+a[0x3f4]),&base) ||
            !g_read(&g,(uint16_t)(0x5470u+a[0xdd]),&low)) goto missing;
        a[0xdc]=(uint8_t)(base+low);a[0xcc]=a[0xdc];
    }
    a[0x47]=0;a[0x48]=0;a[0x46]=1;
    if (!g_read(&g,(uint16_t)(0xbfa6u+a[0xcb]),&skill)) goto missing;
    a[0xdd]=skill<12?skill:3;
    if (!g_read(&g,(uint16_t)(0x548au+a[0x3f4]),&color) ||
        !g_read(&g,(uint16_t)(0x5464u+a[0xdd]),&low)) goto missing;
    a[0x216]=(uint8_t)(color^low);
    for (i=0;i<16;++i) a[0x60u+i]=a[0x40u+i];
    *s=work;return POP_CONTROLLER_COMPLETE;
missing:
    s->required_helper=work.required_helper;return POP_CONTROLLER_HELPER_REQUIRED;
}
