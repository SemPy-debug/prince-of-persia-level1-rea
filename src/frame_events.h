#ifndef POP_FRAME_EVENTS_H
#define POP_FRAME_EVENTS_H
#include "controller.h"
/* SourceF924/F930/D92A/E00C plusD927 clock conversion. IO refers to a
 * caller-staged native runtime; no CPU state or executable image is stored. */
bool pop_frame_event_query(uint16_t helper_id,uint8_t argument,
    PopControllerState *state,PopControllerQueryResult *result,
    const PopControllerIO *io);
#endif
