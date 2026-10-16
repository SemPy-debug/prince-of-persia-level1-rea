#include "world_mechanisms.h"
#include "data.h"
#include <stddef.h>
#include <string.h>

bool pop_world_mechanism_memory_reset(PopWorldMechanismMemory *memory) {
    if (memory == NULL) return false;
    memset(memory, 0, sizeof(*memory));
    return true;
}

static uint8_t *mechanism_address(PopWorldMechanismMemory *m, uint16_t a) {
    if (m == NULL) return NULL;
    if (a >= 0xb660 && a < 0xb6c0) return (uint8_t *)&m->falling + (a - 0xb660u);
    if (a >= 0x5e3c && a < 0x5e5a) return m->half_floor + (a - 0x5e3cu);
    if (a >= 0x5e78 && a < 0x5e96) return m->foreground + (a - 0x5e78u);
    if (a >= 0x5e96 && a < 0x5eb4) return m->floor + (a - 0x5e96u);
    if (a >= 0x5ed2 && a < 0x5ef0) return m->movable + (a - 0x5ed2u);
    if (a >= 0x5ef0 && a < 0x5f0e) return m->object + (a - 0x5ef0u);
    return NULL;
}

bool pop_world_mechanism_memory_read(void *memory, uint16_t address, uint8_t *value) {
    uint8_t *source = mechanism_address(memory, address);
    if (source == NULL || value == NULL) return false;
    *value = *source;
    return true;
}

bool pop_world_mechanism_memory_write(void *memory, uint16_t address, uint8_t value) {
    uint8_t *destination = mechanism_address(memory, address);
    if (destination == NULL) return false;
    *destination = value;
    return true;
}

typedef struct Mechanism {
    PopWorldState *world;
    PopWorldEffects *effects;
    PopWorldEffectMemory *effect_memory;
    PopWorldMechanismMemory *memory;
    PopControllerState *state;
    PopControllerQueryResult *result;
    const PopControllerIO *io;
    PopWorldEffectWrite write;
} Mechanism;

static void value(Mechanism *m, uint8_t a) {
    m->result->a = a; m->result->zero = a == 0; m->result->negative = (a & 128u) != 0;
}
static void compare(Mechanism *m, uint8_t a, uint8_t b) {
    m->result->carry = a >= b; m->result->zero = a == b;
    m->result->negative = ((uint8_t)(a - b) & 128u) != 0;
}
static bool call(Mechanism *m, uint16_t helper, uint8_t a) {
    value(m, a);
    return m->io != NULL && m->io->query != NULL &&
           m->io->query(m->io->context, helper, a, m->state, m->result);
}
static uint16_t pointer(const uint8_t *s, unsigned a) {
    return (uint16_t)(s[a] + 256u * s[a + 1]);
}
static void load_y(Mechanism *m, uint8_t y) {
    m->result->y = y; m->result->zero = y == 0; m->result->negative = (y & 128u) != 0;
}
static bool room_pointers(Mechanism *m, uint8_t room) {
    uint8_t low, high; unsigned type, spec; uint8_t *s = m->state->aux;
    if (room > 24) return false;
    if (room == 0) {
        s[0x24] = 15; s[0x25] = 150; s[0x26] = 45; s[0x27] = 150;
        value(m, 150); m->result->carry = true; return true;
    }
    m->result->x = (uint8_t)((room - 1u) * 2u);
    if (!pop_data_read(NULL, (uint16_t)(0x1220u + m->result->x), &low) ||
        !pop_data_read(NULL, (uint16_t)(0x1221u + m->result->x), &high)) return false;
    type = 0xb700u + low + 256u * high; spec = type + 720u;
    s[0x24] = (uint8_t)type; s[0x25] = (uint8_t)(type >> 8);
    s[0x26] = (uint8_t)spec; s[0x27] = (uint8_t)(spec >> 8);
    value(m, (uint8_t)(spec >> 8)); m->result->carry = false; return true;
}
static void remove_record(Mechanism *m) { value(m, 255); m->effects->phase[0] = 255; }

