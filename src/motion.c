#include "motion.h"
#include "data.h"
#include <stddef.h>
void pop_motion_update_gravity(PopControllerState *s) {
    uint8_t velocity, cap;
    if (!s || s->aux[0x46] != 4) return;
    cap = s->aux[0xc5] ? 4 : 33;
    velocity = (uint8_t)(s->aux[0x48] + (s->aux[0xc5] ? 1u : 3u));
    s->aux[0x48] = velocity < cap ? velocity : cap;
}
PopControllerResult pop_motion_integrate_velocity(PopControllerState *s,const PopControllerIO *io) {
    PopControllerState work;
    PopControllerQueryResult result = {0,0,0,false,false,false};
    uint8_t displacement;
    if (!s) return POP_CONTROLLER_INVALID;
    work = *s; work.required_helper = 0;
    work.aux[0x42] = (uint8_t)(work.aux[0x42] + work.aux[0x48]);
    if (work.aux[0x46] == 4) {
        displacement = work.aux[0x47];
        if (work.aux[0x43] & 0x80u) displacement = (uint8_t)(0u-displacement);
        work.aux[0x41] = (uint8_t)(work.aux[0x41] + displacement);
        result.a = work.aux[0x41];
        if (!io || !io->query ||
            !io->query(io->context,0x3a09,result.a,&work,&result)) {
            s->required_helper = 0x3a09;
            return POP_CONTROLLER_HELPER_REQUIRED;
        }
    }
    *s = work; return POP_CONTROLLER_COMPLETE;
}
PopControllerResult pop_motion_check_opponent_contact(PopControllerState *s,const PopControllerIO *io) {
    PopControllerState work;
    PopControllerQueryResult result={0,0,0,false,false,false};
    if (!s) return POP_CONTROLLER_INVALID;
    work=*s;work.required_helper=0;work.selected_sequence=0;
    if (work.aux[0xd1] < 2 || work.aux[0x4e] != 0 || work.aux[0x3aa] == 0 ||
        work.aux[0x3a2] >= 2 || work.aux[0x43] == work.aux[0x39f]) {
        *s=work;return POP_CONTROLLER_COMPLETE;
    }
    if (!io || !io->query ||
        !io->query(io->context,0xd0ae,work.aux[0x43],&work,&result)) {
        s->required_helper=0xd0ae;return POP_CONTROLLER_HELPER_REQUIRED;
    }
    if (result.a < 241) { *s=work;return POP_CONTROLLER_COMPLETE; }
    if (!pop_data_read(NULL,(uint16_t)(0x127fu+work.aux[0x45]),&work.aux[0x42])) {
        s->required_helper=(uint16_t)(0x127fu+work.aux[0x45]);
        return POP_CONTROLLER_HELPER_REQUIRED;
    }
    pop_controller_select_sequence(&work,47);
    if (!io->query(io->context,0x450c,work.aux[0x4a],&work,&result)) {
        s->required_helper=0x450c;return POP_CONTROLLER_HELPER_REQUIRED;
    }
    *s=work;return POP_CONTROLLER_COMPLETE;
}
typedef struct Motion {
    PopControllerState *state;
    const PopControllerIO *io;
    PopControllerQueryResult result;
    PopMotionWrite write;
} Motion;
static bool m_query(Motion *m,uint16_t helper,uint8_t argument) {
    m->result.a=argument;
    if (!m->io || !m->io->query ||
        !m->io->query(m->io->context,helper,argument,m->state,&m->result)) {
        m->state->required_helper=helper;return false;
    }
    return true;
}
static bool m_next(Motion *m,uint16_t helper) { return m_query(m,helper,m->result.a); }
static bool m_read(Motion *m,uint16_t address,uint8_t *value) {
    if (pop_data_read(NULL,address,value)) return true;
    if (m->io && m->io->read && m->io->read(m->io->context,address,value)) return true;
    m->state->required_helper=address;return false;
}
static bool m_row_height(Motion *m,uint8_t *value) {
    return m_read(m,(uint16_t)(0x127eu+(uint8_t)(m->state->aux[0x45]+1u)),value);
}
static bool m_spec(Motion *m,uint8_t *value) {
    uint16_t base=(uint16_t)(m->state->aux[0x26]|((uint16_t)m->state->aux[0x27]<<8));
    return m_read(m,(uint16_t)(base+m->result.y),value);
}
static bool m_variable(Motion *m,uint16_t address,uint8_t *value) {
    if (address<sizeof m->state->aux) {*value=m->state->aux[address];return true;}
    return m_read(m,address,value);
}
static void m_relative_x(Motion *m,uint8_t displacement) {
    if (m->state->aux[0x43]&128u) displacement=(uint8_t)(0u-displacement);
    m->state->aux[0x41]=(uint8_t)(m->state->aux[0x41]+displacement);
}
static bool m_refresh(Motion *m) { return m_query(m,0x3a09,m->state->aux[0x41]); }
static bool m_animate(Motion *m,uint8_t sequence) {
    pop_controller_select_sequence(m->state,sequence);
    return m_query(m,0x450c,m->state->aux[0x4a]);
}
static bool m_sound13(Motion *m) {
    m->state->aux[0xe4]=1;return m_query(m,0xd909,13);
}
/* Original3C61: align to a neighboring loose-floor edge, then reload its tile. */
static bool loose_floor_alignment(Motion *m) {
    uint8_t displacement;
    if (!m_next(m,0xd018)) return false;
    if (m->result.a < 8) {
        if (!m_next(m,0xd0bd)) return false;
        if (m->result.a != 20) {
            if (!m_next(m,0xd018)) return false;
            displacement=(uint8_t)(m->result.a+4u);
            goto align;
        }
    }
    if (!m_next(m,0xd0c0)) return false;
    if (m->result.a == 20) {
        if (!m_next(m,0xd018)) return false;
        displacement=(uint8_t)((uint8_t)~(uint8_t)(m->result.a+14u)+8u);
    } else {
        if (!m_next(m,0xd018)) return false;
        displacement=(uint8_t)((uint8_t)~m->result.a+8u);
    }
align:
    m_relative_x(m,displacement);
    return m_refresh(m) && m_next(m,0xd0ba);
}
/* Original3B0C/3B60: falling or jumping with action held may grab a ledge. */
static bool grab_ledge(Motion *m) {
    uint8_t *s=m->state->aux,height;
    if (((s[0x3d]&s[0x4f])&128u)==0 || s[0x48]>=32) return true;
    if (!m_row_height(m,&height)) return false;
    if ((uint8_t)(s[0x42]+25u)<height) return true;
    s[0xaf]=s[0x41];m_relative_x(m,0xf8);
    if (!m_refresh(m) || !m_next(m,0xd0c3)) return false;
    s[0x8b]=m->result.a;
    if (!m_next(m,0xd0c6) || !m_next(m,0xd090)) return false;
    if (m->result.zero) { s[0x41]=s[0xaf];return m_refresh(m); }
    if (!m_next(m,0xd018)) return false;
    m_relative_x(m,m->result.a);
    if (!m_row_height(m,&height)) return false;
    s[0x42]=height;s[0x48]=0;
    if (!m_animate(m,15)) return false;
    s[0xe6]=12;return true;
}
static bool finish_landing(Motion *m,bool life_test_reached) {
    uint8_t *s=m->state->aux,sequence;
    if ((life_test_reached && s[0x4f]<128) || s[0x48]>=33 || (s[0x48]>=22 && s[0x4d]>=2)) {
        if (!m_query(m,0xf927,100) || !m_query(m,0xd909,5)) return false;
        sequence=22;
    } else if (s[0x48]>=22 && s[0x4d]==0) {
        if (!m_query(m,0xf927,1)) return false;
        sequence=m->result.zero?22:20;
        if (!m_query(m,0xd909,5)) return false;
    } else if (s[0x4d]>=2 || s[0x4e]==2) {
        s[0x4e]=2;sequence=63;
    } else sequence=17;
    if (!m_animate(m,sequence)) return false;
    s[0x48]=0;return true;
}
static bool spike_landing(Motion *m) {
    if (!m_next(m,0xee18)) return false;
    return !m->result.zero?m_next(m,0x3a0f):finish_landing(m,false);
}
/* Original3A6E: snap to row, resolve nearby spike tile and landing damage. */
static bool land(Motion *m) {
    uint8_t *s=m->state->aux,height;
    if (!m_row_height(m,&height)) return false;
    s[0x42]=height;
    if (!m_next(m,0xd0ba)) return false;
    if (m->result.a==2) return spike_landing(m);
    if (!m_next(m,0xd0bd) || !m_next(m,0xd04b)) return false;
    if (m->result.zero) {
        if (!m_next(m,0xd018)) return false;
        if (m->result.a<4) m_relative_x(m,0xfd);
    }
    if (!m_next(m,0xe01e)) return false;
    if (s[0x4f]<128) return finish_landing(m,true);
    if (!m_next(m,0xd018)) return false;
    if (m->result.a>=12) {
        if (!m_next(m,0xd0c0)) return false;
        if (m->result.a==2) return spike_landing(m);
    }
    if (!m_next(m,0xd0ba)) return false;
    return m->result.a==2?spike_landing(m):finish_landing(m,false);
}
/* Original3B6B: lose support, choose matching fall sequence, check nearby edge. */
static bool supporting_floor(Motion *m) {
    uint8_t *s=m->state->aux,pose,sequence;
    if ((s[0x85]&64u)==0) return true;
    if (!m_next(m,0xd0ba)) return false;
    if (m->result.a==20 && !loose_floor_alignment(m)) return false;
    if (!m_next(m,0xd04b)) return false;
    if (!m->result.zero) return true;
    if (s[0x3f4]==12 && (s[0xdf]&128u) && s[0x45]==0 &&
        (s[0x4b]==2 || (s[0x4b]==13 && s[0x91]>=6))) {
        /* Original3B9F changes the special level12 tile map, outside level1. */
        m->state->required_helper=0x3b9f;return false;
    }
    s[0x23a]=0;s[0x4e]=0;++s[0x45];
    if (!m_next(m,0xe01e)) return false;
    pose=s[0x40];s[0x23a]=pose;
    if (pose==13) sequence=19;
    else if (pose==26) sequence=18;
    else if (pose==44) sequence=21;
    else if (pose>=81 && pose<86) {
        m_relative_x(m,5);if (!m_refresh(m)) return false;sequence=19;
    } else if (pose>=150 && pose<180) {
        if (s[0x4d]>=2) {
            if (s[0x47]&128u) sequence=82;
            else { s[0xe7]=0;sequence=83; }
        } else { s[0xe7]=1;sequence=81; }
    } else sequence=7;
    if (!m_animate(m,sequence) || !m_refresh(m) ||
        !m_next(m,0xd0ba) || !m_next(m,0xd0c9)) return false;
    if (m->result.zero) return loose_floor_alignment(m);
    if (!m_next(m,0xd0bd) || !m_next(m,0xd0c9)) return false;
    if (!m->result.zero) return true;
    if (s[0x23a]==44) {
        if (!m_next(m,0xd018)) return false;
        if (m->result.a<6) return m_animate(m,104) && m_refresh(m);
    }
    m_relative_x(m,0xff);return m_refresh(m);
}
static bool floor_decision(Motion *m) {
    uint8_t *s=m->state->aux,height;
    if (s[0x46]==6 || s[0x46]==2) return true;
    if (s[0x46]==5) return (s[0x40]==109 || s[0x40]==185)?supporting_floor(m):true;
    if (s[0x46]==3) return (s[0x40]>=102 && s[0x40]<106)?grab_ledge(m):true;
    if (s[0x46]!=4) return supporting_floor(m);
    if (!m_row_height(m,&height)) return false;
    if (s[0x42]<height) return grab_ledge(m);
    if (!m_next(m,0xd0ba)) return false;
    if (m->result.a==20 && !loose_floor_alignment(m)) return false;
    if (!m_next(m,0xd04b)) return false;
    if (!m->result.zero) return land(m);
    ++s[0x45];return true;
}
static bool tile_effects(Motion *m) {
    uint8_t *s=m->state->aux,pose=s[0x40];
    if ((pose>=87 && pose<100) || (pose>=135 && pose<141)) {
        if (!m_next(m,0xd0c3)) return false;
    } else {
        if (s[0x46]!=7 && s[0x46]!=5 && s[0x46]>=2) return true;
        if (pose==79) {
            if (!m_next(m,0xd0c3)) return false;
            return m->result.a==11?m_next(m,0xee0c):true;
        }
        if ((s[0x85]&64u)==0) return true;
        if (!m_next(m,0xd0ba)) return false;
    }
    if (m->result.a==15 || m->result.a==6)
        return m_next(m,(s[0x4f]&128u)?0xee06:0xee36);
    if (m->result.a==11) {s[0xe4]=1;return m_query(m,0xee0c,1);}
    return true;
}
static bool spikes(Motion *m) {
    uint8_t *s=m->state->aux,pose=s[0x40];
    m->result.x=s[0x44];m->result.y=s[0x45];
    if (!m_query(m,0xd021,s[0x4b])) return false;
    if (m->result.a!=2 || pose<7) return true;
    if (pose<15) {
        if (!m_next(m,0xee18)) return false;
        if (m->result.a<2) return true;
    } else if (pose==43 || pose==26) {
        if (!m_next(m,0xee18)) return false;
        if (m->result.zero) return true;
    } else return true;
    return m_next(m,0x3a0f);
}
/* Original4AF0/4B43/4803: gate height and the two collision-record masks. */
static bool gate_correction(Motion *m) {
    uint8_t *s=m->state->aux,first,second,spec;
    if (s[0x46]!=7 && s[0x40]!=15 && !(s[0x40]>=108 && s[0x40]<111)) return true;
    if (!m_next(m,0xd0ba)) return false;
    if (m->result.a!=4) {
        --s[0x91];
        if (!m_next(m,0xd024)) return false;
        if (m->result.a!=4) return true;
    }
    if (!m_variable(m,(uint16_t)(0x320u+s[0x91]),&first) ||
        !m_variable(m,(uint16_t)(0x330u+s[0x91]),&second)) return false;
    if ((uint8_t)(first&second)!=255) return true;
    if (!m_spec(m,&spec)) return false;
    if ((uint8_t)((spec>>2)+6u)>=s[0xb5]) return true;
    s[0xe4]=1;
    if (!m_query(m,0xd909,13)) return false;
    s[0x87]=s[0x91];
    if (!m_next(m,0xd0ba)) return false;
    s[0x41]=(uint8_t)(s[0x41]+(s[0x91]<=s[0x87]?251u:5u));
    return true;
}
/* Original4714/477E. The one-step row correction is deliberately not modulo3. */
static bool wall_probe(Motion *m,uint8_t candidate,bool *active) {
    uint8_t *s=m->state->aux,row=s[0x45],spec,value;
    *active=false;s[0x91]=candidate;
    if (row&128u) row=(uint8_t)(row+3u);
    else if (row>=3) row=(uint8_t)(row-3u);
    s[0x92]=row;
    if (!m_variable(m,(uint16_t)(0x360u+candidate),&s[0x93]) ||
        !m_query(m,0xd024,s[0x93])) return false;
    if (m->result.a==10) return true;
    if (m->result.a==4) {
        if (!m_spec(m,&spec)) return false;
        if ((uint8_t)((spec>>2)+6u)>=s[0xb5]) return true;
    } else if (m->result.a==18) {
        if (!m_spec(m,&spec)) return false;
        if (spec!=2) return true;
    } else if (m->result.a==13 && s[0x4d]==0 && s[0x40]>=39 && s[0x40]<44 && (s[0x43]&128u)) {
        if (!m_next(m,0xee27)) return false;
        s[0xe5]=255;return true;
    }
    if (!m_query(m,0xd012,s[0x91])) return false;
    value=m->result.a;
    if (s[0x93]!=s[0xcb]) {
        if (s[0x93]==s[0x31] || s[0x93]==s[0x35]) value=(uint8_t)(value-140u);
        else if (s[0x93]==s[0x32] || s[0x93]==s[0x38]) value=(uint8_t)(value+140u);
    }
    s[0xc2]=(uint8_t)(value+7u);*active=true;return true;
}
/* Original4676/468B: classification selects the immutable4521/4526 tables. */
static bool wall_edge(Motion *m,bool left,uint8_t *edge) {
    uint8_t *s=m->state->aux,offset;
    m->result.x=s[0x91];m->result.y=s[0x92];
    if (!m_query(m,0xd021,s[0x93]) || !m_next(m,0xd04e)) return false;
    if (m->result.zero) {*edge=left?0:255;return true;}
    if (!m_read(m,(uint16_t)((left?0x4526u:0x4521u)+m->result.a),&offset)) return false;
    *edge=left?(uint8_t)(s[0xc2]+13u-offset):(uint8_t)(s[0xc2]+offset);
    return true;
}
static bool wall_bump(Motion *m) {
    m_relative_x(m,0xfc);
    if (m->state->aux[0x46]==4) m->state->aux[0x47]=0;
    else if (!m_animate(m,45)) return false;
    return m_sound13(m);
}
/* Original479D. Sound and immediate animation order differs across responses. */
static bool wall_adjust(Motion *m,uint8_t displacement,uint8_t direction) {
    uint8_t *s=m->state->aux,height,pose;
    s[0xf1]=direction;
    if ((s[0x4f]&128u)==0 || s[0x40]==177) return true;
    s[0x41]=(uint8_t)(s[0x41]+displacement);
    if (!m_next(m,0xd024)) return false;
    if (direction&128u) {
        if (m->result.a==20) {--s[0x91];if (!m_next(m,0xd024)) return false;}
    } else if (m->result.a==12 || m->result.a==7 || m->result.a==20) {
        ++s[0x91];
        if (s[0x93]==0 && s[0x91]==10) {s[0x93]=s[0x4b];s[0x91]=0;}
        if (!m_next(m,0xd024)) return false;
    }
    if (!m_next(m,0xd04b)) return false;
    if (m->result.zero) return wall_bump(m);
    if (!m_read(m,(uint16_t)(0x127fu+s[0x45]),&height)) return false;
    if (s[0x4e]!=2 && (uint8_t)(height-s[0x42])>=15) return wall_bump(m);
    s[0x42]=height;
    if (s[0x48]>=22) {m_relative_x(m,0xfb);return true;}
    s[0x48]=0;
    if (s[0x4f]==0) return true;
    if (s[0x4e]==2) {
        if (s[0xf1]!=s[0x43]) return m_animate(m,64) && m_sound13(m);
        if (!m_animate(m,65)) return false;
        m_relative_x(m,1);return true;
    }
    pose=s[0x40];
    if (pose==24 || pose==25 || (pose>=40 && pose<43) || (pose>=102 && pose<107))
        return m_animate(m,46) && m_sound13(m);
    pop_controller_select_sequence(m->state,47);
    return m_sound13(m) && m_query(m,0x450c,s[0x4a]);
}
static bool wall_correction(Motion *m) {
    uint8_t *s=m->state->aux,edge,candidate;bool active,left;
    if (s[0xc7]) {
        if (!m_query(m,0xc030,0)) return false;
        --s[0xc7];return true;
    }
    if (s[0x46]==2 || s[0x46]==6 || (s[0x40]>=135 && s[0x40]<149)) return true;
    if ((s[0xc3]&128u)==0) {
        candidate=s[0xc3];s[0x87]=candidate;left=false;
        if (s[0x4e]!=2 && s[0x43]!=0) return true;
    } else {
        if (s[0xc4]&128u) return true;
        candidate=s[0xc4];s[0x87]=candidate;left=true;
        if (s[0x4e]!=2 && (s[0x43]&128u)==0) return true;
    }
    if (!wall_probe(m,candidate,&active)) return false;
    if (!active) return true;
    if (!wall_edge(m,left,&edge)) return false;
    return wall_adjust(m,(uint8_t)(edge-s[left?0xbe:0xbf]),left?0:255);
}
/* Original4A47: scan collision records from column9 down to0, not all tiles. */
static bool chomper_hit(Motion *m,uint8_t spec) {
    uint8_t *s=m->state->aux,height;
    uint16_t address,base;
    base=(uint16_t)(s[0x26]|((uint16_t)s[0x27]<<8));
    address=(uint16_t)(base+m->result.y);
    if (!m->write || !m->io || !m->write(m->io->context,address,(uint8_t)(spec|128u))) {
        m->state->required_helper=0x4a74;return false;
    }
    if (s[0x40]==178) return true;
    if (!m_query(m,0xd012,s[0x91])) return false;
    s[0x41]=(uint8_t)(m->result.a+7u);m_relative_x(m,8);
    if (!m_row_height(m,&height)) return false;
    s[0x42]=height;
    if (!m_query(m,0xf927,100) || !m_query(m,0xd909,5)) return false;
    return m_animate(m,54);
}
static bool chompers(Motion *m) {
    uint8_t *s=m->state->aux,record,spec;int column;
    s[0x92]=s[0x45];
    for (column=9;column>=0;--column) {
        s[0x91]=(uint8_t)column;
        if (!m_variable(m,(uint16_t)(0x320u+(unsigned)column),&record)) return false;
        if (record!=255) continue;
        if (!m_variable(m,(uint16_t)(0x360u+(unsigned)column),&s[0x93]) ||
            !m_query(m,0xd024,s[0x93])) return false;
        if (m->result.a!=18) continue;
        if (!m_spec(m,&spec)) return false;
        if ((spec&127u)!=2) continue;
        return chomper_hit(m,spec);
    }
    return true;
}
/* Original4B43: carry is the unsigned height comparison. */
static bool guard_gate_passable(Motion *m,bool *passable) {
    uint8_t spec;
    if (!m_spec(m,&spec)) return false;
    *passable=(uint8_t)((spec>>2)+6u)>=m->state->aux[0xb5];return true;
}
/* Original4733, entered with current tile pointers/coordinates already set. */
static bool guard_wall_predicate(Motion *m,uint8_t tile,bool *accepted) {
    uint8_t *s=m->state->aux,spec,value;bool passable;
    *accepted=false;
    if (tile==10) return true;
    if (tile==4) {
        if (!guard_gate_passable(m,&passable)) return false;
        if (passable) return true;
    } else if (tile==18) {
        if (!m_spec(m,&spec)) return false;
        if (spec!=2) return true;
    } else if (tile==13 && !s[0x4d] && s[0x40]>=39 && s[0x40]<44 && (s[0x43]&128u)) {
        if (!m_next(m,0xee27)) return false;
        s[0xe5]=255;return true;
    }
    if (!m_query(m,0xd012,s[0x91])) return false;
    value=m->result.a;
    if (s[0x93]!=s[0xcb]) {
        if (s[0x93]==s[0x31] || s[0x93]==s[0x35]) value=(uint8_t)(value-140u);
        else if (s[0x93]==s[0x32] || s[0x93]==s[0x38]) value=(uint8_t)(value+140u);
    }
    s[0xc2]=(uint8_t)(value+7u);*accepted=true;return true;
}
static bool guard_wall_correction(Motion *m) {
    uint8_t *s=m->state->aux,tile,offset,penetration;bool blocked=false,passable,accepted;
    if (s[0xc7] || s[0x46]!=1 || !(s[0x4f]&128u) || s[0x4e]<2) return true;
    if (!m_next(m,0xd0ba)) return false;
    tile=m->result.a;
    if (tile==20 || tile==7) blocked=true;
    else if (tile==4) {
        if (!guard_gate_passable(m,&passable)) return false;
        blocked=!passable;
    }
    if (!blocked) {
        if (s[0x43]&128u) return true;
        --s[0x91];
        if (!m_next(m,0xd024)) return false;
        if (m->result.a==7) blocked=true;
        else if (m->result.a==4) {
            if (!guard_gate_passable(m,&passable)) return false;
            blocked=!passable;
        }
    }
    if (!blocked) return true;
    if (!m_next(m,0xd066) || !m_next(m,0xd033)) return false;
    m->result.x=s[0x91];m->result.y=s[0x92];
    if (!m_query(m,0xd021,s[0x93])) return false;
    s[0xf2]=m->result.a;
    if (!guard_wall_predicate(m,s[0xf2],&accepted)) return false;
    if (!accepted) return true;
    if (!m_query(m,0xd04e,s[0xf2])) return false;
    if (m->result.zero) penetration=255;
    else {
        if (!m_read(m,(uint16_t)(((s[0x43]&128u)?0x4521u:0x4526u)+m->result.a),&offset)) return false;
        if (s[0x43]&128u) {s[0xf0]=(uint8_t)(s[0xc2]+offset);penetration=(uint8_t)(s[0xf0]-s[0xbf]);}
        else {s[0xf0]=(uint8_t)(s[0xc2]+13u-offset);penetration=(uint8_t)(s[0xbe]-s[0xf0]);}
    }
    if (!(penetration&128u)) return true;
    m_relative_x(m,(uint8_t)(0u-penetration));
    return m_animate(m,65) && m_refresh(m);
}
/* Original4AB4: test two tile edges, then preserve4A70 write/damage order. */
static bool guard_chomper_test(Motion *m,bool *hit) {
    uint8_t *s=m->state->aux,spec,edge;
    *hit=false;
    if (m->result.a!=18) return true;
    if (!m_spec(m,&spec)) return false;
    if ((spec&127u)!=2) return true;
    if (!m_query(m,0xd012,s[0x91])) return false;
    s[0xc2]=(uint8_t)(m->result.a+7u);
    if (!wall_edge(m,false,&edge)) return false;
    if (edge>=s[0xbf]) return true;
    if (!wall_edge(m,true,&edge)) return false;
    if (edge<=s[0xbe]) return true;
    if (!m_next(m,0xd024) || !m_spec(m,&spec) || !chomper_hit(m,spec)) return false;
    *hit=true;return true;
}
static bool guard_chompers(Motion *m) {
    bool hit;
    if (!m_next(m,0xd0ba) || !guard_chomper_test(m,&hit)) return false;
    if (hit) return true;
    ++m->state->aux[0x91];
    return m_next(m,0xd024) && guard_chomper_test(m,&hit);
}
/* Original4616: keep the record row that refers to the actor's new row. The
 * three comparisons use byte arithmetic, including their wrapped +/-3 cases. */
