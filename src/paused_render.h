#ifndef POP_PAUSED_RENDER_H
#define POP_PAUSED_RENDER_H
#include "game_render.h"
#include "loop_state.h"

/* One actual D924 draw after the caller advances its separate modal phase.
 * The destination is the existing displayed MAIN page. No game or modal
 * state is changed. This is separate from a fresh whole-room composition. */
bool pop_paused_torch_apply(const PopLoopPausedDraw *draw,
    uint8_t *page,size_t page_bytes);

/* Fresh pure view with D921's private current-room torch phases. Modal begin
 * temporarily flips AUX00; the adapter corrects that only on a local copy.
 * This is a reconstruction, not the exact retained modal page: clipped torch
 * bottoms and foreground retain history in the original. Use the incremental
 * operation above on an existing page for exact paused presentation. */
bool pop_game_render_paused_hgr(const PopGame *game,const PopLoopModal *modal,
    uint8_t *page,size_t page_bytes);
#endif
