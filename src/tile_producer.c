#include "tile_producer.h"
#include <string.h>
#include "../generated/tile_data.h"

typedef struct Tile {
    const PopTileContext *c;
    PopTileOutput *o;
    PopTileParameters p;
    uint8_t gate_y,gate_top;
} Tile;

static bool append_background(Tile *t)
{
    if(t->o->background_total>=199 || t->p.y>=192)return true;
    if(t->o->background_count>=POP_TILE_MAX_BACKGROUND)return false;
    PopRoomBackgroundDraw d={(PopAssetTable)(t->p.image>>7),t->p.image&127u,
        t->p.x,t->p.y,t->p.mode,t->c->cell,t->c->current_type,t->c->previous_type,0};
    t->o->background[t->o->background_count++]=d;++t->o->background_total;
    return true;
}
static bool append_foreground(Tile *t)
{
    if(t->o->foreground_total>=99 || t->p.y>=192)return true;
    if(t->o->foreground_count>=POP_TILE_MAX_FOREGROUND)return false;
    PopForegroundDraw d={(PopAssetTable)(t->p.image>>7),t->p.image&127u,t->p.x,t->p.y,t->p.mode};
    t->o->foreground[t->o->foreground_count++]=d;++t->o->foreground_total;
    return true;
}
static bool background(Tile *t,uint8_t image,uint8_t x,uint8_t y,uint8_t mode)
{
    t->p=(PopTileParameters){x,y,image,mode};return append_background(t);
}
static bool foreground_pair(Tile *t)
{
    t->p.mode=4;if(!append_foreground(t))return false;
    t->p.mode=1;return append_foreground(t);
}
static bool loose_index(uint8_t spec,uint8_t *index)
{
    if(spec&128u){spec&=127u;if(spec>=11)spec=1;}
    if(spec>=11)return false;
    *index=spec;return true;
}
static bool void_floor(uint8_t type)
{return type==0||type==9||type==12||type>=26;}

