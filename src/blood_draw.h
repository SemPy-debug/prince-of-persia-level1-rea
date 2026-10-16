#ifndef POP_BLOOD_DRAW_H
#define POP_BLOOD_DRAW_H
#include "controller.h"
#include "actor_draw.h"
/* Original52C9 saves/restores the caller's projection around52D2.
 * Input includes the current working actor40..4F and pre-list projection70..7F.
 * The returned draw is blood kind5; no live AUX or destination changes on
 * skipped/unsupported/invalid results. Does not decide CF/D2 eligibility. */
PopActorDrawStatus pop_blood_draw_projected(const PopControllerState *projected,
    PopSceneDraw *out,uint16_t *required_helper);
#endif
