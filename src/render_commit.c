#include "render_commit.h"
#include "player_render.h"
#include "falling_draw.h"
#include "data.h"
#include <string.h>

static bool dirty_cell(uint8_t column, uint8_t row, uint8_t *cell, bool *normal)
{
    if ((row & 128u) != 0) { *cell = column; *normal = false; return true; }
    if (row >= 3 || column >= 10) { *cell = 30; *normal = false; return true; }
    uint8_t base;
    if (!pop_data_read(NULL,(uint16_t)(0x1200u+row),&base)) return false;
    *cell = (uint8_t)(base+column); *normal = true; return true;
}

static bool mark(PopRuntime *r, uint8_t column, uint8_t row, uint8_t count,
                  unsigned kind)
{
    uint8_t cell;bool normal;
    if (!dirty_cell(column,row,&cell,&normal)) return false;
    if (!normal) {
        if (kind != 0 && cell < 10) r->effect_memory.top[cell] = count;
    } else if (kind == 0) r->mechanisms.foreground[cell] = count;
    else if (kind == 1) r->mechanisms.floor[cell] = count;
    else r->mechanisms.half_floor[cell] = count;
    return true;
}

static bool actor_marks(PopRuntime *r, const PopControllerState *state,
                         bool guard, uint16_t *helper)
{
    if (!guard && pop_player_render_delay_cycles(state)!=0) return true;
    PopControllerState p;
    if (!(guard ? pop_guard_render_project(&r->world,state,&p,helper) :
                  pop_player_render_project(&r->world,state,&p,helper))) return false;
    const uint8_t *a = p.aux;
    uint8_t row = a[0xbd];
    for (unsigned rows=0;rows<256;++rows) {
        uint8_t col = a[0xbb];
        for (unsigned columns=0;columns<256;++columns) {
            if (!mark(r,col,row,3,0)) return false;
            if (col == a[0xba]) break;
            --col;
            if ((col & 128u) != 0) break;
        }
        if (row == a[0xbc]) break;
        --row;
        if ((row & 128u) != 0) break;
    }
    const bool half = a[0x40] >= 135 && a[0x40] < 149;
    bool floor = half || a[0x46] == 2 || a[0x46] == 3 || a[0x46] == 4 || a[0x46] == 6;
    if (a[0x46] == 1 && a[0x40] >= 78 && a[0x40] < 80) floor = true;
    if (floor) {
        uint8_t col = a[0xbb];
        for (unsigned columns=0;columns<256;++columns) {
            if (!mark(r,col,a[0xbd],2,half?2:1)) return false;
            if (a[0xbc] != a[0xbd] && !mark(r,col,a[0xbc],2,half?2:1)) return false;
            if (col == a[0xba]) break;
            --col;
            if ((col & 128u) != 0) break;
        }
    }
    return true;
}

static void health_dirty_cells(PopRuntime *r,unsigned first,unsigned count)
{
    for (unsigned cell=first;cell<first+count;++cell) {
        r->effect_memory.full_tile[cell]=2;
        r->effect_memory.wipe_repeat[cell]=2;
        r->effect_memory.wipe_height[cell]=4;
    }
}

static void health_dirty(PopRuntime *r)
{
    /* OriginalF90C/F9FD: three left footer cells and two right cells. */
    health_dirty_cells(r,20,3);
    health_dirty_cells(r,28,2);
}

static uint8_t footer_dirty(const PopRuntime *r,unsigned first,unsigned count)
{
    uint8_t value=0;
    for (unsigned i=first;i<first+count;++i)
        value |= r->effect_memory.full_tile[i] | r->mechanisms.floor[i] |
            r->mechanisms.half_floor[i] | r->mechanisms.foreground[i] |
            r->effect_memory.wipe_repeat[i];
    return value;
}

/* Persistent output of source18AE->1CB7, separate from its draw parameters.
 * 1486 visits body rows2..0 and columns0..9. Incremental1544/164B invokes
 * 18AE only for a full-tile or movable redraw. The above-room1639 strip
 * does not call18AE. Type16 is unaffected by1EDE's plate normalization. */
static bool exit_clip(PopRuntime *r,PopControllerState *state,bool full)
{
    const unsigned room=state->aux[0xcb];uint8_t left;
    if (!pop_world_initial_neighbor(room,POP_WORLD_LEFT,&left)) return false;
    for (unsigned row=3;row>0;) {
        --row;uint8_t bottom;
        if (!pop_data_read(NULL,(uint16_t)(0x127au+row),&bottom)) return false;
        const uint8_t floor=(uint8_t)(bottom-3u);
        for (unsigned col=0;col<9;++col) {
            const unsigned cell=row*10u+col;
            if (!full && r->effect_memory.full_tile[cell]==0 &&
                r->mechanisms.movable[cell]==0) continue;
            unsigned previous;
            if (col==0) {
                if (left==0) continue; /*1735 supplies type20 for no left room.*/
                previous=(left-1u)*30u+row*10u+9u;
            } else previous=(room-1u)*30u+cell-1u;
            if ((r->world.types[previous]&31u)!=16 ||
                (uint8_t)(bottom-67u)>=192) continue;
            /* Two consecutive SBCs retain the first subtraction's borrow. */
            state->aux[0x215]=(uint8_t)(floor-14u-
                (r->world.specs[previous]>>2)-(floor<14u?1u:0u));
        }
    }
    return true;
}

