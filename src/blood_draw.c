#include "blood_draw.h"
#include <stddef.h>
#include <string.h>
PopActorDrawStatus pop_blood_draw_projected(const PopControllerState *projected,
    PopSceneDraw *out,uint16_t *required_helper){
    uint8_t aux[256],pose;uint16_t pixel;int delta;
    PopActorDrawStatus status;
    if(!projected||!out)return POP_ACTOR_DRAW_INVALID;
    if(required_helper)*required_helper=0;
    memcpy(aux,projected->aux,sizeof aux);pose=aux[0x40];aux[0x75]=255;
    if(pose==178)return POP_ACTOR_DRAW_SKIPPED;
    delta=pose==177?-5:5;
    if(pose==185||(pose>=106&&pose<111))aux[0x73]=(uint8_t)(aux[0x73]+4u);
    else if(pose!=177)aux[0x73]=(uint8_t)(aux[0x73]-(aux[0x4d]==0?15u:11u));
    /* D09C/D1C9: byte-signed displacement applied to a16-bit facing-relative
     * projected coordinate, then52D2's actor/facing parity alignment. */
    pixel=(uint16_t)(aux[0x71]+256u*aux[0x72]);
    pixel=(uint16_t)((int)pixel+((aux[0x74]&128u)?-delta:delta));
    if((((aux[0x4d]?1u:0u)^(uint8_t)pixel^aux[0x74])&1u)==0)++pixel;
    aux[0x71]=(uint8_t)pixel;aux[0x72]=(uint8_t)(pixel>>8);
    aux[0x70]=65;aux[0x7a]=0;aux[0x76]=0;aux[0x78]=0;aux[0x79]=40;aux[0x77]=192;
    /* Actual1E69..1E71 sends both kinds3 and5 to4C06/5029. Reuse that
     * identical masked-draw path without adding a new global actor policy. */
    status=pop_actor_draw_projected(aux,sizeof aux,3,out);
    if(status==POP_ACTOR_DRAW_UNSUPPORTED&&required_helper)*required_helper=0x4c06;
    return status;
}
