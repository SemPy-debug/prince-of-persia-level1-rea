#ifndef POP_GAME_RENDER_H
#define POP_GAME_RENDER_H

#include "game.h"
#include "room_scene.h"

/* Native full-page view: source room commands, live player/kind2 guard body
 * projection/clipping, D027 attachments and native masks/foreground. CB is
 * the original displayed room selector. This integration admits the source
 * source-local geometry replacements, live torches/gates/loose phases,
 * falling slabs, blood and ordinary source kind2 guards. Special guard colors,
 * mode3 flash and further trap/exit visual states remain unsupported. Unsupported
 * states return false and preserve the destination. Dynamic visual programs
 * must be added before using this API for those later gameplay states.
 * Input game is read-only; no emulator, target code or frame lookup runs. */
bool pop_game_render_hgr(const PopGame *game, uint8_t *page, size_t page_bytes);
/* Explicit native actor/attachment/blood/falling command program. Output is
 * source-depth ordered; room-owned potion bubbles remain in the room layer. */
bool pop_game_render_actor_commands(const PopGame *game, PopSceneDraw *draws,
                                     size_t capacity, size_t *count);
/* Whole view after the semantic render commit: includes explicit health and
 * supported source message overlays. The room-layer API above remains useful
 * for comparisons at original0676, before the final07D9 HUD queue. */
bool pop_game_render_view_hgr(const PopGame *game, uint8_t *page, size_t page_bytes);

#endif
