#include "room_live.h"
#include "foreground.h"
#include "tile_producer.h"
#include "../generated/tile_contexts.h"
#include <string.h>

bool pop_room_live_build(const PopWorldState *world,const uint8_t links[256],
                         const PopControllerState *state,
                         const PopRoomScene *initial_room,
                         PopRoomLiveScene *out)
{
    PopRoomLiveScene result;
    PopForegroundDraw foreground[100];
    size_t foreground_count=0;
    uint8_t exit_room,right_room;
    if(world==NULL||links==NULL||state==NULL||initial_room==NULL||out==NULL||
       initial_room!=pop_room_scene_initial(initial_room->room_id)||
       !pop_world_initial_info(0x40,&exit_room)||
       !pop_world_initial_neighbor(initial_room->room_id,POP_WORLD_RIGHT,&right_room))return false;
    memset(&result,0,sizeof result);
    result.room=*initial_room;
    result.room.background_count=0;result.room.middle_count=0;
    PopTileParameters parameters={0};
    for(unsigned group=0;group<40;++group){
        const PopTileContextSeed *seed=pop_tile_context_initial(initial_room->room_id,group);
        PopTileOutput produced;
        if(seed==NULL)return false;
        PopTileContext context=seed->context;
        uint8_t *types[3]={&context.current_type,&context.previous_type,&context.below_type};
        uint8_t *specs[3]={&context.current_spec,&context.previous_spec,&context.below_spec};
        for(unsigned owner=0;owner<3;++owner){
            if(seed->owners[owner]>=0&&!pop_room_live_cell(world,links,
                (unsigned)seed->owners[owner],types[owner],specs[owner]))return false;
        }
        context.room=(uint8_t)initial_room->room_id;
        context.exit_room=exit_room;context.right_room=right_room;
        context.player_room=state->aux[0x5b];
        context.player_column=state->aux[0x54];context.player_row=state->aux[0x55];
        context.palette=state->aux[0x303];
        context.background_total=(uint8_t)result.room.background_count;
        context.foreground_total=(uint8_t)foreground_count;
        context.middle_total=(uint8_t)result.room.middle_count;
        if(group==0)parameters=seed->context.parameters;
        /* Actual161E/1639 caller reloads0001 fromF3 for each cell, while
         * the remaining parameter bytes and accepted-list totals carry. */
        context.parameters=parameters;context.parameters.x=context.x_byte;
        if(!pop_tile_produce(&context,&produced)||
           produced.background_count>200-result.room.background_count||
           produced.foreground_count>100-foreground_count||
           produced.middle_count>45-result.room.middle_count)return false;
        memcpy(result.background+result.room.background_count,produced.background,
               produced.background_count*sizeof *result.background);
        memcpy(foreground+foreground_count,produced.foreground,
               produced.foreground_count*sizeof *foreground);
        memcpy(result.middle+result.room.middle_count,produced.middle,
               produced.middle_count*sizeof *result.middle);
        result.room.background_count+=produced.background_count;
        foreground_count+=produced.foreground_count;
        result.room.middle_count+=produced.middle_count;
        if(produced.background_total!=result.room.background_count||
           produced.foreground_total!=foreground_count||
           produced.middle_total!=result.room.middle_count)return false;
        parameters=produced.parameters;
        /* The pure view does not commit source0215. Its persistent writer is
         * reconstructed separately by render_commit in caller order. */
    }
    memset(result.foreground_preserve,255,sizeof result.foreground_preserve);
    if(!pop_foreground_apply(result.foreground_xor,POP_HGR_PAGE_BYTES,
        foreground,foreground_count)||
       !pop_foreground_apply(result.foreground_preserve,POP_HGR_PAGE_BYTES,
        foreground,foreground_count))return false;
    for(size_t i=0;i<POP_HGR_PAGE_BYTES;++i)
        result.foreground_preserve[i]^=result.foreground_xor[i];
    *out=result;
    out->room.background=out->background;out->room.middle=out->middle;
    out->room.foreground_preserve=out->foreground_preserve;
    out->room.foreground_xor=out->foreground_xor;
    return true;
}
