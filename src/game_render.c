#include "game_render.h"
#include "player_render.h"
#include "room_live.h"
#include "falling_draw.h"
#include "object_draw.h"
#include "hud.h"
#include "blood_draw.h"
#include <string.h>

static bool append_actor(const PopGame *game, bool guard,
                          PopDrawableObject objects[20], size_t *count)
{
    if (!guard && pop_player_render_delay_cycles(&game->state)!=0) {
        /* Actual first-level exit pose0 takes23D5's E021 delay without a
         * body projection. Its observed wrapper has no blood or weapon. */
        return game->state.aux[0xcf]==0 && game->state.aux[0x5e]==0;
    }
    PopControllerState projected;
    uint16_t helper;
    const bool ready = guard ?
        pop_guard_render_project(&game->runtime.world, &game->state, &projected, &helper) :
        pop_player_render_project(&game->runtime.world, &game->state, &projected, &helper);
    if (!ready) return false;
    PopSceneDraw draw;
    PopActorDrawStatus status = pop_actor_draw_projected(projected.aux,
        sizeof(projected.aux), guard ? 2 : 0, &draw);
    if (status != POP_ACTOR_DRAW_READY && status != POP_ACTOR_DRAW_SKIPPED) return false;
    PopDrawableObject body = {guard ? 2 : 0, projected.aux[0x75], projected.aux[0x73],
        status == POP_ACTOR_DRAW_READY ? 1 : 0, {{0}}};
    if (status == POP_ACTOR_DRAW_READY) body.draws[0] = draw;
    if (*count < 19) objects[(*count)++] = body;
    if ((game->state.aux[guard ? 0xd2 : 0xcf] & 128u) != 0) {
        status = pop_blood_draw_projected(&projected,&draw,&helper);
        if (status == POP_ACTOR_DRAW_READY) {
            PopDrawableObject blood = {5,255,draw.params.bottom_y,1,{{0}}};
            blood.draws[0]=draw;
            if (*count < 19) objects[(*count)++] = blood;
        } else if (status != POP_ACTOR_DRAW_SKIPPED) return false;
    }
    status = pop_actor_attachment_draw(&projected, &draw, &helper);
    if (status == POP_ACTOR_DRAW_READY) {
        PopDrawableObject attachment = {3, projected.aux[0x75], draw.params.bottom_y, 1, {{0}}};
        attachment.draws[0] = draw;
        if (*count < 19) objects[(*count)++] = attachment;
    }
    else if (status != POP_ACTOR_DRAW_SKIPPED) return false;
    return true;
}

bool pop_game_render_actor_commands(const PopGame *game, PopSceneDraw *draws,
                                     size_t capacity, size_t *count)
{
    PopDrawableObject objects[20];
    size_t object_count = 0;
    if (game == NULL || !game->initialized || draws == NULL || count == NULL) return false;
    const PopRoomScene *room = pop_room_scene_initial(game->state.aux[0xcb]);
    if (room == NULL) return false;
    const unsigned falling_count = game->state.aux[0x30d];
    uint8_t below;
    if (falling_count > 15 ||
        !pop_world_initial_neighbor(room->room_id, POP_WORLD_BELOW, &below)) return false;
    /* Original EE12 runs before2397 and visits the source secondary slots in
     * descending order. Renderer snapshots remain read-only. */
    for (unsigned n = falling_count; n > 0; --n) {
        uint8_t record[6];
        for (unsigned field = 0; field < 6; ++field)
            record[field] = game->runtime.mechanisms.falling[field][n];
        PopFallingObject falling;
        if (!pop_falling_draw(record, (uint8_t)room->room_id, below, &falling)) return false;
        if (!falling.visible) continue;
        PopDrawableObject object = {128, falling.depth, falling.bottom_y, 4, {{0}}};
        for (unsigned i = 0; i < 4; ++i) object.draws[i] = falling.draws[i];
        objects[object_count++] = object;
    }
    /* Original23A7 guard then23C0 player source order. Each body's actual
     * D027 attachment follows its body; blood overlays remain explicit. */
    if (game->state.aux[0x6b] == game->state.aux[0xcb] && game->state.aux[0x63] != 0x56) {
        if (!append_actor(game, true, objects, &object_count)) return false;
    }
    if (game->state.aux[0x5b] != 0 && game->state.aux[0x5b] == game->state.aux[0xcb]) {
        if (!append_actor(game, false, objects, &object_count)) return false;
    }
    return pop_object_draw_order(objects, object_count, draws, capacity, count);
}

bool pop_game_render_hgr(const PopGame *game, uint8_t *page, size_t page_bytes)
{
    PopSceneDraw actors[64];size_t actor_count=0;
    PopRoomLiveScene visual;
    if (game == NULL || !game->initialized || page == NULL ||
        page_bytes != POP_HGR_PAGE_BYTES) return false;
    const PopRoomScene *room = pop_room_scene_initial(game->state.aux[0xcb]);
    if (room == NULL || !pop_room_live_build(&game->runtime.world,
        game->runtime.effect_memory.link_map,&game->state,room,&visual) ||
        !pop_game_render_actor_commands(game,actors,64,&actor_count)) return false;
    return pop_room_compose(page, page_bytes, &visual.room, NULL, 0, actors, actor_count);
}

bool pop_game_render_view_hgr(const PopGame *game, uint8_t *page, size_t page_bytes)
{
    uint8_t output[POP_HGR_PAGE_BYTES];
    if (page == NULL || page_bytes != POP_HGR_PAGE_BYTES ||
        !pop_game_render_hgr(game, output, sizeof(output)) ||
        !pop_hud_apply(&game->state, output, sizeof(output))) return false;
    memcpy(page, output, sizeof(output));
    return true;
}