/* F45D/F49A/F4D1 select normal or top-buffer dirty positions. Numeric
 * neighbor fields retain their verified original source identities. */
static void dirty_position(Mechanism *m, unsigned kind) {
    PopControllerQueryResult *r = m->result; uint8_t *s = m->state->aux;
    uint8_t cell = m->effects->cell[0], room = m->effects->room[0];
    value(m, room); compare(m, room, s[0xcb]);
    if (kind == 2) {
        if (r->zero) {
            load_y(m, cell); compare(m, cell, 10);
            if (!r->carry) { load_y(m, (uint8_t)(cell + 1u)); r->carry = true; return; }
            compare(m, cell, 19); if (!r->zero) compare(m, cell, 29);
            if (!r->zero) { value(m, (uint8_t)(cell - 9u)); load_y(m, r->a); r->carry = false; return; }
        } else {
            compare(m, room, s[0x31]);
            if (r->zero) {
                load_y(m, cell); compare(m, cell, 9);
                if (r->zero) { load_y(m, 0); r->carry = true; return; }
                compare(m, cell, 19); if (!r->zero) compare(m, cell, 29);
                if (r->zero) { value(m, (uint8_t)(cell - 19u)); load_y(m, r->a); r->carry = false; return; }
            } else {
                compare(m, room, s[0x34]);
                if (r->zero) {
                    load_y(m, cell); compare(m, cell, 9);
                    if (!r->carry) { value(m, (uint8_t)(cell + 21u)); load_y(m, r->a); r->carry = false; return; }
                } else {
                    compare(m, room, s[0x35]);
                    if (!r->zero) return; /* F4D0 is a bareRTS; retain Y/C. */
                    load_y(m, cell); compare(m, cell, 9);
                    if (r->zero) { load_y(m, 20); r->carry = false; return; }
                }
            }
        }
    } else if (r->zero) {
        load_y(m, cell);
        if (kind == 0) { compare(m, cell, 30); return; }
        compare(m, cell, 9); if (!r->zero) compare(m, cell, 19); if (!r->zero) compare(m, cell, 29);
        if (!r->zero) { load_y(m, (uint8_t)(cell + 1u)); r->carry = false; return; }
    } else {
        if (kind == 1) {
            compare(m, room, s[0x31]);
            if (r->zero) {
                load_y(m, cell); compare(m, cell, 9); if (!r->zero) compare(m, cell, 19); if (!r->zero) compare(m, cell, 29);
                if (r->zero) { value(m, (uint8_t)(cell - 9u)); load_y(m, r->a); r->carry = false; return; }
                load_y(m, 30); r->carry = true; return;
            }
        }
        compare(m, room, s[0x33]);
        if (r->zero) { value(m, (uint8_t)(cell - 20u)); load_y(m, r->a); r->carry = true; return; }
        return; /* F450 BNEF44F retains Y and comparison carry. */
    }
    load_y(m, 30); r->carry = true;
}

static bool dirty_write(Mechanism *m, uint8_t *normal) {
    PopControllerQueryResult *r = m->result;
    value(m, 2);
    if (r->carry) {
        compare(m, r->y, 10);
        if (!r->carry) { m->effect_memory->top[r->y] = r->a; r->carry = true; }
        return true;
    }
    if (r->y >= 30) return false;
    normal[r->y] = r->a; return true;
}
static bool dirty_object(Mechanism *m, unsigned kind) {
    dirty_position(m, kind); return dirty_write(m, m->memory->movable);
}
static bool dirty_gate(Mechanism *m) {
    dirty_position(m, 1);
    if (!dirty_write(m, m->memory->movable) || !dirty_write(m, m->memory->foreground)) return false;
    return dirty_object(m, 2);
}
static uint8_t random_byte(Mechanism *m) {
    unsigned sum = (uint8_t)(5u * m->state->aux[0x9e]) + 23u;
    m->result->carry = sum > 255u; value(m, (uint8_t)sum);
    m->state->aux[0x9e] = m->result->a; return m->result->a;
}
static bool sound(Mechanism *m, uint8_t request) { return call(m, 0xd909, request); }

