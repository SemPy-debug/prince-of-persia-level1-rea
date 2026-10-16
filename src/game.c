#include "game.h"
#include "geometry.h"
#include "motion.h"
#include "player_tick.h"
#include <stddef.h>
#include <string.h>

extern const uint8_t pop_start_zp[256];
extern const uint8_t pop_start_globals[512];
extern const uint8_t pop_start_effects[96];
extern const uint8_t pop_start_secondary[96];
extern const uint8_t pop_start_sounds[33];
extern const uint8_t pop_start_world[1440];
extern const uint8_t pop_start_guards[144];
extern const uint8_t pop_start_links[256];
extern const uint8_t pop_start_dirty[250];

bool pop_game_reset(PopGame *game) {
    if (game == NULL) return false;
    memset(game, 0, sizeof(*game));
    if (!pop_runtime_reset(&game->runtime)) return false;
    memcpy(game->state.aux, pop_start_zp, sizeof(pop_start_zp));
    memcpy(game->state.aux + 0x200, pop_start_globals, sizeof(pop_start_globals));
    memcpy(&game->runtime.effects, pop_start_effects, sizeof(pop_start_effects));
    memcpy(game->runtime.mechanisms.falling, pop_start_secondary, sizeof(pop_start_secondary));
    game->runtime.sound_count = pop_start_sounds[0];
    memcpy(game->runtime.sounds, pop_start_sounds + 1, sizeof(game->runtime.sounds));
    memcpy(&game->runtime.world, pop_start_world, sizeof(pop_start_world));
    memcpy(&game->runtime.guards, pop_start_guards, sizeof(pop_start_guards));
    memcpy(game->runtime.effect_memory.link_map, pop_start_links, sizeof(pop_start_links));
    for (unsigned i = 0; i < sizeof(pop_start_dirty); ++i)
        if (!pop_runtime_write(&game->runtime, (uint16_t)(0x5e3cu + i), pop_start_dirty[i])) return false;
    game->initialized = true;
    return true;
}
static bool helper(PopGame *game, uint16_t id) {
    PopControllerQueryResult result = {0};
    return pop_runtime_query(&game->runtime, id, 0, &game->state, &result);
}

static bool world_write(void *context, uint16_t address, uint8_t value) {
    return pop_runtime_write(context, address, value);
}

PopGameResult pop_game_step(PopGame *game, PopGameInput input) {
    PopGame work;
    PopControllerIO io;
    PopControllerResult sampled;
    uint16_t failed = 0;
    if (game == NULL || !game->initialized) return POP_GAME_INVALID;
    work = *game;
    work.required_helper = 0;
    work.state.required_helper = 0;
    work.runtime.missing_helper = 0;
    io.query = pop_runtime_query;
    io.read = pop_runtime_read;
    io.context = &work.runtime;

    /* The order here is the actual source22F4 routine, not a generic physics
     * loop. Whole-frame rollback also protects writes from earlier effects. */
    failed = 0xd0a8;
    if (pop_player_prepare_tick(&work.state, &io) != POP_CONTROLLER_COMPLETE) goto unmet;
    failed = 0x2519;
    sampled=pop_player_sample_tick_input(&work.state,input.horizontal,input.vertical,input.action,&io);
    if (sampled==POP_CONTROLLER_RESTART_LEVEL || sampled==POP_CONTROLLER_GAME_OVER ||
        sampled==POP_CONTROLLER_TIME_EXPIRED) {
        PopGameResult flow=sampled==POP_CONTROLLER_RESTART_LEVEL?POP_GAME_RESTART_LEVEL:
            (sampled==POP_CONTROLLER_GAME_OVER?POP_GAME_GAME_OVER:POP_GAME_TIME_EXPIRED);
        *game=work;return flow;
    }
    if (sampled!=POP_CONTROLLER_COMPLETE) goto unmet;
    if (work.state.aux[0x9f] != 0 && (work.state.aux[0x4f] & 0x80u) == 0) {
        failed = 0x04c3; /* Source nonlocal redraw/flip transfer, still explicit. */
        goto unmet;
    }
    pop_player_tick_modifier(&work.state);
    if (work.state.aux[0x4b] != 0) {
        failed = 0x450c;
        if (!helper(&work, failed)) goto unmet;
        pop_motion_update_gravity(&work.state);
        failed = 0xe033;
        if (pop_motion_integrate_velocity(&work.state, &io) != POP_CONTROLLER_COMPLETE) goto unmet;
        failed = 0xd066;
        if (!pop_geometry_project_actor(work.state.aux)) goto unmet;
        failed = 0x3a09;
        if (!helper(&work, failed)) goto unmet;
        failed = 0xd033;
        if (!pop_geometry_bounds(work.state.aux)) goto unmet;
        failed = 0xf909;
        if (pop_motion_check_opponent_contact(&work.state, &io) != POP_CONTROLLER_COMPLETE) goto unmet;
        failed = 0x4500;
        if (pop_motion_build_collision(&work.state, &io) != POP_CONTROLLER_COMPLETE) goto unmet;
        failed = 0x4503;
        if (pop_motion_correct_wall(&work.state, &io) != POP_CONTROLLER_COMPLETE) goto unmet;
        failed = 0x4518;
        if (pop_motion_correct_gate(&work.state, &io) != POP_CONTROLLER_COMPLETE) goto unmet;
        failed = 0x3a03;
        if (pop_motion_resolve_floor(&work.state, &io) != POP_CONTROLLER_COMPLETE) goto unmet;
        failed = 0x3a0c;
        if (pop_motion_check_tile_effects(&work.state, &io) != POP_CONTROLLER_COMPLETE) goto unmet;
        failed = 0xd096;
        if (!helper(&work, failed)) goto unmet;
        failed = 0x3a15;
        if (pop_motion_check_spikes(&work.state, &io) != POP_CONTROLLER_COMPLETE) goto unmet;
        failed = 0x450f;
        if (pop_motion_check_chompers(&work.state, &io, world_write) != POP_CONTROLLER_COMPLETE) goto unmet;
        failed = 0x25c6;
        if (!helper(&work, failed)) goto unmet;
    }
    /* Original D060/D848 commits the complete working16-byte player record. */
    memcpy(work.state.aux + 0x50, work.state.aux + 0x40, 16);
    ++work.ticks;
    *game = work;
    return POP_GAME_UPDATED;

unmet:
    if (work.runtime.missing_helper != 0) failed = work.runtime.missing_helper;
    else if (work.state.required_helper != 0) failed = work.state.required_helper;
    game->required_helper = failed;
    game->runtime.missing_helper = failed;
    return POP_GAME_HELPER_REQUIRED;
}
