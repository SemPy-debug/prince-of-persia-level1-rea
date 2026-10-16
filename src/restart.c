#include "restart.h"
#include "data.h"
#include "newgame.h"
#include "world_tick.h"
#include <stddef.h>
#include <string.h>
extern const uint8_t pop_reload_world[1440],pop_reload_links[256],pop_reload_info[256];
extern const uint8_t pop_reload_gate_specs[2],pop_reload_descriptor[2];
typedef struct Restart {PopGame *game;PopControllerIO io;PopControllerQueryResult result;} Restart;
static bool need(Restart *r,uint16_t helper){r->game->required_helper=helper;return false;}
static bool query(Restart *r,uint16_t helper,uint8_t argument){
    r->result.a=argument;
    if(pop_runtime_query(&r->game->runtime,helper,argument,&r->game->state,&r->result))return true;
    return need(r,r->game->runtime.missing_helper?r->game->runtime.missing_helper:helper);
}
static bool room_pointers(Restart *r,uint8_t room){
    uint8_t low,high,*a=r->game->state.aux;unsigned pointer;
    if(room<1||room>24)return need(r,0x04cc);
    if(!pop_data_read(NULL,(uint16_t)(0x1220u+2u*(room-1u)),&low)||
       !pop_data_read(NULL,(uint16_t)(0x1221u+2u*(room-1u)),&high))return need(r,0x04cc);
    pointer=(0xb700u+low+256u*high)&65535u;
    a[0x24]=(uint8_t)pointer;a[0x25]=(uint8_t)(pointer>>8);pointer=(pointer+720u)&65535u;
    a[0x26]=(uint8_t)pointer;a[0x27]=(uint8_t)(pointer>>8);return true;
}
/* E036/E7C2/E7D2/1296/1F69 normalize the freshly loaded type/spec data.
 * Only the original first-level gate encoding1/2 is admitted; both table
 * bytes are pinned typed data, not an executable source-image reader. */