static bool gate_sound(Mechanism *m) {
    uint8_t *s = m->state->aux, room = m->effects->room[0], cell = m->effects->cell[0];
    bool edge = cell == 9 || cell == 19 || cell == 29;
    if ((s[0xe8] & 1u) == 0) return true;
    if ((s[0x3f4] == 3 && room == 2) || (room == s[0x31] && edge) ||
        (room == s[0xcb] && !edge)) return sound(m, 12);
    return true;
}

/* Original E2 EE39..EE44 data, separate from the AUX data address space.
 * Only the listed gate indices are admitted; no instruction bytes are copied. */
static const uint8_t close_step[9] = {0,0,0,20,40,60,80,100,120};
static const uint8_t open_step[3] = {255,4,4};

static bool gate(Mechanism *m, bool exit_gate) {
    uint8_t *s = m->state->aux, phase = m->effects->phase[0], old = s[0xe8];
    PopControllerQueryResult *r = m->result; unsigned sum;
    r->x = phase;
    if (phase & 128u) return exit_gate ? dirty_object(m,1) : dirty_gate(m);
    compare(m, phase, 3);
    if (!r->carry) {
        if (exit_gate) {
            if (!sound(m, 10)) return false;
            sum = old + 4u; r->carry = sum > 255u; value(m, (uint8_t)sum); s[0xe8] = r->a;
            compare(m, r->a, 172);
            if (r->carry) {
                remove_record(m);
                if (!sound(m,2)) return false;
                r->x = 15;
                if (!call(m,0xd930,8)) return false;
                s[0x86] = 1;
                /* E045/E94D is inert for first-level03F4=1. */
                if (s[0x3f4] == 4) return false;
            }
            return dirty_object(m,1);
        }
        compare(m, old, 255);
        if (r->zero) {
            remove_record(m); if (!sound(m,2)) return false; return dirty_gate(m);
        }
        sum = old + open_step[phase]; r->carry = sum > 255u; value(m,(uint8_t)sum); s[0xe8] = r->a;
        compare(m,phase,0);
        if (r->zero) {
            compare(m,r->a,0);
            if (r->zero) { remove_record(m); if (!sound(m,2)) return false; return dirty_gate(m); }
            compare(m,r->a,188);
            if (!r->carry && !gate_sound(m)) return false;
            return dirty_gate(m);
        }
        compare(m,r->a,188);
        if (r->carry) {
            compare(m,phase,2);
            if (!r->carry) { s[0xe8]=238;value(m,0);m->effects->phase[0]=0;return true; }
            s[0xe8]=255;remove_record(m);if(!sound(m,2))return false;return dirty_gate(m);
        }
        if (!sound(m,11)) return false;
        return dirty_gate(m);
    }
    compare(m,phase,8);
    if (!r->carry) { ++phase; m->effects->phase[0]=phase; r->x=phase; }
    if (phase >= 9) return false;
    r->carry = old >= close_step[phase]; value(m,(uint8_t)(old-close_step[phase]));s[0xe8]=r->a;
    if (!r->zero && !r->carry) {
        if (exit_gate) {remove_record(m);value(m,0);s[0xe8]=0;}
        else {value(m,0);s[0xe8]=0;remove_record(m);}
        if (!sound(m,15)) return false;
    }
    return exit_gate ? dirty_object(m,1) : dirty_gate(m);
}

