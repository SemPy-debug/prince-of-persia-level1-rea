#ifndef POP_PLAYER_RENDER_H
#define POP_PLAYER_RENDER_H

#include "actor_draw.h"
#include "world_query.h"

/* Source23C0/23D5 visible-player pose0 takes E2E021 with A25 and draws no
 * body. This is a pure wait: return its exact helper-entry-to-RTS cycle cost
 * for host pacing; do not advance animation, RNG, clocks or level state here.
 * A body projection does not exist for this branch; the bool projection API
 * retains its E021 diagnostic, while the draw API returns SKIPPED. */
uint32_t pop_player_render_delay_cycles(const PopControllerState *state);

/* Read-only rendering projection from the source23D5 player path. Loads the
 * permanent actor50..5F into a state copy, refreshes frame/column, projects,
 * computes source bounds, source object-cell and live world-dependent clips.
 * Full-room rendering does not need dirty rectangle/list counter mutations;
 * no mutable redraw-state ABI is claimed. Output70..7A is the original533F
 * pre-byte-division contract. Failures preserve *projection and name a source
 * helper when possible. Input world/state are always preserved. */
bool pop_player_render_project(const PopWorldState *world,
                                const PopControllerState *state,
                                PopControllerState *projection,
                                uint16_t *required_helper);

/* Applies original player-room visibility and the projected command adapter.
 * Returns explicit skipped/unsupported/invalid statuses. All non-ready
 * results preserve *draw. Special unknown paths are not replaced by defaults. */
PopActorDrawStatus pop_player_render_draw(const PopWorldState *world,
                                         const PopControllerState *state,
                                         PopSceneDraw *draw,
                                         uint16_t *required_helper);

/* Original23FC repeats the source clip pipeline with permanent60..6F.
 * This interface admits first-level semantic guard kind2; kind1's special
 * mode3 path is explicitly unsupported. Projection remains a pure copy. */
bool pop_guard_render_project(const PopWorldState *world,
                               const PopControllerState *state,
                               PopControllerState *projection,
                               uint16_t *required_helper);

/* Original D027/D391 attachment producer after an actor's clip projection.
 * Uses the immutable three-byte source attachment records, direction-aware
 * D1C9 pixel offsets and the body's actual clip rectangle. */
PopActorDrawStatus pop_actor_attachment_draw(const PopControllerState *projection,
                                             PopSceneDraw *draw,
                                             uint16_t *required_helper);

#endif
