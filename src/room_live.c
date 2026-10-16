#include "room_live.h"
#include <stddef.h>

bool pop_room_live_cell(const PopWorldState *world,const uint8_t links[256],
                        unsigned index,uint8_t *type,uint8_t *spec)
{
    if(world==NULL||links==NULL||index>=720||type==NULL||spec==NULL)return false;
    uint8_t visual_type=world->types[index]&31u;
    uint8_t visual_spec=world->specs[index];
    if((visual_type==6||visual_type==15)&&(links[visual_spec]&31u)>=2){
        if(visual_type==6)visual_type=5;
        else{visual_type=1;visual_spec=0;}
    }
    *type=visual_type;*spec=visual_spec;
    return true;
}
