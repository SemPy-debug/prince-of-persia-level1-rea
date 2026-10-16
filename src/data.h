#ifndef POP_DATA_H
#define POP_DATA_H

#include <stdbool.h>
#include <stdint.h>

/* Read source-extracted immutable AUX1000..128F/2800..39FF/4521..452A data. Compatible
 * with sequence/controller read callbacks. Context is unused and may be NULL.
 * Returns false outside admitted windows and leaves *value unchanged. */
bool pop_data_read(void *context, uint16_t address, uint8_t *value);

#endif
