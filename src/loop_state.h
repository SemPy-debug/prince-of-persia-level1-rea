#ifndef POP_LOOP_STATE_H
#define POP_LOOP_STATE_H
#include "controller.h"

typedef bool (*PopLoopWrite)(void *context,uint16_t address,uint8_t value);
/* Raw Apple keyboard/buttons supplied by a host adapter, not polled by native
 * gameplay. Only D91E currently consumes the buttons. */
typedef struct PopLoopHardware {
    uint8_t keyboard_data,keyboard_status,button0,button1;
} PopLoopHardware;
/* D921's four9-byte arrays plus its cycle index. Source slots0..7 are
 * display records; slot8 can hold the terminatingFF. This is data only. */
typedef struct PopLoopModal {
    uint8_t paused_objects[37];
    uint8_t resource;
    bool active;
} PopLoopModal;
typedef struct PopLoopPausedDraw {
    uint8_t x,y,phase,clip;
    bool active;
} PopLoopPausedDraw;
typedef enum PopLoopModalResult {
    POP_LOOP_MODAL_IDLE=0,POP_LOOP_MODAL_OPENED=1,
    POP_LOOP_MODAL_ACTIVE=2,POP_LOOP_MODAL_REQUIRED=3,
    POP_LOOP_MODAL_INVALID=4
} PopLoopModalResult;
typedef struct PopLoopVisual {
    uint8_t fill_byte,page;
    bool low_resolution,fill_required;
} PopLoopVisual;
void pop_loop_modal_reset(PopLoopModal *modal);
/* Native D921 data builder, independently comparable to its actual return. */
bool pop_loop_build_paused_objects(PopLoopModal *modal,PopControllerState *state,
    PopControllerQueryResult *result,const PopControllerIO *io);
/* Source D924/DC70's paused torch rotation and EE24/F331 random phase.
 * Only the private paused list, RNG9E and phase scratchE8 are committed.
 * The returned draw record feeds a pure host renderer; blitter registers,
 * keyboard polling and music progression are separate boundaries. */
bool pop_loop_modal_torch_step(PopLoopModal *modal,PopControllerState *state,
    PopLoopPausedDraw *draw);
/* Host event boundary replacing04B7/04DB/04BA. Begin stops before resource
 * playback; the host owns audio/presentation/input timing. Original resource3
 * is an automatically completed music cue, not an Enter-required dialog.
 * Finish applies the source0319/page/latch changes after playback finishes.
 * No gameplay updates occur while active. Legacy modal/message names remain
 * for ABI compatibility with independently checked caller components. */
PopLoopModalResult pop_loop_modal_begin(PopLoopModal *modal,
    PopControllerState *state,const PopControllerIO *io);
bool pop_loop_modal_dismiss(PopLoopModal *modal,PopControllerState *state);
/* Typed visual operations: E2E003 selects a cached fill resource and lores
 * page1; E2E00F selects hires and page2 when AUX00==0. CPU open-bus register
 * values and VBL pacing belong to the host presentation boundary. */
bool pop_loop_visual_event(uint16_t helper,uint8_t argument,
    PopControllerState *state,PopLoopVisual *visual);

/* Source218C's two semantic clears; RNG is a separate0436 query because its
 * A/carry result is observed by other source callers too. */
void pop_loop_clear_frame_flags(PopControllerState *state);
/* Source helper semantics outside224E. Caller must stage IO-owned state along
 * with AUX; dependencies can mutate the same staged runtime. On failure AUX
 * and function result roll back and required_helper names the unmet boundary.
 * Active2641 modal display is explicit, never treated as a completed tick. */
bool pop_loop_state_query(uint16_t helper,uint8_t argument,
    PopControllerState *state,PopControllerQueryResult *result,
    const PopLoopHardware *hardware,const PopControllerIO *io,PopLoopWrite write);
#endif