static void collision_keep_previous_row(uint8_t *s) {
    uint8_t row=s[0x39a],previous=s[0x39b];
    unsigned room_base,mask_base;int column;
    if (row==previous || (uint8_t)(row+3u)==previous ||
        (uint8_t)(row-3u)==previous) {room_base=0x360;mask_base=0x320;}
    else if ((uint8_t)(row+1u)==previous || (uint8_t)(row-2u)==previous) {
        room_base=0x390;mask_base=0x350;
    } else {room_base=0x380;mask_base=0x340;}
    for (column=9;column>=0;--column) {
        s[0x370u+(unsigned)column]=s[room_base+(unsigned)column];
        s[0x330u+(unsigned)column]=s[mask_base+(unsigned)column];
        s[0x360u+(unsigned)column]=255;
        s[0x390u+(unsigned)column]=255;
        s[0x380u+(unsigned)column]=255;
    }
}
/* Original4676/468B as used by45BD. Query normalization may change the room
 * and column. Re-query from the unnormalized scan coordinates on both edges. */
static bool collision_scan_edge(Motion *m,bool right_edge,uint8_t *edge) {
    uint8_t *s=m->state->aux,offset;
    m->result.x=s[0xc1];m->result.y=s[0x8d];
    if (!m_query(m,0xd021,s[0x4b]) || !m_next(m,0xd04e)) return false;
    if (m->result.zero) {*edge=right_edge?0:255;return true;}
    if (!m_read(m,(uint16_t)((right_edge?0x4526u:0x4521u)+m->result.a),&offset)) return false;
    *edge=right_edge?(uint8_t)(s[0xc2]+13u-offset):(uint8_t)(s[0xc2]+offset);
    return true;
}
/* Original45BD replaces only absolute-address low bytes. Native base offsets
 * express those record destinations directly; no executable bytes are changed. */
