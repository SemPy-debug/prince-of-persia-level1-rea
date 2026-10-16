#include "frame_events.h"
#include "../generated/frame_event_clock_data.h"
#include <stddef.h>
typedef struct FrameEvent {
    PopControllerState *state;PopControllerQueryResult *result;const PopControllerIO *io;
} FrameEvent;
static void nz(PopControllerQueryResult *r,uint8_t value) {r->zero=value==0;r->negative=(value&128u)!=0;}
static void value_a(PopControllerQueryResult *r,uint8_t value) {r->a=value;nz(r,value);}
static void value_x(PopControllerQueryResult *r,uint8_t value) {r->x=value;nz(r,value);}
static void value_y(PopControllerQueryResult *r,uint8_t value) {r->y=value;nz(r,value);}
static void compare(PopControllerQueryResult *r,uint8_t a,uint8_t b) {r->carry=a>=b;nz(r,(uint8_t)(a-b));}
static void add(PopControllerQueryResult *r,uint8_t value,bool carry) {
    unsigned sum=(unsigned)r->a+value+(carry?1u:0u);r->carry=sum>255u;value_a(r,(uint8_t)sum);
}
static bool query(FrameEvent *f,uint16_t helper) {
    if (f->io && f->io->query && f->io->query(f->io->context,helper,f->result->a,f->state,f->result)) return true;
    f->state->required_helper=helper;return false;
}
static bool spec(FrameEvent *f,uint8_t *value) {
    uint8_t *s=f->state->aux;
    uint16_t address=(uint16_t)(s[0x26]+256u*s[0x27]+f->result->y);
    if (f->io && f->io->read && f->io->read(f->io->context,address,value)) return true;
    f->state->required_helper=address;return false;
}
/* DCEB's self-modified add immediates become explicit native constants. Its
 * Y result is packedBCD; decimal-decrement61..00 is finite, then99 exits. */