static bool append_secondary(Mechanism *m) {
    PopControllerQueryResult *r=m->result;uint8_t *s=m->state->aux;
    r->x=s[0x30d];
    if(r->x>15)return false;
    compare(m,r->x,15);if(r->zero)return true;
    ++r->x;s[0x30d]=r->x;
    for(unsigned i=0;i<6;++i){value(m,m->memory->falling[i][0]);m->memory->falling[i][r->x]=r->a;}
    return true;
}

/* F386/F66E: detach the actual cell, then publish its falling record. Full
 * secondary queues keep source-ordered world/scratch writes before refusing
 * only the append. Column/row are D07E/D2CF's repeated subtraction of ten. */
static bool spawn_secondary(Mechanism *m) {
    PopControllerQueryResult *r=m->result;uint8_t *s=m->state->aux,cell=m->effects->cell[0],row=0;
    if(!pop_world_write(m->world,(uint16_t)(pointer(s,0x24)+r->y),0))return false;
    value(m,0);r->x=s[0x303];compare(m,r->x,1);if(r->zero)value(m,1);
    s[0xe8]=r->a;remove_record(m);
    while(cell>=10){cell=(uint8_t)(cell-10u);++row;}
    r->x=row;value(m,(uint8_t)(cell*4u));r->carry=false;
    m->memory->falling[0][0]=r->a;m->memory->falling[5][0]=row;
    {uint8_t y;if(!pop_data_read(NULL,(uint16_t)(0x127au+row),&y))return false;value(m,y);m->memory->falling[1][0]=r->a;}
    value(m,m->effects->room[0]);m->memory->falling[2][0]=r->a;
    value(m,0);m->memory->falling[3][0]=0;m->memory->falling[4][0]=0;
    return append_secondary(m);
}

static bool record_handler(Mechanism *m, uint8_t type) {
    uint8_t *s=m->state->aux, phase=m->effects->phase[0], old=s[0xe8];
    PopControllerQueryResult *r=m->result;
    r->x=phase;
    switch(type) {
    case 19: /* F31B/F331 */
        if(phase&128u)return true;
        value(m,m->effects->room[0]);compare(m,r->a,s[0xcb]);
        if(!r->zero){remove_record(m);return true;}
        {uint8_t next=random_byte(m);compare(m,next,old);
         if(r->zero || next>=18){value(m,(uint8_t)(old+1u));compare(m,r->a,18);if(r->carry)value(m,0);}
         else compare(m,next,18);
         s[0xe8]=r->a;}
        return dirty_object(m,1);
    case 15: case 6: /* F264 */
        if(phase&128u)return true;
        r->x=old;value(m,m->effect_memory->link_map[old]&31u);
        r->carry=r->a>=1;value(m,(uint8_t)(r->a-1u));
        {uint8_t next=r->a;s[0xe9]=next&31u;m->effect_memory->link_map[old]=(m->effect_memory->link_map[old]&224u)|s[0xe9];value(m,next);}
        compare(m,r->a,2);
        if(r->carry)return true;
        if(!sound(m,1))return false;
        remove_record(m);return call(m,0xf445,r->a);
    case 2: /* F354 */
        if(phase&128u)return call(m,0xf429,r->a);
        value(m,old);
        if(old&128u){--s[0xe8];value(m,s[0xe8]&127u);if(!r->zero)return true;s[0xe8]=6;}
        else {++s[0xe8];compare(m,old,5);if(r->zero)s[0xe8]=143;
              else {compare(m,old,9);if(r->zero){s[0xe8]=0;remove_record(m);}}}
        return call(m,0xf429,r->a);
    case 11: /* F386 */
        if((phase&128u)==0){++s[0xe8];value(m,s[0xe8]);
            if((r->a&128u)!=0){r->x=s[0x3f4];compare(m,r->x,13);if(r->zero)return true;compare(m,r->a,132);if(r->carry){s[0xe8]=0;remove_record(m);}}
            else {compare(m,r->a,10);if(r->carry&&!spawn_secondary(m))return false;}}
        return call(m,0xf40c,r->a);
    case 0:remove_record(m);return call(m,0xf40c,r->a);
    case 4:return gate(m,false);
    case 16:return gate(m,true);
    case 10: /* F2DF/F34A */
        if(phase&128u)return true;
        value(m,m->effects->room[0]);compare(m,r->a,s[0xcb]);if(!r->zero){remove_record(m);return true;}
        s[0xe9]=old&224u;value(m,(uint8_t)((old&31u)+1u));compare(m,r->a,9);if(r->carry)value(m,1);
        value(m,r->a|s[0xe9]);s[0xe8]=r->a;return dirty_object(m,0);
    case 22: /* F302 */
        value(m,m->effects->room[0]);compare(m,r->a,s[0xcb]);if(!r->zero){remove_record(m);return true;}
        --s[0xe8];if(s[0xe8]==0){value(m,random_byte(m)&63u);m->result->carry=false;value(m,(uint8_t)(r->a+40u));s[0xe8]=r->a;}
        return dirty_object(m,0);
    case 18:return false; /* Not present in the accepted first-level blueprint. */
    default:remove_record(m);return true;
    }
}