/* Original190F/1955: the below-left cache and previous-cell overlay. */
static bool below_image(Tile *t)
{
    uint8_t type=t->c->below_type,image;
    if(type==20)image=tile_wall_below[t->c->below_spec<2?t->c->below_spec:0];
    else{
        image=tile_below_image[type];
        if(image==159){if(t->c->below_spec>=3)return true;image=tile_below_variant[t->c->below_spec];}
    }
    if(!image)return true;
    return background(t,image,t->c->x_byte,t->c->bottom_y,1);
}
static bool below_overlay(Tile *t)
{
    uint8_t image=tile_below_overlay[t->c->previous_type];if(!image)return true;
    t->p.image=image;t->p.y=t->c->bottom_y;t->p.mode=0;
    return append_background(t);
}
static bool empty_support(Tile *t)
{return !void_floor(t->c->current_type)||(below_image(t)&&below_overlay(t));}
static bool below_gate(Tile *t)
{
    if((t->c->current_type!=0&&t->c->current_type!=9&&t->c->current_type!=12)||t->c->below_type!=4)return true;
    t->p.y=t->c->bottom_y;t->p.image=13;t->p.mode=0;
    if(!append_background(t))return false;
    unsigned height=(t->c->below_spec<188?t->c->below_spec:188u)>>2;
    t->p.image=tile_gate_remainder[height&7u];t->p.mode=1;
    return append_background(t);
}
static bool previous_face(Tile *t)
{
    uint8_t type=t->c->previous_type,spec=t->c->previous_spec,image,y;
    if(t->c->current_type==20)return true;
    if(type==0){if(spec>=4)return true;image=tile_empty_previous[spec];y=tile_empty_previous_y[spec];}
    else if(type==1){unsigned i=spec<4?spec:0;image=tile_floor_previous[i];y=tile_floor_previous_y[i];}
    else if(type==20){image=tile_wall_previous[spec<2?spec:0];y=tile_previous_y[type];}
    else{
        image=tile_previous_face[type];y=tile_previous_y[type];
        if(image==158){if(spec>=3)return true;image=tile_previous_variant[spec];}
    }
    if(!image)return true;
    return background(t,image,t->c->x_byte,(uint8_t)(t->c->floor_y+y),1);
}
static bool base_piece(Tile *t)
{
    uint8_t image=tile_base[t->c->current_type];if(!image)return true;
    return background(t,image,t->c->x_byte,t->c->floor_y,0);
}
static bool face_piece(Tile *t)
{
    uint8_t type=t->c->current_type,image;
    if(type==11){uint8_t i;if(!loose_index(t->c->current_spec,&i))return false;image=tile_loose_face[i];}
    else image=tile_face[type];
    if(!image)return true;
    return background(t,image,t->c->x_byte,(uint8_t)(t->c->floor_y+tile_face_y[type]),1);
}
static bool current_face(Tile *t)
{
    uint8_t previous=t->c->previous_type;
    if(previous==26){
        if(t->c->current_type==12)return background(t,161,t->c->x_byte,t->c->floor_y,1);
    }else if(previous==7||previous==9||previous==12||previous==20){
        if(!base_piece(t))return false;
    }
    return face_piece(t);
}
static void gate_geometry(Tile *t)
{
    unsigned height=((t->c->previous_spec<188?t->c->previous_spec:188u)>>2)+1u;
    t->gate_top=(uint8_t)(t->c->bottom_y-62u);
    t->gate_y=(uint8_t)(t->c->floor_y-height);
}
static bool gate_underlay(Tile *t)
{
    if(!background(t,tile_previous_face[4],t->c->x_byte,(uint8_t)(t->c->floor_y+tile_previous_y[4]),2))return false;
    if(void_floor(t->c->current_type)&&!below_image(t))return false;
    return current_face(t);
}
static bool gate_background(Tile *t)
{
    gate_geometry(t);
    if((uint8_t)(t->gate_y+12u)>=t->c->floor_y){
        if(!gate_underlay(t))return false;
        t->p.y=(uint8_t)(t->gate_y-2u);t->p.image=68;t->p.mode=1;
    }else{t->p.y=t->gate_y;t->p.image=67;t->p.mode=2;}
    if(!append_background(t))return false;
    t->p.mode=2;t->p.image=55;t->p.y=(uint8_t)(t->gate_y-12u);
    for(unsigned n=0;n<64;++n){
        uint8_t y=t->p.y;
        if(y>=192)return true;
        if(y<7||(uint8_t)(y-7u)<t->gate_top)break;
        if(!append_background(t))return false;
        uint8_t next=(uint8_t)(y-8u);if(next==0)break;t->p.y=next;
    }
    uint8_t partial=(uint8_t)(t->p.y-t->gate_top+1u);
    if(partial&&partial<9){t->p.image=tile_gate_partial[partial];return append_background(t);}
    return true;
}
static bool spike(Tile *t,bool previous)
{
    uint8_t spec=previous?t->c->previous_spec:t->c->current_spec,index=(spec&128u)?5:spec;
    if(index>=10)return false;
    uint8_t image=previous?tile_spike_previous[index]:tile_spike_current[index];
    if(!image)return true;
    return background(t,image,t->c->x_byte,(uint8_t)(t->c->floor_y-1u),1);
}
static bool exit_gate(Tile *t)
{
    t->p.image=107;t->p.y=(uint8_t)(t->c->floor_y-12u);
    if(t->c->x_byte>=36)return true;
    t->p.x=(uint8_t)(t->c->x_byte+1u);t->p.mode=2;
    if(t->c->room!=t->c->exit_room&&!append_background(t))return false;
    uint8_t limit=(uint8_t)(t->c->bottom_y-67u);if(limit>=192)return true;
    uint8_t y=(uint8_t)(t->c->floor_y-14u-(t->c->floor_y<14?1u:0u)-(t->c->previous_spec>>2));
    t->o->exit_y_written=true;t->o->exit_y=y;t->p.y=y;
    for(unsigned n=0;n<64;++n){
        t->p.image=109;t->p.mode=0;if(!append_background(t))return false;
        t->p.image=108;t->p.mode=1;if(!append_background(t))return false;
        y=(uint8_t)(t->p.y-4u);if(y<limit)break;t->p.y=y;
        if(n==63)return false;
    }
    y=(uint8_t)(t->c->floor_y-64u);if(y>=192)return true;
    t->p.y=y;t->p.image=110;t->p.mode=2;return append_background(t);
}
static bool previous_dynamic(Tile *t)
{
    switch(t->c->previous_type){
    case 4:return gate_background(t);
    case 2:return spike(t,true);
    case 11:{uint8_t i;if(!loose_index(t->c->previous_spec,&i))return false;
        t->p.image=27;t->p.y=(uint8_t)(t->c->floor_y+tile_loose_offset[i]);t->p.mode=1;return append_background(t);}
    case 19:
        t->p.x=t->c->x_byte;t->p.y=t->c->floor_y;
        if(t->p.x==0)return true;
        if(t->c->previous_spec>=18)return false;
        ++t->p.x;t->p.y=(uint8_t)(t->p.y-43u);t->p.image=tile_torch[t->c->previous_spec];t->p.mode=2;
        return append_background(t);
    case 16:return exit_gate(t);
    default:return true;
    }
}
static bool bottom_piece(Tile *t)
{
    uint8_t type=t->c->current_type,image;t->p.mode=type==12?1:2;
    image=type==20?tile_wall_bottom[t->c->current_spec<2?t->c->current_spec:0]:tile_bottom[type];
    if(!image)return true;
    t->p.image=image;t->p.x=t->c->x_byte;t->p.y=t->c->bottom_y;
    return append_background(t);
}
static bool loose_bottom(Tile *t)
{
    if(t->c->current_type!=11)return true;
    uint8_t i;if(!loose_index(t->c->current_spec,&i))return false;
    uint8_t image=tile_loose_bottom[i];if(!image)return true;
    return background(t,image,t->c->x_byte,t->c->bottom_y,2);
}
static bool potion(Tile *t)
{
    uint8_t kind=t->c->current_spec&224u,phase=t->c->current_spec&31u;
    uint8_t alignment=kind>64?3:2;
    if(kind==0||phase>=9)phase=0;
    t->p.x=(uint8_t)(t->c->x_byte+2u);
    t->p.y=(uint8_t)(t->c->floor_y-14u-(kind>=64?4u:0u));
    t->p.image=tile_potion[phase];t->p.mode=2;
    if(t->o->middle_total>=45)return true;
    if(t->o->middle_count>=POP_TILE_MAX_MIDDLE)return false;
    PopSceneDraw draw={(PopAssetTable)(t->p.image>>7),t->p.image&127u,
        {t->p.x,t->p.y,alignment,2,0,0,40,192}};
    t->o->middle[t->o->middle_count++]=draw;++t->o->middle_total;return true;
}
static bool current_dynamic(Tile *t)
{
    switch(t->c->current_type){
    case 2:return spike(t,false);
    case 10:return potion(t);
    case 22:return background(t,t->c->current_spec==1?179:153,t->c->x_byte,t->c->floor_y,2);
    case 18:return false;
    default:return true;
    }
}
static bool gate_foreground(Tile *t)
{
    const PopTileContext *c=t->c;
    if(c->previous_type!=4||c->source_row!=c->player_row||
       (uint8_t)(c->column-1u)!=c->player_column||c->right_room==c->player_room)return true;
    gate_geometry(t);t->p.mode=1;t->p.image=68;t->p.y=(uint8_t)(t->gate_y-2u);
    if(!append_foreground(t))return false;
    t->p.image=55;t->p.y=(uint8_t)(t->gate_y-12u);
    for(unsigned n=0;n<64;++n){
        uint8_t y=t->p.y;if(y>=192||y<7||(uint8_t)(y-7u)<t->gate_top)return true;
        if(!append_foreground(t))return false;
        uint8_t next=(uint8_t)(y-8u);if(next==0)return true;t->p.y=next;
    }
    return false;
}
static bool foreground_piece(Tile *t)
{
    uint8_t type=t->c->current_type,image;
    if(!gate_foreground(t))return false;
    if(type==18)return false;
    uint8_t kind=t->c->current_spec&224u;
    image=(type==10&&kind>=64&&kind!=160)?149:tile_foreground[type];
    if(!image)return true;
    t->p.image=image;t->p.y=(uint8_t)(t->c->floor_y+tile_foreground_y[type]);
    t->p.x=(uint8_t)(t->c->x_byte+tile_foreground_x[type]);
    if(type>=27||type==3||type==20){
        if(type==20)t->p.image=tile_wall_foreground[t->c->current_spec<2?t->c->current_spec:0];
        t->p.mode=2;return append_foreground(t);
    }
    return foreground_pair(t);
}
bool pop_tile_produce(const PopTileContext *context,PopTileOutput *out)
{
    PopTileOutput result={0};
    if(!context||!out||(context->entry!=0x161e&&context->entry!=0x1639)||context->palette!=0||
       context->current_type>=30||context->previous_type>=30||context->below_type>=30||
       context->cell>=30||context->column>=10||context->room<1||context->room>24||
       context->background_total>199||context->foreground_total>99||context->middle_total>45)return false;
    result.background_total=context->background_total;result.foreground_total=context->foreground_total;
    result.middle_total=context->middle_total;
    Tile t={context,&result,context->parameters,0,0};
    if(!empty_support(&t)||!below_gate(&t)||!previous_face(&t))return false;
    if(context->entry==0x161e&&!previous_dynamic(&t))return false;
    if(!bottom_piece(&t)||!loose_bottom(&t))return false;
    if(context->entry==0x161e&&(!current_face(&t)||!current_dynamic(&t)))return false;
    if(!foreground_piece(&t))return false;
    result.parameters=t.p;*out=result;return true;
}
