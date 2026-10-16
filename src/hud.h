#ifndef POP_HUD_H
#define POP_HUD_H
#include "controller.h"
#include "render.h"

/* Explicit full-view HUD over a completed source rendering state. The source
 * page field00 has already flipped atE006. No simulation state is changed. */
bool pop_hud_apply(const PopControllerState *state, uint8_t *page, size_t size);
#endif