static bool consume_record(Mechanism *m,uint8_t index) {
    uint8_t raw;PopControllerQueryResult *r=m->result;PopWorldEffects *e=m->effects;
    e->cell[0]=e->cell[index];e->room[0]=e->room[index];e->phase[0]=e->phase[index];
    if(!room_pointers(m,e->room[0]))return false;
    load_y(m,e->cell[0]);
    if(!pop_world_read(m->world,(uint16_t)(pointer(m->state->aux,0x26)+r->y),&m->state->aux[0xe8]) ||
       !pop_world_read(m->world,(uint16_t)(pointer(m->state->aux,0x24)+r->y),&raw))return false;
    value(m,raw&31u);
    {static const uint8_t types[]={19,15,6,2,11,0,18,4,16,10,22};
     for(unsigned i=0;i<sizeof(types);++i){compare(m,r->a,types[i]);if(r->zero)break;}}
    if(!record_handler(m,r->a))return false;
    value(m,m->state->aux[0xe8]);load_y(m,e->cell[0]);
    return pop_world_write(m->world,(uint16_t)(pointer(m->state->aux,0x26)+r->y),r->a);
}

static bool consume_main(Mechanism *m) {
    PopControllerQueryResult *r=m->result;PopWorldEffects *e=m->effects;uint8_t *s=m->state->aux;
    bool removed=false;uint8_t count=s[0x95];
    if(count>31 || m->io==NULL || m->write==NULL)return false;
    value(m,0);if(!m->write(m->io->context,0xb6e0,0))return false;
    r->x=count;r->zero=count==0;r->negative=(count&128u)!=0;if(count==0)return true;
    for(uint8_t i=count;i!=0;--i){
        s[0x96]=i;
        if(!consume_record(m,i))return false;
        r->x=s[0x96];value(m,e->phase[0]);
        if(r->negative){value(m,255);removed=true;}
        e->phase[r->x]=r->a;--r->x;r->zero=r->x==0;r->negative=(r->x&128u)!=0;
    }
    value(m,removed?255:0);if(!removed)return true;
    r->x=1;load_y(m,0);
    do{
        value(m,e->phase[r->x]);compare(m,r->a,255);
        if(!r->zero){++r->y;e->phase[r->y]=r->a;value(m,e->cell[r->x]);e->cell[r->y]=r->a;value(m,e->room[r->x]);e->room[r->y]=r->a;}
        ++r->x;compare(m,r->x,s[0x95]);
    }while(!r->carry||r->zero);
    s[0x95]=r->y;return true;
}

