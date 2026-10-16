#ifndef POP_RENDER_COMMIT_H
#define POP_RENDER_COMMIT_H
#include "runtime.h"

/* Source22DD or2439 presentation semantics, independent of host video IO.
 * Commits persistent page/message/redraw state and native sound requests.
 * Original blitter scratch, stack and graphics command buffers are not part
 * of this API. Unknown helpers fail transactionally with an explicit ID. */
bool pop_render_commit(PopRuntime *runtime, PopControllerState *state,
                        uint16_t helper_id, uint16_t *required_helper);
#endif
