#ifndef POP_GUARD_H
#define POP_GUARD_H
#include "controller.h"

enum { POP_GUARD_RECORD_BYTES=144, POP_GUARD_ROOM_COUNT=24 };
/* Six24-byte room arrays: sourceBF47 row/cell,BF5F facing,BF77 x,
 * BF8F sequence-low,BFA7 skill,BFBF sequence-high. Each original base is one
 * byte earlier because source code indexes rooms1..24. No embedded pointers. */
typedef struct PopGuardMemory { uint8_t bytes[POP_GUARD_RECORD_BYTES]; } PopGuardMemory;
bool pop_guard_memory_reset(PopGuardMemory *memory);
/* Overlay exactly BF47..BFD6. Other Info/blueprint bytes remain immutable. */
bool pop_guard_memory_read(void *memory,uint16_t source_address,uint8_t *value);
bool pop_guard_memory_write(void *memory,uint16_t source_address,uint8_t value);
/* Original5499 builds relative AI inputs. Original3CA0 then delegates those
 * inputs to the common actor controller. Effects require caller staging. */
PopControllerResult pop_guard_build_inputs(PopControllerState *state,const PopControllerIO *io);
PopControllerResult pop_guard_decide(PopControllerState *state,const PopControllerIO *io);
/* Original5C80/5412 activates the current room's retained guard record. */
PopControllerResult pop_guard_load_room(PopControllerState *state,const PopControllerIO *io);
/* OriginalE027/E905 event and F900/F936 dismissal shared by guard lifecycle. */
PopControllerResult pop_guard_notify_death(PopControllerState *state,const PopControllerIO *io);
void pop_guard_dismiss(PopControllerState *state);
#endif
