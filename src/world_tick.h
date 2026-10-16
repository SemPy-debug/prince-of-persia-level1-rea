#ifndef POP_WORLD_TICK_H
#define POP_WORLD_TICK_H
#include "controller.h"
typedef bool (*PopWorldTickWrite)(void *context,uint16_t address,uint8_t value);
/* D02A/D174: current room and its adjacent/corner room IDs. */
bool pop_world_tick_room_links(PopControllerState *state);
/*5415/5C22 direction0:left,1:right,2:above,3:below. No drawing is performed. */
bool pop_world_tick_transfer_actor(PopControllerState *state,uint8_t direction);
/*540C/59D9: player room selection and guard follow/persistence. CameraCB is
 * activated later by24D3; this sets candidateA6. All callbacks/writes must use
 * the caller's staged runtime transaction. */
PopControllerResult pop_world_tick_rooms(PopControllerState *state,
    const PopControllerIO *io,PopWorldTickWrite write);
/*24D3: activate nonzero changed roomA6, refresh room/actor state, seed its
 * mechanisms and load its original guard record. Caller stages runtime IO. */
PopControllerResult pop_world_tick_activate_room(PopControllerState *state,
    const PopControllerIO *io);
/*540F/5AA9: dismiss or persist an opponent falling below the room. */
PopControllerResult pop_world_tick_guard_fall(PopControllerState *state,
    const PopControllerIO *io,PopWorldTickWrite write);
#endif
