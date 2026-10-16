#include "game.h"
#include "guard_tick.h"
#include "guard_combat.h"
#include "world_tick.h"
#include "frame_events.h"
#include <stddef.h>

static bool helper(PopGame *game,PopControllerQueryResult *q,uint16_t id) {
    return pop_runtime_query(&game->runtime,id,q->a,&game->state,q);
}
static bool event(PopGame *game,PopControllerQueryResult *q,uint16_t id,
    const PopControllerIO *io) {
    return pop_frame_event_query(id,q->a,&game->state,q,io);
}
PopGameResult pop_game_frame(PopGame *game,PopGameInput input) {
    PopGame work;PopControllerIO io;PopControllerQueryResult q={0};uint16_t failed;
    PopGameResult player;
    if (!game || !game->initialized) return POP_GAME_INVALID;
    work=*game;work.required_helper=0;work.state.required_helper=0;
    work.runtime.missing_helper=0;
    io=(PopControllerIO){pop_runtime_query,pop_runtime_read,&work.runtime};
    failed=0x224e;
    if (work.state.aux[0x3f4]!=1) goto unmet; /* This product contains level1. */
    failed=0xee0f;if (!helper(&work,&q,failed)) goto unmet;
    failed=0xee00;if (!helper(&work,&q,failed)) goto unmet;
    failed=0xf924;if (!event(&work,&q,failed,&io)) goto unmet;
    failed=0xf930;if (!event(&work,&q,failed,&io)) goto unmet;
    failed=0x22f4;player=pop_game_step(&work,input);
    if (player==POP_GAME_RESTART_LEVEL||player==POP_GAME_GAME_OVER||player==POP_GAME_TIME_EXPIRED){
        *game=work;return player;
    }
    if (player!=POP_GAME_UPDATED) goto unmet;
    failed=0x234e;
    if (pop_guard_tick(&work.state,&io,pop_runtime_write)!=POP_CONTROLLER_COMPLETE) goto unmet;
    failed=0x5403;
    if (pop_guard_check_sword_contact(&work.state,&io)!=POP_CONTROLLER_COMPLETE) goto unmet;
    failed=0x5406;
    if (pop_guard_resolve_sword_hits(&work.state,&io)!=POP_CONTROLLER_COMPLETE) goto unmet;
    failed=0x26c6;if (!helper(&work,&q,failed)) goto unmet;
    failed=0x25f0;if (!helper(&work,&q,failed)) goto unmet;
    failed=0x540c;
    if (pop_world_tick_rooms(&work.state,&io,pop_runtime_write)!=POP_CONTROLLER_COMPLETE) goto unmet;
    failed=0x24d3;
    if (pop_world_tick_activate_room(&work.state,&io)!=POP_CONTROLLER_COMPLETE) goto unmet;
    failed=0x540f;
    if (pop_world_tick_guard_fall(&work.state,&io,pop_runtime_write)!=POP_CONTROLLER_COMPLETE) goto unmet;
    /* Level0/6/12 special transfers in2275..22B4 are outside this first level. */
    failed=0xd92a;if (!event(&work,&q,failed,&io)) goto unmet;
    failed=0xe00c;if (!event(&work,&q,failed,&io)) goto unmet;
    if ((work.state.aux[0x300]|work.state.aux[0x302])==0) {
        *game=work;return POP_GAME_TIME_EXPIRED; /* Actual nonreturning2503 boundary. */
    }
    *game=work;return POP_GAME_UPDATED;
unmet:
    if (work.runtime.missing_helper) failed=work.runtime.missing_helper;
    else if (work.state.required_helper) failed=work.state.required_helper;
    else if (work.required_helper) failed=work.required_helper;
    game->required_helper=failed;game->runtime.missing_helper=failed;
    return POP_GAME_HELPER_REQUIRED;
}
