#ifndef POP_ITEMS_H
#define POP_ITEMS_H
#include "controller.h"
typedef bool (*PopItemWrite)(void *context,uint16_t address,uint8_t value);
/* E030 original item removal/dirty marks; F90F item effects. IO/write refer
 * to one caller-staged runtime; unsupported calls preserve localAUX/result. */
bool pop_item_query(uint16_t helper_id,uint8_t argument,PopControllerState *state,
    PopControllerQueryResult *result,const PopControllerIO *io,PopItemWrite write);
#endif
