#ifndef POP_TILE_CONTEXTS_H
#define POP_TILE_CONTEXTS_H
#include "../src/tile_producer.h"
typedef struct PopTileContextSeed { PopTileContext context; int16_t owners[3]; } PopTileContextSeed;
extern const PopTileContextSeed pop_tile_contexts[24][40];
/* Owners are world indices0..719 or-1 for source boundary defaults.
 * Copy the seed, project live owner state, then carry live counts/parameters. */
const PopTileContextSeed *pop_tile_context_initial(unsigned room,unsigned group);
#endif