static bool collision_scan_row(Motion *m,unsigned room_base,unsigned mask_base) {
    uint8_t *s=m->state->aux,edge,column,index,mask;
    unsigned iterations=0,address;
    if (!m_query(m,0xd012,s[0xc8])) return false;
    s[0xc2]=(uint8_t)(m->result.a+7u);column=s[0xc8];
    do {
        s[0xc1]=column;
        if (!collision_scan_edge(m,false,&edge)) return false;
        s[0xf0]=edge<s[0xbf]?15:0;
        if (!collision_scan_edge(m,true,&edge)) return false;
        mask=(uint8_t)(s[0xf0]|(edge>s[0xbe]?240u:0u));
        index=s[0x91];address=mask_base+index;
        if (address>=sizeof m->state->aux || room_base+index>=sizeof m->state->aux) {
            m->state->required_helper=(uint16_t)address;return false;
        }
        s[address]=mask;s[room_base+index]=s[0x93];
        s[0xc2]=(uint8_t)(s[0xc2]+14u);column=(uint8_t)(s[0xc1]+1u);
        ++iterations;
    } while (column!=s[0xc0] && iterations<256u);
    return true;
}
static bool collision_prepass(Motion *m) {
    uint8_t *s=m->state->aux,end;int column;
    s[0xc3]=255;s[0xc4]=255;
    if (s[0x46]==7) return true;
    s[0x39a]=s[0x45];collision_keep_previous_row(s);s[0x39b]=s[0x39a];
    if (!m_query(m,0xd00c,s[0xbf])) return false;
    end=(uint8_t)(m->result.a+2u);s[0xc0]=end<11?end:11;
    if (!m_query(m,0xd00c,s[0xbe])) return false;
    s[0xc8]=(uint8_t)(m->result.a-1u);
    s[0x8d]=s[0x39a];if (!collision_scan_row(m,0x360,0x320)) return false;
    s[0x8d]=(uint8_t)(s[0x39a]+1u);if (!collision_scan_row(m,0x380,0x340)) return false;
    s[0x8d]=(uint8_t)(s[0x39a]-1u);if (!collision_scan_row(m,0x390,0x350)) return false;
    for (column=9;column>=0;--column) {
        unsigned i=(unsigned)column;
        if ((s[0x360u+i]&128u) || s[0x360u+i]!=s[0x370u+i]) continue;
        if (!(s[0x330u+i]&15u) && (s[0x320u+i]&15u)) s[0xc3]=(uint8_t)column;
        if (!(s[0x330u+i]&240u) && (s[0x320u+i]&240u)) s[0xc4]=(uint8_t)column;
    }
    return true;
}
static PopControllerResult run_motion_decision(PopControllerState *s,const PopControllerIO *io,
                                              bool (*decide)(Motion *),PopMotionWrite write) {
    PopControllerState work;Motion m;
    if (!s) return POP_CONTROLLER_INVALID;
    work=*s;work.required_helper=0;work.selected_sequence=0;
    m=(Motion){&work,io,{0,0,0,false,false,false},write};
    if (!decide(&m)) {s->required_helper=work.required_helper;return POP_CONTROLLER_HELPER_REQUIRED;}
    *s=work;return POP_CONTROLLER_COMPLETE;
}
PopControllerResult pop_motion_resolve_floor(PopControllerState *s,const PopControllerIO *io) {
    return run_motion_decision(s,io,floor_decision,NULL);
}
PopControllerResult pop_motion_check_tile_effects(PopControllerState *s,const PopControllerIO *io) {
    return run_motion_decision(s,io,tile_effects,NULL);
}
PopControllerResult pop_motion_check_spikes(PopControllerState *s,const PopControllerIO *io) {
    return run_motion_decision(s,io,spikes,NULL);
}
PopControllerResult pop_motion_correct_gate(PopControllerState *s,const PopControllerIO *io) {
    return run_motion_decision(s,io,gate_correction,NULL);
}
PopControllerResult pop_motion_correct_wall(PopControllerState *s,const PopControllerIO *io) {
    return run_motion_decision(s,io,wall_correction,NULL);
}
PopControllerResult pop_motion_build_collision(PopControllerState *s,const PopControllerIO *io) {
    return run_motion_decision(s,io,collision_prepass,NULL);
}
PopControllerResult pop_motion_check_chompers(PopControllerState *s,const PopControllerIO *io,PopMotionWrite write) {
    return run_motion_decision(s,io,chompers,write);
}
PopControllerResult pop_motion_correct_guard_wall(PopControllerState *s,const PopControllerIO *io) {
    return run_motion_decision(s,io,guard_wall_correction,NULL);
}
PopControllerResult pop_motion_check_guard_chompers(PopControllerState *s,const PopControllerIO *io,PopMotionWrite write) {
    return run_motion_decision(s,io,guard_chompers,write);
}
