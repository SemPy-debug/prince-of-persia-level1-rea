#ifndef POP_WORLD_COLLISION_H
#define POP_WORLD_COLLISION_H
#include "world_query.h"
/* Additional source-backed compound predicates. Context is PopWorldState*.
 * Unsupported IDs/data or an external world effect leave state, result and
 * world unchanged. D096's EE03 spike-effect branch is such a boundary. */
bool pop_world_collision_query(void *world,uint16_t helper_id,uint8_t argument,
                               PopControllerState *state,PopControllerQueryResult *result);
/* Extended scan can tail-call the realEE03 producer. All objects and external
 * callbacks must belong to the same caller-staged transaction; the caller
 * rolls them back together on false. The pure wrapper above stays read-only. */
bool pop_world_collision_query_io(void *world,uint16_t helper_id,uint8_t argument,
    PopControllerState *state,PopControllerQueryResult *result,const PopControllerIO *io);
#endif