static void secondary_transfer(Mechanism *m,uint8_t index,bool save) {
    for(unsigned i=0;i<6;++i){
        value(m,m->memory->falling[i][save?0:index]);
        m->memory->falling[i][save?index:0]=m->result->a;
    }
}
static bool secondary_row(Mechanism *m) {
    uint8_t (*q)[16]=m->memory->falling;PopControllerQueryResult *r=m->result;
    ++q[5][0];value(m,q[5][0]);compare(m,r->a,3);if(!r->carry)return true;
    r->carry=q[1][0]>=192;value(m,(uint8_t)(q[1][0]-192u));q[1][0]=r->a;
    value(m,0);q[5][0]=0;value(m,q[2][0]);
    if(r->zero)return true; /* D16B's zero guard preserves incoming carry. */
    {uint8_t room=q[2][0],below;if(!pop_world_initial_neighbor(room,POP_WORLD_BELOW,&below))return false;
     r->x=(uint8_t)(room*4u);r->carry=(room&64u)!=0;value(m,below);q[2][0]=r->a;}
    return true;
}

static bool dirty_prepare_cell(Mechanism *m) {
    uint8_t *s=m->state->aux;PopControllerQueryResult *r=m->result;
    load_y(m,s[0x92]);
    if(r->negative){load_y(m,s[0x91]);r->carry=true;return true;}
    compare(m,r->y,3);
    if(!r->carry){value(m,s[0x91]);compare(m,r->a,10);
        if(!r->carry){uint8_t base;if(!pop_data_read(NULL,(uint16_t)(0x1200u+r->y),&base))return false;
            value(m,(uint8_t)(r->a+base));load_y(m,r->a);r->carry=false;return true;}}
    load_y(m,30);r->carry=true;return true;
}
static bool dirty_wipe(Mechanism *m) {
    PopControllerQueryResult *r=m->result;PopWorldEffectMemory *e=m->effect_memory;
    if(r->carry)return true;
    if(r->y>=30)return false;
    if(e->wipe_repeat[r->y]==0||m->state->aux[4]>=e->wipe_height[r->y])e->wipe_height[r->y]=m->state->aux[4];
    e->wipe_repeat[r->y]=r->a;value(m,r->a);r->carry=false;return true;
}
static bool redraw_secondary(Mechanism *m) {
    value(m,m->memory->falling[2][0]);compare(m,m->result->a,m->state->aux[0xcb]);
    if(!m->result->zero)return true;
    value(m,31);m->state->aux[4]=31;
    if(!dirty_prepare_cell(m)||!dirty_write(m,m->effect_memory->full_tile)||!dirty_wipe(m))return false;
    ++m->state->aux[0x91];
    return dirty_prepare_cell(m)&&dirty_write(m,m->effect_memory->full_tile)&&
           dirty_write(m,m->memory->foreground)&&dirty_wipe(m);
}

static bool secondary_cell(Mechanism *m) {
    uint8_t (*q)[16]=m->memory->falling,*s=m->state->aux;
    value(m,q[5][0]);s[0x92]=m->result->a;
    value(m,q[0][0]>>2);m->result->carry=(q[0][0]&2u)!=0;s[0x91]=m->result->a;
    value(m,q[2][0]);s[0x93]=m->result->a;
    return call(m,0xd024,m->result->a);
}
static bool secondary_impact(Mechanism *m) {
    uint8_t *s=m->state->aux;PopControllerQueryResult *r=m->result;
    if(!secondary_cell(m))return false;
    compare(m,r->a,6);
    if(!r->zero){compare(m,r->a,15);
        if(r->zero){value(m,14);if(!pop_world_write(m->world,(uint16_t)(pointer(s,0x24)+r->y),r->a))return false;}
        else {bool hit=false;for(unsigned i=0;i<4;++i){static const uint8_t types[]={1,2,10,19};compare(m,r->a,types[i]);if(r->zero){hit=true;break;}}
              if(!hit)return true;
              value(m,14);if(!pop_world_write(m->world,(uint16_t)(pointer(s,0x24)+r->y),r->a))return false;return redraw_secondary(m);}}
    if(!call(m,0xee06,r->a)||!call(m,0xd024,r->a))return false;
    value(m,14);if(!pop_world_write(m->world,(uint16_t)(pointer(s,0x24)+r->y),r->a))return false;
    return redraw_secondary(m);
}

