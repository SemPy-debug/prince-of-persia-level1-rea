#ifndef POP_GEOMETRY_H
#define POP_GEOMETRY_H

#include <stdbool.h>
#include <stdint.h>

/* High-level native geometry over the original semantic variable block.
 * These functions do not execute original machine code. They use immutable
 * source-extracted tables through pop_data_read. At least256 bytes are needed.
 * Source/helper mappings are in research/geometry-evidence/fragments/INDEX.json. */
bool pop_geometry_load_frame(uint8_t aux[256]);       /* D069 -> D701 */
bool pop_geometry_update_column(uint8_t aux[256]);    /* D0A5 -> D2DC */
bool pop_geometry_project_actor(uint8_t aux[256]);    /* D066 -> D34A */
/* D033/D47A actor bounds, using original immutable image dimensions. Defined
 * separately in geometry_bounds.c; link assets.c and generated image data. */
bool pop_geometry_bounds(uint8_t aux[256]);

/* Exact original type predicates, not universal solid/empty classifications. */
uint8_t pop_geometry_d684(uint8_t type);
uint8_t pop_geometry_d69e(uint8_t type);

#endif
