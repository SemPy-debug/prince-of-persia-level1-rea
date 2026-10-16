#include "guard_tick.h"
#include "guard.h"
#include <stddef.h>
#include <string.h>

static bool query(PopControllerState *s,const PopControllerIO *io,
    PopControllerQueryResult *q,uint16_t helper) {
    if (!io || !io->query || !io->query(io->context,helper,q->a,s,q)) {
        s->required_helper=helper;return false;
    }
    return true;
}
PopControllerResult pop_guard_tick(PopControllerState *s,
    const PopControllerIO *io,PopMotionWrite write) {
    PopControllerState work;PopControllerQueryResult q={0};uint16_t failed;
    if (!s) return POP_CONTROLLER_INVALID;
    work=*s;work.required_helper=0;work.selected_sequence=0;
    if (work.aux[0x63]==86) { *s=work;return POP_CONTROLLER_COMPLETE; }
    /* D0B1/D87B: guard becomes the working actor, player its opponent. */
    memcpy(work.aux+0x40,work.aux+0x60,16);
    memcpy(work.aux+0x39c,work.aux+0x50,16);
    failed=0x3a09;if (!query(&work,io,&q,failed)) goto unmet;
    failed=0xf918;if (!query(&work,io,&q,failed)) goto unmet;
    failed=0x3a06;
    if (pop_guard_decide(&work,io)!=POP_CONTROLLER_COMPLETE) goto unmet;
    if (work.aux[0x4b]==work.aux[0xcb]) {
        failed=0x450c;if (!query(&work,io,&q,failed)) goto unmet;
        if (work.aux[0x41]>=44 && work.aux[0x41]<211) {
            pop_motion_update_gravity(&work);
            failed=0xe033;
            if (pop_motion_integrate_velocity(&work,io)!=POP_CONTROLLER_COMPLETE) goto unmet;
            failed=0xd066;if (!query(&work,io,&q,failed)) goto unmet;
            failed=0x3a09;if (!query(&work,io,&q,failed)) goto unmet;
            failed=0xd033;if (!query(&work,io,&q,failed)) goto unmet;
            failed=0x451e;
            if (pop_motion_correct_guard_wall(&work,io)!=POP_CONTROLLER_COMPLETE) goto unmet;
            failed=0x3a03;
            if (pop_motion_resolve_floor(&work,io)!=POP_CONTROLLER_COMPLETE) goto unmet;
            failed=0x3a0c;
            if (pop_motion_check_tile_effects(&work,io)!=POP_CONTROLLER_COMPLETE) goto unmet;
            failed=0xd096;if (!query(&work,io,&q,failed)) goto unmet;
            failed=0x3a15;
            if (pop_motion_check_spikes(&work,io)!=POP_CONTROLLER_COMPLETE) goto unmet;
            failed=0x4512;
            if (pop_motion_check_guard_chompers(&work,io,write)!=POP_CONTROLLER_COMPLETE) goto unmet;
        }
    }
    /* D063/D853 commits the complete guard working record. */
    memcpy(work.aux+0x60,work.aux+0x40,16);
    *s=work;return POP_CONTROLLER_COMPLETE;
unmet:
    s->required_helper=work.required_helper?work.required_helper:failed;
    return POP_CONTROLLER_HELPER_REQUIRED;
}