static bool secondary_chain(Mechanism *m) {
    uint8_t (*q)[16]=m->memory->falling,*s=m->state->aux;PopControllerQueryResult *r=m->result;
    if(!pop_world_write(m->world,(uint16_t)(pointer(s,0x24)+r->y),0))return false;
    value(m,0);r->x=s[0x303];compare(m,r->x,1);if(r->zero)value(m,1);
    if(!pop_world_write(m->world,(uint16_t)(pointer(s,0x26)+r->y),r->a))return false;
    value(m,q[3][0]>>1);r->carry=(q[3][0]&1u)!=0;q[3][0]=r->a;
    r->x=s[0x96];secondary_transfer(m,r->x,true);
    {unsigned sum=q[1][0]+6u;r->carry=sum>255;value(m,(uint8_t)sum);q[1][0]=r->a;}
    if(!secondary_row(m)||!append_secondary(m))return false;
    r->x=s[0x96];secondary_transfer(m,r->x,false);
    return redraw_secondary(m);
}

static bool integrate_secondary(Mechanism *m) {
    uint8_t (*q)[16]=m->memory->falling,*s=m->state->aux;PopControllerQueryResult *r=m->result;
    value(m,q[3][0]);if(r->negative)return true;
    compare(m,r->a,29);
    if(!r->carry){value(m,(uint8_t)(r->a+3u));q[3][0]=r->a;}
    {unsigned sum=r->a+q[1][0];r->carry=sum>255;value(m,(uint8_t)sum);q[1][0]=r->a;}
    r->x=q[2][0];
    if(r->x==0){compare(m,r->a,209);if(r->carry){value(m,254);q[3][0]=r->a;}return true;}
    compare(m,r->a,226);if(r->carry)return true;
    r->x=q[5][0];if(r->x>=3)return false;
    {uint8_t height;if(!pop_data_read(NULL,(uint16_t)(0x1284u+r->x),&height))return false;compare(m,r->a,height);if(!r->carry)return true;}
    if(!secondary_cell(m))return false;
    s[0xee]=r->a;compare(m,r->a,0);
    if(r->zero)return secondary_row(m);
    compare(m,r->a,11);
    if(r->zero){if(!secondary_chain(m))return false;return secondary_row(m);}
    if(!sound(m,7))return false;
    value(m,q[2][0]);s[0x93]=r->a;value(m,q[5][0]);s[0x92]=r->a;
    if(!call(m,0xf833,r->a))return false;
    r->x=q[5][0];
    {uint8_t height;if(r->x>=3||!pop_data_read(NULL,(uint16_t)(0x1284u+r->x),&height))return false;value(m,height);q[1][0]=r->a;}
    value(m,254);q[3][0]=r->a;
    return secondary_impact(m);
}