static bool initialize_specs(Restart *r){
    uint8_t *a=r->game->state.aux,raw,spec,value;uint16_t tp,sp;
    a[0x23]=(uint8_t)(pop_reload_info[0]-1u);
    do{
        if(!room_pointers(r,a[0x23]))return false;
        tp=(uint16_t)(a[0x24]+256u*a[0x25]);sp=(uint16_t)(a[0x26]+256u*a[0x27]);
        for(int cell=29;cell>=0;--cell){
            if(!pop_runtime_read(&r->game->runtime,(uint16_t)(tp+(unsigned)cell),&raw))return need(r,0xe036);
            raw&=31u;
            if(raw!=4&&raw!=10&&raw!=11)continue;
            value=0;
            if(raw!=11){
                if(!pop_runtime_read(&r->game->runtime,(uint16_t)(sp+(unsigned)cell),&spec))return need(r,0xe036);
                if(raw==4){if(spec<1||spec>2)return need(r,0x1f7e);value=pop_reload_gate_specs[spec-1u];}
                else value=(uint8_t)(spec<<5);
            }
            if(!pop_runtime_write(&r->game->runtime,(uint16_t)(sp+(unsigned)cell),value))return need(r,0xe036);
        }
        --a[0x23];
    }while(a[0x23]!=0);
    return true;
}
static bool initialize_guards(Restart *r){
    uint8_t cell;unsigned room;
    for(room=24;room>0;--room){
        cell=r->game->runtime.guards.bytes[room-1u];
        if(cell>=30)continue;
        if(!query(r,0xd012,(uint8_t)(cell%10u)))return false;
        if(!pop_runtime_write(&r->game->runtime,(uint16_t)(0xbf76u+room),(uint8_t)(r->result.a+14u))||
           !pop_runtime_write(&r->game->runtime,(uint16_t)(0xbfbeu+room),0))return need(r,0xe042);
    }
    return true;
}
static bool initialize_player(Restart *r){
    static const uint8_t cleared[]={0x47,0x48,0xa7,0xc5,0x9f,0xcd,0xe7,0x4e,0x8a};
    uint8_t *a=r->game->state.aux,cell=pop_reload_info[0x41],height;
    a[0x4b]=pop_reload_info[0x40];a[0x44]=(uint8_t)(cell%10u);a[0x45]=(uint8_t)(cell/10u);
    if(!query(r,0xd012,a[0x44]))return false;
    a[0x41]=(uint8_t)(r->result.a+14u);a[0x43]=(uint8_t)(pop_reload_info[0x42]^255u);
    a[0xd0]=a[0xe1];a[0xce]=a[0xe1];
    r->result.x=2;r->result.y=0;
    if(!query(r,0xd021,5)||!query(r,0xee06,r->result.a))return false;
    pop_controller_select_sequence(&r->game->state,7);
    if(!pop_data_read(NULL,(uint16_t)(0x127fu+a[0x45]),&height))return need(r,0xe039);
    a[0x42]=height;a[0x4f]=255;a[0x4d]=0;
    for(unsigned i=0;i<sizeof(cleared);++i)a[cleared[i]]=0;
    if(!query(r,0x450c,a[0x4a]))return false;
    memcpy(a+0x50,a+0x40,16);return true;
}
static bool scan_start_exit(Restart *r){
    uint8_t raw,*a=r->game->state.aux;uint16_t pointer;
    if(!room_pointers(r,a[0x5b]))return false;
    pointer=(uint16_t)(a[0x24]+256u*a[0x25]);
    for(int cell=29;cell>=0;--cell){
        if(!pop_runtime_read(&r->game->runtime,(uint16_t)(pointer+(unsigned)cell),&raw))return need(r,0x2629);
        if((raw&31u)==16)return need(r,0xee15); /* Absent from original level1 start room. */
    }
    return true;
}
static bool prepare(Restart *r){
    static const uint16_t cleared[]={0x21,0x318,0xc7,0xcb,0x86,0x88,0xdf,0x95,0x30d,
        0xd1,0xe5,0xe6,0xd3,0xcf,0xcc,0xdb,0x309,0xa0};
    uint8_t *a=r->game->state.aux;
    /* 0445/0AF9 stores this fill byte before MAIN presentation work. F92D
     * replaces the level resource, not unrelated queue/dirty storage. */
    a[0x31d]=160;a[0x3f0]=pop_reload_descriptor[0];a[0x3f1]=pop_reload_descriptor[1];
    memcpy(&r->game->runtime.world,pop_reload_world,1440);
    memcpy(r->game->runtime.effect_memory.link_map,pop_reload_links,256);
    memcpy(&r->game->runtime.guards,pop_reload_info+0x47,144);
    if(!initialize_specs(r)||!initialize_guards(r))return false;
    for(unsigned i=0;i<sizeof(cleared)/sizeof(cleared[0]);++i)a[cleared[i]]=0;
    if(a[0x319]!=3)a[0x319]=0;
    r->game->runtime.sound_count=0; /* D906: old sound slots remain. */
    /* 0427 clears presentation lists B115/B143, outside owned game state. */
    for(unsigned i=0;i<10;++i){a[0x360+i]=255;a[0x370+i]=255;a[0x380+i]=255;a[0x390+i]=255;}
    a[0x39b]=255;
    for(unsigned i=0;i<10;++i)a[0x30eu+i]=0;
    a[0x30a]=1;a[0xc6]=255;a[0x6d]=2;a[0x63]=86;
    if(!initialize_player(r))return false;
    a[0x30a]=0;a[0x30b]=1;a[0xdb]=20;
    return scan_start_exit(r);
}
static bool begin(PopGame *game,PopGame *work,Restart *r){
    if(!game||!game->initialized)return false;
    *work=*game;work->required_helper=0;work->state.required_helper=0;work->runtime.missing_helper=0;
    if(work->state.aux[0x3f4]!=1){game->required_helper=0x20e2;return false;}
    r->game=work;r->io=(PopControllerIO){pop_runtime_query,pop_runtime_read,&work->runtime};
    r->result=(PopControllerQueryResult){0};return true;
}
static bool failed(PopGame *game,const PopGame *work){
    game->required_helper=work->required_helper?work->required_helper:
        (work->state.required_helper?work->state.required_helper:0x20e2);return false;
}
bool pop_game_restart_prepare(PopGame *game){
    PopGame work;Restart r;
    if(!begin(game,&work,&r))return false;
    if(!prepare(&r))return failed(game,&work);
    *game=work;return true;
}
bool pop_game_restart(PopGame *game,PopRestartCommit commit){
    PopGame work;Restart r;
    if(!begin(game,&work,&r))return false;
    if(!prepare(&r))return failed(game,&work);
    work.state.aux[0xa6]=work.state.aux[0x5b];
    if(pop_world_tick_activate_room(&work.state,&r.io)!=POP_CONTROLLER_COMPLETE)return failed(game,&work);
    if(!commit){work.required_helper=0x2439;return failed(game,&work);}
    if(!commit(&work.runtime,&work.state,0x2439,&work.required_helper))return failed(game,&work);
    *game=work;return true;
}
bool pop_game_new_game(PopGame *game,PopRestartCommit commit){
    PopGame work;PopControllerQueryResult result={0};PopControllerIO io;
    if(!game||!game->initialized)return false;
    work=*game;work.required_helper=0;work.state.required_helper=0;work.runtime.missing_helper=0;
    io=(PopControllerIO){pop_runtime_query,pop_runtime_read,&work.runtime};
    if(!pop_new_game_initialize(1,&work.state,&result,&io))return failed(game,&work);
    if(!pop_game_restart(&work,commit))return failed(game,&work);
    *game=work;return true;
}