static bool commit(PopRuntime *r,PopControllerState *state,uint16_t helper_id,uint16_t *helper)
{
    uint8_t *s=state->aux;
    const bool reset=helper_id==0x2439 || (helper_id==0x22dd && s[0xa3]!=0);
    if (helper_id!=0x2439 && helper_id!=0x22dd) { *helper=helper_id;return false; }
    if (s[0xcb]==0 || s[0xcb]>24) { *helper=0x2482;return false; }
    if (reset) {
        s[0xa3]=0;s[0x28]=2;s[0x31d]=160;s[0]=0;
        memset(r->effect_memory.full_tile,0,sizeof(r->effect_memory.full_tile));
        memset(r->effect_memory.wipe_repeat,0,sizeof(r->effect_memory.wipe_repeat));
        memset(r->effect_memory.top,0,sizeof(r->effect_memory.top));
        memset(r->mechanisms.half_floor,0,sizeof(r->mechanisms.half_floor));
        memset(r->mechanisms.foreground,0,sizeof(r->mechanisms.foreground));
        memset(r->mechanisms.floor,0,sizeof(r->mechanisms.floor));
        memset(r->mechanisms.movable,0,sizeof(r->mechanisms.movable));
        memset(r->mechanisms.object,0,sizeof(r->mechanisms.object));
        health_dirty(r);
    }
    s[0x23]=s[0xcb];
    /*2439's2453/246F full-room producer runs before2482's actor marks.*/
    if (reset && !exit_clip(r,state,true)) return false;
    if (s[0x97]!=0) { --s[0x97];health_dirty(r); }
    if (s[0x30d]>15) { *helper=0xee12;return false; }
    uint8_t below;
    if (!pop_world_initial_neighbor(s[0xcb],POP_WORLD_BELOW,&below)) return false;
    for (unsigned slot=s[0x30d];slot>0;--slot) {
        uint8_t record[6];
        /* Original EE12/F773 calls EEBB before testing the record's state:
         * all six scratch-slot fields are loaded even for hidden records.
         * These source writes persist after rendering; active slots remain
         * unchanged and the next EE0F reloads scratch before physics reads. */
        for (unsigned field=0;field<6;++field) {
            record[field]=r->mechanisms.falling[field][slot];
            r->mechanisms.falling[field][0]=record[field];
        }
        PopFallingObject object;
        if (!pop_falling_draw(record,s[0xcb],below,&object)) return false;
        if (!object.visible) continue;
        /* F7AC wraps only the source Y scratch when the accepted slab is
         * drawn from the room below. Its record keeps the unwrapped Y. */
        r->mechanisms.falling[1][0]=object.bottom_y;
        uint8_t row=3,threshold;
        while (row<4) {
            if (!pop_data_read(NULL,(uint16_t)(0x1275u+row),&threshold)) return false;
            if (object.bottom_y>=threshold) break;
            --row;
        }
        const uint8_t column=(uint8_t)((record[0]>>2)+1u);
        if (!mark(r,column,row,2,1) || !mark(r,column,row,2,0)) return false;
        const uint8_t upper_y=(uint8_t)(object.bottom_y-17u);uint8_t upper=3;
        while (upper<4) {
            if (!pop_data_read(NULL,(uint16_t)(0x1275u+upper),&threshold)) return false;
            if (upper_y>=threshold) break;
            --upper;
        }
        if (upper!=row && (!mark(r,column,upper,2,1) || !mark(r,column,upper,2,0))) return false;
    }
    if (s[0x6b]==s[0xcb] && s[0x63]!=86 && !actor_marks(r,state,true,helper)) return false;
    if (s[0x5b]!=0 && s[0x5b]==s[0xcb] && !actor_marks(r,state,false,helper)) return false;
    /*2397's25E2 runs after both actor producers. Any nonzero health delta
     * requests its own footer: F91E/FA03 marks20..22;F921/FA07 marks28..29. */
    if (s[0xcf]!=0) health_dirty_cells(r,20,3);
    if (s[0xd2]!=0) health_dirty_cells(r,28,2);
    s[0x31b]=footer_dirty(r,20,3);s[0x31e]=footer_dirty(r,28,2);
    if (!exit_clip(r,state,false)) return false;
    for (unsigned i=0;i<30;++i) {
        if (r->effect_memory.wipe_repeat[i]) --r->effect_memory.wipe_repeat[i];
        if (r->effect_memory.full_tile[i]) --r->effect_memory.full_tile[i];
        else if (r->mechanisms.movable[i]) --r->mechanisms.movable[i];
        if (r->mechanisms.floor[i]) --r->mechanisms.floor[i];
        else if (r->mechanisms.half_floor[i]) --r->mechanisms.half_floor[i];
        r->mechanisms.object[i]=0;
        if (r->mechanisms.foreground[i]) --r->mechanisms.foreground[i];
    }
    uint8_t above;
    if (!pop_world_initial_neighbor(s[0xcb],POP_WORLD_ABOVE,&above)) return false;
    if (above) for(unsigned i=0;i<10;++i) if(r->effect_memory.top[i]) --r->effect_memory.top[i];
    if (s[0xdb]!=0) {
        --s[0xdb];
        if ((s[0x5f]&128u)==0 && s[0xdb]>=15 && s[0xdb]<95 && (s[0xdb]&7u)==2) {
            PopControllerQueryResult q={16,0,0,false,false,false};
            if (!pop_runtime_query(r,0xd909,16,state,&q)) { *helper=0xd909;return false; }
        }
    }
    s[0]=s[0]==0?32:0;
    return true;
}

bool pop_render_commit(PopRuntime *runtime, PopControllerState *state,
                        uint16_t helper_id, uint16_t *required_helper)
{
    if (required_helper!=NULL) *required_helper=0;
    if (runtime==NULL || state==NULL) return false;
    PopRuntime staged=*runtime;PopControllerState work=*state;uint16_t helper=0;
    if (!commit(&staged,&work,helper_id,&helper)) {
        if(required_helper!=NULL) *required_helper=helper;
        return false;
    }
    *runtime=staged;*state=work;return true;
}
