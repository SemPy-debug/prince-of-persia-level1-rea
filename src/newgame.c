#include "newgame.h"
#include <stddef.h>
bool pop_new_game_initialize(uint8_t level,PopControllerState *state,
    PopControllerQueryResult *result,const PopControllerIO *io) {
    static const uint16_t cleared[]={0x22,0x97,0x200,0x201,0x3f8,0x21,0xde,
        0x318,0x9f,0x212,0x31f,0x306,0x307,0x301};
    PopControllerState s;PopControllerQueryResult r;size_t i;
    if(!state||!result)return false;
    s=*state;r=*result;s.aux[0x3f4]=level;s.aux[0x31c]=level;
    r.a=level;r.carry=level>=1;r.zero=level==1;
    r.negative=((uint8_t)(level-1u)&128u)!=0;
    if(level==1){
        r.a=3;r.x=25;r.zero=false;r.negative=false;
        if(!io||!io->query){state->required_helper=0xd930;return false;}
        s.required_helper=0;
        if(!io->query(io->context,0xd930,3,&s,&r)){
            state->required_helper=s.required_helper?s.required_helper:0xd930;return false;
        }
    }
    s.aux[0xe1]=3;
    for(i=0;i<sizeof(cleared)/sizeof(cleared[0]);++i)s.aux[cleared[i]]=0;
    s.aux[0x300]=255;s.aux[0x302]=255;s.aux[0x30c]=1;
    r.a=1;r.zero=false;r.negative=false;
    *state=s;*result=r;return true;
}
