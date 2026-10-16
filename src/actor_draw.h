#ifndef POP_ACTOR_DRAW_H
#define POP_ACTOR_DRAW_H

#include "scene.h"

typedef enum PopActorDrawStatus {
    POP_ACTOR_DRAW_READY = 0,
    POP_ACTOR_DRAW_SKIPPED = 1,
    POP_ACTOR_DRAW_UNSUPPORTED = 2,
    POP_ACTOR_DRAW_INVALID = 3
} PopActorDrawStatus;

/* Convert a live original semantic projection, before533F changes pixel71/72
 * into object-list byteX/phase. Fields70=image,71/72=signed pixelX,73=bottomY,
 * 74=facing,76/77=top/bottom,78/79=left/right,7A=loaded character-table selector.
 * Source-kind0/4 uses the original A2/page gate and DF flash selector; kind3
 * is the ordinary masked attachment path. Kind2 admits the source normal
 * guard path when a full0216 color field is supplied and equalszero. Other
 * kinds, special guard colors and mode3 return an
 * explicit UNSUPPORTED result. The caller owns pose projection and clipping;
 * this function does not perform gameplay updates or infer legal actor poses.
 * No emulator, instruction execution or saved-frame lookup is used.
 * All non-READY results preserve *out and all calls preserve aux. */
PopActorDrawStatus pop_actor_draw_projected(const uint8_t *aux, size_t aux_bytes,
                                           uint8_t object_kind,
                                           PopSceneDraw *out);

#endif