static bool select_sequence(Mechanism *m,uint8_t id) {
    PopControllerQueryResult *r=m->result;uint8_t *s=m->state->aux;
    unsigned index=2u*(uint8_t)(id-1u);r->x=(uint8_t)index;r->carry=index>255;
    if(!pop_data_read(NULL,(uint16_t)(0x3000u+r->x),&s[0x49])||
       !pop_data_read(NULL,(uint16_t)(0x3001u+r->x),&s[0x4a]))return false;
    value(m,s[0x4a]);m->state->selected_sequence=id;return true;
}
static void actor_transfer(Mechanism *m,bool save) {
    uint8_t *s=m->state->aux;
    if(save)memcpy(s+0x50,s+0x40,16);else memcpy(s+0x40,s+0x50,16);
    m->result->a=s[0x40];m->result->x=255;m->result->zero=false;m->result->negative=true;
}
static bool falling_player_contact(Mechanism *m) {
    uint8_t (*q)[16]=m->memory->falling,*s=m->state->aux;PopControllerQueryResult *r=m->result;
    actor_transfer(m,false);
    value(m,q[2][0]);compare(m,r->a,s[0x4b]);
    if(!r->zero){r->carry=false;return true;}
    value(m,q[0][0]>>2);r->carry=(q[0][0]&2u)!=0;compare(m,r->a,s[0x44]);
    if(!r->zero){r->carry=false;return true;}
    value(m,q[1][0]);compare(m,r->a,s[0x42]);
    if(r->carry){r->carry=false;return true;}
    r->carry=s[0x42]>=30;value(m,(uint8_t)(s[0x42]-30u));compare(m,r->a,q[1][0]);
    if(r->carry){r->carry=false;return true;}
    value(m,s[0x3f4]);compare(m,r->a,13);
    if(!r->zero){value(m,s[0x40]);compare(m,r->a,5);if(r->carry){compare(m,r->a,15);if(!r->carry){actor_transfer(m,true);return true;}}}
    value(m,s[0x46]);compare(m,r->a,2);
    if(r->carry){compare(m,r->a,7);if(!r->zero){actor_transfer(m,true);return true;}}
    r->x=(uint8_t)(s[0x45]+1u);
    {uint8_t height;if(!pop_data_read(NULL,(uint16_t)(0x127eu+r->x),&height))return false;value(m,height);s[0x42]=r->a;}
    if(!call(m,0xf927,1))return false;
    if(r->zero){if(!select_sequence(m,22))return false;}
    else {value(m,s[0x40]);compare(m,r->a,109);if(!r->zero&&!select_sequence(m,52))return false;}
    actor_transfer(m,true);return true;
}

static bool consume_secondary(Mechanism *m) {
    uint8_t (*q)[16]=m->memory->falling,*s=m->state->aux;PopControllerQueryResult *r=m->result;
    uint8_t count=s[0x30d];if(count>15)return false;
    r->x=count;r->zero=count==0;r->negative=(count&128u)!=0;if(count==0)return true;
    for(uint8_t i=count;i!=0;--i){
        r->x=i;s[0x96]=i;secondary_transfer(m,i,false);
        value(m,q[4][0]);if(r->zero&&!integrate_secondary(m))return false;
        value(m,q[3][0]);if(r->negative)++q[3][0];
        if(!falling_player_contact(m))return false;
        r->x=s[0x96];secondary_transfer(m,r->x,true);--r->x;
    }
    r->x=1;load_y(m,0);
    do{
        value(m,q[3][r->x]);compare(m,r->a,255);
        if(!r->zero){++r->y;q[3][r->y]=r->a;
            for(unsigned field=0;field<6;++field)if(field!=3){value(m,q[field][r->x]);q[field][r->y]=r->a;}}
        ++r->x;compare(m,r->x,s[0x30d]);
    }while(!r->carry||r->zero);
    s[0x30d]=r->y;return true;
}

bool pop_world_mechanism_query_io(PopWorldState *world, PopWorldEffects *effects,
    PopWorldEffectMemory *effect_memory, PopWorldMechanismMemory *mechanism_memory,
    uint16_t helper_id, uint8_t argument, PopControllerState *state,
    PopControllerQueryResult *result, const PopControllerIO *io,
    PopWorldEffectWrite write) {
    Mechanism m={world,effects,effect_memory,mechanism_memory,state,result,io,write};
    (void)argument;
    if(world==NULL||effects==NULL||effect_memory==NULL||mechanism_memory==NULL||state==NULL||result==NULL)return false;
    if(helper_id==0xee00)return consume_main(&m);
    if(helper_id==0xee0f)return consume_secondary(&m);
    return false;
}
