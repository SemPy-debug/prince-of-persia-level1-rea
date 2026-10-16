#include "world_tick.h"
#include "world.h"
#include "data.h"
#include "geometry.h"
#include "guard.h"
#include <stddef.h>
static bool neighbor(uint8_t room,PopWorldDirection direction,uint8_t *out) {
    if (!room) {*out=0;return true;}
    return pop_world_initial_neighbor(room,direction,out);
}
bool pop_world_tick_room_links(PopControllerState *s) {
    PopControllerState work;uint8_t *a;
    if (!s) return false;
    work=*s;a=work.aux;
    if (!neighbor(a[0xcb],POP_WORLD_LEFT,&a[0x31]) ||
        !neighbor(a[0xcb],POP_WORLD_RIGHT,&a[0x32]) ||
        !neighbor(a[0xcb],POP_WORLD_ABOVE,&a[0x33]) ||
        !neighbor(a[0xcb],POP_WORLD_BELOW,&a[0x34]) ||
        !neighbor(a[0x34],POP_WORLD_LEFT,&a[0x35]) ||
        !neighbor(a[0x34],POP_WORLD_RIGHT,&a[0x38]) ||
        !neighbor(a[0x33],POP_WORLD_LEFT,&a[0x36]) ||
        !neighbor(a[0x33],POP_WORLD_RIGHT,&a[0x37])) {s->required_helper=0xd02a;return false;}
    *s=work;return true;
}
bool pop_world_tick_transfer_actor(PopControllerState *s,uint8_t direction) {
    uint8_t room;
    if (!s || direction>3) return false;
    if (!neighbor(s->aux[0x4b],(PopWorldDirection)direction,&room)) {
        s->required_helper=(uint16_t)(0xd03fu+3u*direction);return false;
    }
    s->aux[0x4b]=room;
    if (direction==0) s->aux[0x41]=(uint8_t)(s->aux[0x41]+140u);
    else if (direction==1) s->aux[0x41]=(uint8_t)(s->aux[0x41]-140u);
    else if (direction==2) {s->aux[0x45]=(uint8_t)(s->aux[0x45]+3u);s->aux[0x42]=(uint8_t)(s->aux[0x42]+189u);}
    else {s->aux[0x45]=(uint8_t)(s->aux[0x45]-3u);s->aux[0x42]=(uint8_t)(s->aux[0x42]-189u);}
    return true;
}
typedef struct WorldTick {
    PopControllerState *state;const PopControllerIO *io;PopWorldTickWrite write;
    PopControllerQueryResult result;
} WorldTick;
static bool query(WorldTick *t,uint16_t helper,uint8_t argument) {
    t->result.a=argument;
    if (!t->io || !t->io->query || !t->io->query(t->io->context,helper,argument,t->state,&t->result)) {
        t->state->required_helper=helper;return false;
    }
    return true;
}
static bool read_value(WorldTick *t,uint16_t address,uint8_t *value) {
    if (pop_data_read(NULL,address,value) ||
        (t->io && t->io->read && t->io->read(t->io->context,address,value))) return true;
    t->state->required_helper=address;return false;
}
static bool write_value(WorldTick *t,uint16_t address,uint8_t value) {
    if (t->io && t->write && t->write(t->io->context,address,value)) return true;
    t->state->required_helper=address;return false;
}
static bool transfer_side_effects(WorldTick *t,uint8_t direction) {
    uint8_t *s=t->state->aux;uint16_t pointer;
    if (direction==0) {
        if (s[0x86] && s[0x86]!=77 && s[0x3f4]==4 && s[0x45]==0 && s[0x4b]==11) {
            t->result.x=50;if (!query(t,0xd930,3)) return false;s[0x86]=77;
        }
        if (s[0x3f4]==3 && s[0x4b]==7) {s[0x212]=1;s[0xe1]=s[0xd0];}
    } else if (direction==1) {
        if (s[0x3f4]==12 && s[0x4b]==18) {
            t->result.x=1;t->result.y=0;if (!query(t,0xd021,15)) return false;
            pointer=(uint16_t)(s[0x24]|((uint16_t)s[0x25]<<8));
            if (!write_value(t,(uint16_t)(pointer+t->result.y),1)) return false;
        }
        if (s[0x3f4]==13 && !s[0x86] && s[0x4b]==3) {
            t->result.x=25;if (!query(t,0xd930,10)) return false;
        }
    }
    return true;
}
/* Original5B08. The high/low Y guards precede pose/horizontal tests. */
static bool select_transition(WorldTick *t,uint8_t *direction) {
    uint8_t *s=t->state->aux,y=s[0x42],action=s[0x46],pose=s[0x40],d=255;
    if (action!=3 && action!=4 && action!=5 && (y<10 || y>=240)) d=2;
    else if (y>=215) {
        if (s[0x3f4]==6 && s[0x4b]==1) {*direction=255;return true;}
        d=3;
    } else if ((pose>=135 && pose<150) || (pose>=110 && pose<120) ||
               (pose>=150 && pose<163) || (pose>=166 && pose<169) || action==7) {
        *direction=255;return true;
    } else if (s[0x43]) {
        if (s[0xb7]<=54) d=0;else if (s[0xb7]>=198) d=1;
    } else {
        t->result.x=9;t->result.y=s[0x45];
        if (!query(t,0xd021,s[0x4b])) return false;
        if (t->result.a!=7 && t->result.a!=12 && s[0xb8]>=201) d=1;
        else if (s[0xb8]<=57) d=0;
    }
    if (d!=255 && (!transfer_side_effects(t,d) || !pop_world_tick_transfer_actor(t->state,d))) return false;
    *direction=d;return true;
}
static void copy_actor(uint8_t *s,unsigned destination,unsigned source) {
    unsigned i;for (i=0;i<16;++i) s[destination+i]=s[source+i];
}
static bool persist_guard(WorldTick *t) {
    uint8_t *s=t->state->aux,room=s[0x6b],row=s[0x65],cell;
    if (s[0x63]==86 || s[0x6d]==1 || s[0x6d]==24) return true;
    s[0x91]=0;s[0x92]=row;
    /* D06C/D2B2 result Y; no semantic AUX is written by that helper. */
    cell=(row&128u)?s[0x91]:(row>=3 || s[0x91]>=10?30:(uint8_t)(row*10u+s[0x91]));
    if (!write_value(t,(uint16_t)(0xbf46u+room),cell) ||
        !write_value(t,(uint16_t)(0xbf76u+room),s[0x61]) ||
        !write_value(t,(uint16_t)(0xbf5eu+room),s[0x63]) ||
        !write_value(t,(uint16_t)(0xbfa6u+room),s[0xdd])) return false;
    if (s[0x6f]&128u) {
        if (!write_value(t,(uint16_t)(0xbfbeu+room),0)) return false;
    } else if (!write_value(t,(uint16_t)(0xbf8eu+room),s[0x69]) ||
               !write_value(t,(uint16_t)(0xbfbeu+room),s[0x6a])) return false;
    s[0x63]=86;s[0xcc]=0;return true;
}
static bool rooms(WorldTick *t) {
    uint8_t *s=t->state->aux,d,record;bool follows=false;
    if (s[0x28]) {--s[0x28];return true;}
    copy_actor(s,0x40,0x50);
    if (!pop_geometry_project_actor(s)) {t->state->required_helper=0xd066;return false;}
    if (!pop_geometry_bounds(s)) {t->state->required_helper=0xd033;return false;}
    if (!select_transition(t,&d)) return false;
    if (d==255) return true;
    s[0xf2]=d;copy_actor(s,0x50,0x40);s[0xa6]=s[0x4b];
    if (s[0x63]==86) return true;
    if ((s[0x6f]&128u) && s[0x6e]==2) {
        if (!read_value(t,(uint16_t)(0xbf46u+s[0x5b]),&record)) return false;
        if (record>=30) follows=true;
        else {
            if (!read_value(t,(uint16_t)(0xbfbeu+s[0x5b]),&record)) return false;
            follows=record!=0;
        }
        if (follows) {
            if (d==0) follows=s[0x61]<91;
            else if (d==1) follows=s[0x61]>=165;
            else if (d==2) follows=(s[0x65]&128u)!=0;
            else follows=s[0x65]>=3;
        }
    }
    if (!follows) return persist_guard(t);
    if (!write_value(t,(uint16_t)(0xbf46u+s[0x5b]),255) ||
        !write_value(t,(uint16_t)(0xbf46u+s[0x6b]),255)) return false;
    copy_actor(s,0x40,0x60);
    if (!pop_world_tick_transfer_actor(t->state,d)) return false;
    copy_actor(s,0x60,0x40);return true;
}
PopControllerResult pop_world_tick_rooms(PopControllerState *s,const PopControllerIO *io,PopWorldTickWrite write) {
    PopControllerState work;WorldTick t;
    if (!s) return POP_CONTROLLER_INVALID;
    work=*s;work.required_helper=0;work.selected_sequence=0;
    t=(WorldTick){&work,io,write,{0,0,0,false,false,false}};
    if (!rooms(&t)) {s->required_helper=work.required_helper;return POP_CONTROLLER_HELPER_REQUIRED;}
    *s=work;return POP_CONTROLLER_COMPLETE;
}
PopControllerResult pop_world_tick_activate_room(PopControllerState *s,const PopControllerIO *io) {
    PopControllerState work;WorldTick t;PopControllerResult result;
    if (!s) return POP_CONTROLLER_INVALID;
    work=*s;work.required_helper=0;work.selected_sequence=0;
    if (!work.aux[0xa6] || work.aux[0xa6]==work.aux[0xcb]) {*s=work;return POP_CONTROLLER_COMPLETE;}
    work.aux[0xcb]=work.aux[0xa6];
    if (work.aux[0xcb]==5 && work.aux[0x3f4]==14) {
        s->required_helper=0x250e;return POP_CONTROLLER_HELPER_REQUIRED;
    }
    work.aux[0xa3]=1;
    if (!pop_world_tick_room_links(&work)) {s->required_helper=work.required_helper;return POP_CONTROLLER_HELPER_REQUIRED;}
    /* D05A/D835 copies only the player; the opponent cache39C is untouched. */
    copy_actor(work.aux,0x40,0x50);
    t=(WorldTick){&work,io,NULL,{0,0,0,false,false,false}};
    if (!query(&t,0xe01e,0) || !query(&t,0xe000,0)) {
        s->required_helper=work.required_helper;return POP_CONTROLLER_HELPER_REQUIRED;
    }
    /* E048/E04E has no writes or callees unless the level equals13. */
    if (work.aux[0x3f4]==13 && !query(&t,0xe048,0)) {
        s->required_helper=work.required_helper;return POP_CONTROLLER_HELPER_REQUIRED;
    }
    result=pop_guard_load_room(&work,io);
    if (result!=POP_CONTROLLER_COMPLETE) {s->required_helper=work.required_helper;return result;}
    *s=work;return POP_CONTROLLER_COMPLETE;
}
PopControllerResult pop_world_tick_guard_fall(PopControllerState *s,const PopControllerIO *io,PopWorldTickWrite write) {
    PopControllerState work;WorldTick t;PopControllerResult result;uint8_t *a,room;
    if (!s) return POP_CONTROLLER_INVALID;
    work=*s;work.required_helper=0;work.selected_sequence=0;a=work.aux;
    t=(WorldTick){&work,io,write,{0,0,0,false,false,false}};
    if (a[0x63]==86 || a[0x62]<215) {*s=work;return POP_CONTROLLER_COMPLETE;}
    if (a[0x6d]==1) {
        if (a[0x66]==4) {copy_actor(a,0x40,0x60);pop_guard_dismiss(&work);copy_actor(a,0x60,0x40);}
        *s=work;return POP_CONTROLLER_COMPLETE;
    }
    if (a[0x6d]==4) {
        if (!neighbor(a[0x6b],POP_WORLD_BELOW,&room)) {s->required_helper=0xd048;return POP_CONTROLLER_HELPER_REQUIRED;}
        a[0x6b]=room;
        if (room==3) {
            if (!query(&t,0xd909,5)) goto missing;
            a[0x61]=133;a[0x65]=1;a[0x63]=0;a[0x6f]=255;
            if (!persist_guard(&t)) goto missing;
            *s=work;return POP_CONTROLLER_COMPLETE;
        }
    }
    result=pop_guard_notify_death(&work,io);
    if (result!=POP_CONTROLLER_COMPLETE) {s->required_helper=work.required_helper;return result;}
    if (!write_value(&t,(uint16_t)(0xbf46u+a[0xcb]),255)) goto missing;
    a[0x63]=86;a[0xcc]=0;a[0xd2]=255;
    *s=work;return POP_CONTROLLER_COMPLETE;
missing:
    s->required_helper=work.required_helper;return POP_CONTROLLER_HELPER_REQUIRED;
}