static void clock_remainder(FrameEvent *f,uint16_t increment) {
    uint8_t *s=f->state->aux;PopControllerQueryResult *r=f->result;unsigned step;
    value_y(r,0x61);
    for (step=0;step<62;++step) {
        value_a(r,s[0xea]);compare(r,r->a,s[0x307]);
        if (r->carry) {
            if (!r->zero) return;
            value_a(r,s[0xe9]);compare(r,r->a,s[0x306]);
            if (r->carry) return;
        }
        value_a(r,s[0xe9]);add(r,(uint8_t)increment,false);s[0xe9]=r->a;
        value_a(r,s[0xea]);add(r,(uint8_t)(increment>>8),r->carry);s[0xea]=r->a;
        if (r->y==0) {r->carry=false;value_a(r,0x99);value_y(r,0);return;}
        r->carry=true;
        value_a(r,(uint8_t)((r->y&15u)?r->y-1u:r->y-7u));value_y(r,r->a);
    }
}
static void update_clock(FrameEvent *f) {
    uint8_t *s=f->state->aux;PopControllerQueryResult *r=f->result;
    value_a(r,0);s[0xe9]=0;s[0xea]=0;
    clock_remainder(f,0x02d5);s[0x300]=r->y;compare(r,r->y,2);
    if (r->carry) return;
    value_a(r,0x17);s[0xe9]=r->a;value_a(r,0xa7);s[0xea]=r->a;
    value_a(r,0);clock_remainder(f,12);s[0x302]=r->y;
}
static bool progress_counter(FrameEvent *f) {
    uint8_t *s=f->state->aux;PopControllerQueryResult *r=f->result;uint8_t index;
    value_a(r,s[0x3f4]);if (r->zero) return true;
    value_a(r,s[0x5f]);if (!r->negative) return true;
    ++s[0x306];nz(r,s[0x306]);
    if (r->zero) {++s[0x307];nz(r,s[0x307]);}
    if (r->zero) {value_a(r,255);s[0x306]=255;s[0x307]=255;}
    value_y(r,s[0x301]);compare(r,r->y,36);if (r->carry) return true;
    index=r->y;
    /* Natural source indices start0 and advance2. An odd35 would read the
     * adjacent RTS as data, outside this typed36-byte table. */
    if (index==35) {f->state->required_helper=0xdd3a;return false;}
    value_a(r,s[0x307]);compare(r,r->a,pop_frame_event_clock_thresholds[index+1]);if (!r->carry) return true;
    value_a(r,s[0x306]);compare(r,r->a,pop_frame_event_clock_thresholds[index]);if (!r->carry) return true;
    value_a(r,s[0xdb]);if (!r->zero) return true;
    ++s[0x301];nz(r,s[0x301]);++s[0x301];nz(r,s[0x301]);
    value_a(r,2);s[0x31f]=2;return true;
}
static void notify(FrameEvent *f) {
    uint8_t *s=f->state->aux;PopControllerQueryResult *r=f->result;
    value_a(r,s[0x31f]);if (r->zero) return;
    value_a(r,s[0x5f]);if (!r->negative) return;
    update_clock(f);
    value_a(r,s[0x300]);compare(r,r->a,2);
    if (!r->carry) {
        value_a(r,s[0x302]);
        if (!r->zero) {
            value_a(r,s[0x3f4]);compare(r,r->a,14);
            if (!r->carry) {
                value_a(r,s[0x302]);compare(r,r->a,2);
                if (!r->carry) {value_a(r,0);s[0x31f]=0;s[0xdb]=0;return;}
                value_a(r,s[0x30b]);compare(r,r->a,3);
                if (!r->zero) {
                    value_a(r,s[0xdb]);if (!r->zero) return;
                    value_a(r,3);s[0x30b]=3;
                }
                value_a(r,1);s[0x31f]=1;s[0xdb]=1;return;
            }
        } else {value_a(r,s[0x31f]);compare(r,r->a,3);if (!r->carry) return;}
    }
    value_a(r,s[0xdb]);if (!r->zero) return;
    value_a(r,3);s[0x30b]=3;value_a(r,20);value_x(r,s[0x31f]);compare(r,r->x,4);
    if (r->carry) value_a(r,25);
    s[0xdb]=r->a;value_a(r,0);s[0x31f]=0;
}
static bool tile_at_x(FrameEvent *f) {
    PopControllerQueryResult *r=f->result;uint8_t *s=f->state->aux;
    if (!query(f,0xd00c)) return false;
    value_x(r,r->a);value_y(r,s[0x55]);value_a(r,s[0x5b]);return query(f,0xd021);
}
static void no_visibility(FrameEvent *f) {value_a(f->result,0);f->state->aux[0xd1]=0;}
static bool visibility(FrameEvent *f) {
    uint8_t *s=f->state->aux,tile,parameter;PopControllerQueryResult *r=f->result;unsigned count;
    value_a(r,s[0x6d]);compare(r,r->a,24);if (r->zero) return true;
    compare(r,r->a,1);
    if (r->zero) {value_a(r,s[0x3f4]);compare(r,r->a,12);if (!r->zero) {no_visibility(f);return true;}}
    value_a(r,s[0x50]);if (r->zero) {no_visibility(f);return true;}
    compare(r,r->a,219);
    if (r->carry) {compare(r,r->a,229);if (!r->carry) {no_visibility(f);return true;}}
    value_a(r,s[0x63]);compare(r,r->a,86);if (r->zero) {no_visibility(f);return true;}
    value_a(r,(uint8_t)(s[0x5f]&s[0x6f]));if (!r->negative) {no_visibility(f);return true;}
    value_a(r,s[0x5b]);compare(r,r->a,s[0x6b]);if (!r->zero) {no_visibility(f);return true;}
    value_a(r,s[0x55]);compare(r,r->a,s[0x65]);if (!r->zero) {no_visibility(f);return true;}
    value_a(r,2);s[0xd1]=2;
    value_a(r,s[0x54]);if (!query(f,0xd012)) return false;add(r,7,false);s[0xf0]=r->a;
    value_a(r,s[0x64]);if (!query(f,0xd012)) return false;add(r,7,false);s[0xf1]=r->a;
    value_a(r,s[0xf1]);compare(r,r->a,s[0xf0]);
    if (!r->carry) {value_x(r,r->a);value_a(r,s[0xf0]);s[0xf1]=r->a;s[0xf0]=r->x;}
    value_a(r,s[0xf0]);if (!tile_at_x(f)) return false;compare(r,r->a,18);
    if (r->zero) {value_a(r,14);add(r,s[0xf0],false);s[0xf0]=r->a;}
    value_a(r,s[0xf1]);if (!tile_at_x(f)) return false;compare(r,r->a,4);
    if (r->zero) {value_a(r,s[0xf1]);r->carry=r->a>=14;value_a(r,(uint8_t)(r->a-14u));s[0xf1]=r->a;}
    value_a(r,s[0xf1]);compare(r,r->a,s[0xf0]);if (!r->carry) return true;
    value_a(r,s[0xf0]);
    for (count=0;count<256;++count) {
        compare(r,r->a,s[0xf1]);if (!r->zero && r->carry) return true;
        if (!tile_at_x(f)) return false;
        tile=r->a;compare(r,tile,20);if (r->zero) {no_visibility(f);return true;}
        compare(r,tile,7);if (r->zero) {no_visibility(f);return true;}
        compare(r,tile,12);if (r->zero) {no_visibility(f);return true;}
        compare(r,tile,11);
        if (r->zero) {value_a(r,1);s[0xd1]=1;}
        else {
            compare(r,tile,4);
            if (r->zero) {
                if (!spec(f,&parameter)) return false;
                value_a(r,parameter);compare(r,r->a,112);
                if (!r->carry) {value_a(r,1);s[0xd1]=1;}
            } else {
                compare(r,tile,18);
                if (r->zero) {value_a(r,1);s[0xd1]=1;}
                else {if (!query(f,0xd04b)) return false;if (r->zero) {value_a(r,1);s[0xd1]=1;}}
            }
        }
        value_a(r,s[0xf0]);add(r,14,false);s[0xf0]=r->a;if (r->zero) return true;
    }
    f->state->required_helper=0xfd23;return false;
}
static bool skeleton_guard(FrameEvent *f) {
    PopControllerQueryResult *r=f->result;uint8_t *s=f->state->aux;
    value_a(r,s[0x3f4]);compare(r,r->a,3);if (!r->zero) return true;
    value_a(r,s[0x63]);compare(r,r->a,86);if (!r->zero) return true;
    value_a(r,s[0xcb]);compare(r,r->a,1);if (!r->zero) return true;
    value_a(r,s[0x86]);if (r->zero) return true;
    value_a(r,s[0x54]);compare(r,r->a,2);
    if (!r->zero) {compare(r,r->a,3);if (!r->zero) return true;}
    f->state->required_helper=0xfbd4;return false;
}
bool pop_frame_event_query(uint16_t helper,uint8_t argument,PopControllerState *s,
    PopControllerQueryResult *r,const PopControllerIO *io) {
    PopControllerState work;PopControllerQueryResult output;FrameEvent event;bool ok;
    (void)argument;
    if (!s || !r) return false;
    work=*s;output=*r;event=(FrameEvent){&work,&output,io};
    switch(helper) {
    case 0xf924:ok=skeleton_guard(&event);break;
    case 0xf930:ok=visibility(&event);break;
    case 0xd92a:ok=progress_counter(&event);break;
    case 0xd927:update_clock(&event);ok=true;break;
    case 0xe00c:notify(&event);ok=true;break;
    default:return false;
    }
    if (!ok) {s->required_helper=work.required_helper;return false;}
    *s=work;*r=output;return true;
}
