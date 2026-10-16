#ifndef POP_WORLD_QUERY_H
#define POP_WORLD_QUERY_H

#include "controller.h"
#include "world_state.h"

/* Native geometry/world predicates for the controller's source-named callback
 * ABI. Context is PopWorldState*. Unknown helpers or unsupported data leave
 * the complete state and result unchanged and return false. No opcode loop,
 * target instructions or recorded oracle returns are used by these helpers. */
bool pop_world_query(void *world, uint16_t helper_id, uint8_t argument,
                     PopControllerState *state, PopControllerQueryResult *result);

#endif
