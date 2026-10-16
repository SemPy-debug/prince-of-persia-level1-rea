#ifndef POP_MOTION_H
#define POP_MOTION_H
#include "controller.h"

/* Original E03F/E8AB, physical language-card E bank2. */
void pop_motion_update_gravity(PopControllerState *state);
/* Original E033/E8D0. Every action integrates Y; only action4 also integrates
 * facing-relative X and refreshes frame/block geometry through3A09. */
PopControllerResult pop_motion_integrate_velocity(PopControllerState *state,
                                                  const PopControllerIO *io);
/* F909/F9B5: source-backed response to opponent proximity/facing conditions. */
PopControllerResult pop_motion_check_opponent_contact(PopControllerState *state,
                                                      const PopControllerIO *io);
/* 3A03/3A3B: supporting-floor loss, falling/landing and ledge-grab decisions. */
PopControllerResult pop_motion_resolve_floor(PopControllerState *state,
                                            const PopControllerIO *io);
PopControllerResult pop_motion_check_tile_effects(PopControllerState *state,
                                                 const PopControllerIO *io);
PopControllerResult pop_motion_check_spikes(PopControllerState *state,
                                           const PopControllerIO *io);
PopControllerResult pop_motion_correct_gate(PopControllerState *state,
                                            const PopControllerIO *io);
PopControllerResult pop_motion_correct_wall(PopControllerState *state,
                                            const PopControllerIO *io);
/* Original4500/452C: prepare collision masks and room records for this update. */
PopControllerResult pop_motion_build_collision(PopControllerState *state,
                                              const PopControllerIO *io);
typedef bool (*PopMotionWrite)(void *context,uint16_t source_address,uint8_t value);
/* World/effect callbacks must use the caller's staged world transaction: this
 * function preserves the original write-before-damage/animation order. */
PopControllerResult pop_motion_check_chompers(PopControllerState *state,
                                             const PopControllerIO *io,
                                             PopMotionWrite write);
/* Original451E/4B4D and4512/4AA7 use guard-specific wall/trap decisions. */
PopControllerResult pop_motion_correct_guard_wall(PopControllerState *state,
                                                 const PopControllerIO *io);
PopControllerResult pop_motion_check_guard_chompers(PopControllerState *state,
                                                   const PopControllerIO *io,
                                                   PopMotionWrite write);
#endif
