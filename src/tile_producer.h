#ifndef POP_TILE_PRODUCER_H
#define POP_TILE_PRODUCER_H
#include "room_scene.h"
#include "foreground.h"

typedef struct PopTileParameters { uint8_t x, y, image, mode; } PopTileParameters;
typedef struct PopTileContext {
    uint16_t entry; /* Original source161E body or1639 above-room partial. */
    uint8_t current_type,current_spec,previous_type,previous_spec,below_type,below_spec;
    uint8_t x_byte,floor_y,bottom_y,column,cell,source_row;
    uint8_t room,exit_room,right_room,player_room,player_column,player_row;
    uint8_t palette,clip_top;
    uint8_t background_total,foreground_total,middle_total;
    PopTileParameters parameters;
} PopTileContext;

enum { POP_TILE_MAX_BACKGROUND=64, POP_TILE_MAX_FOREGROUND=32, POP_TILE_MAX_MIDDLE=4 };
typedef struct PopTileOutput {
    PopRoomBackgroundDraw background[POP_TILE_MAX_BACKGROUND];
    PopForegroundDraw foreground[POP_TILE_MAX_FOREGROUND];
    PopSceneDraw middle[POP_TILE_MAX_MIDDLE];
    size_t background_count,foreground_count,middle_count;
    uint8_t background_total,foreground_total,middle_total;
    PopTileParameters parameters;
    bool exit_y_written;
    uint8_t exit_y;
    uint16_t required_helper;
} PopTileOutput;

/* Pure source-native command production. Types/specs must already reflect
 * original1EDE plate/link projection for every current/previous/below input.
 * Counts and parameter carry are explicit so callers may concatenate groups.
 * Failure leaves output unchanged. This does not mutate the world or runCPU. */
bool pop_tile_produce(const PopTileContext *context,PopTileOutput *out);
#endif
