#include "world_collision.h"
#include "geometry.h"
#include "data.h"
#include <stddef.h>

static void value_result(PopControllerQueryResult *q,uint8_t value) {
    q->a=value;q->zero=value==0;q->negative=(value&128u)!=0;
}
static void compare_result(PopControllerQueryResult *q,uint8_t left,uint8_t right) {
    uint8_t difference=(uint8_t)(left-right);
    q->carry=left>=right;q->zero=difference==0;q->negative=(difference&128u)!=0;
}
/* D80F: follow an empty column downward only while it remains in actor's room.
 * EE03 is a real spike effect, outside this read-only query component. */
static bool scan_column(PopWorldState *world,PopControllerState *s,PopControllerQueryResult *q,
                         const PopControllerIO *io) {
    unsigned rows;
    s->aux[0x91]=q->a;s->aux[0x92]=s->aux[0x45];s->aux[0x93]=s->aux[0x4b];
    for (rows=0;rows<256;++rows) {
        if (!pop_world_query(world,0xd024,q->a,s,q)) return false;
        compare_result(q,q->a,2);
        if (q->zero) {
            /* OriginalD820 tail-jumpsEE03, returning to the bounds loop. */
            return io && io->query && io->query(io->context,0xee03,q->a,s,q);
        }
        if (!pop_world_query(world,0xd04b,q->a,s,q)) return false;
        if (!q->zero) return true;
        value_result(q,s->aux[0x93]);
        if (q->zero) return true;
        compare_result(q,q->a,s->aux[0x4b]);
        if (!q->zero) return true;
        ++s->aux[0x92];q->zero=s->aux[0x92]==0;q->negative=(s->aux[0x92]&128u)!=0;
    }
    return false;
}
/* D7F0 enumerates the inclusive byte-column range produced by D00C. */
static bool scan_bounds(PopWorldState *world,PopControllerState *s,PopControllerQueryResult *q,
                         const PopControllerIO *io) {
    unsigned columns;
    if (!pop_world_query(world,0xd00c,s->aux[0xb8],s,q)) return false;
    if (q->negative) return true;
    s->aux[0xe8]=q->a;
    if (!pop_world_query(world,0xd00c,s->aux[0xb7],s,q)) return false;
    for (columns=0;columns<256;++columns) {
        s->aux[0x8c]=q->a;
        if (!scan_column(world,s,q,io)) return false;
        value_result(q,s->aux[0x8c]);compare_result(q,q->a,s->aux[0xe8]);
        if (q->zero) return true;
        q->carry=q->a==255;value_result(q,(uint8_t)(q->a+1u));
    }
    return false;
}
/* Original48FC: facing-relative distance from the projected actor edge to the
 * classified tile boundary. Negative255 is the original no-boundary sentinel. */
static bool boundary_distance(PopWorldState *world,PopControllerState *s,PopControllerQueryResult *q) {
    uint8_t spec,offset,edge;unsigned sum;
    uint16_t base;
    if (s->aux[0xf2]==4) {
        base=(uint16_t)(s->aux[0x26]|((uint16_t)s->aux[0x27]<<8));
        if (!pop_world_read(world,(uint16_t)(base+q->y),&spec)) return false;
        compare_result(q,(uint8_t)((spec>>2)+6u),s->aux[0xb5]);
        if (q->carry) {value_result(q,255);return true;}
    }
    if (!pop_world_query(world,0xd012,s->aux[0x91],s,q)) return false;
    sum=(unsigned)q->a+7u;q->carry=sum>=256;value_result(q,(uint8_t)sum);s->aux[0xc2]=q->a;
    if (!pop_world_query(world,0xd04e,s->aux[0xf2],s,q)) return false;
    if (q->zero) {value_result(q,255);return true;}
    q->y=q->a;
    if ((s->aux[0x43]&128u)==0) {
        if (!pop_data_read(NULL,(uint16_t)(0x4521u+q->y),&offset)) return false;
        edge=(uint8_t)(s->aux[0xc2]+offset);s->aux[0xf0]=edge;
        q->carry=edge>=s->aux[0xbf];value_result(q,(uint8_t)(edge-s->aux[0xbf]));
    } else {
        if (!pop_data_read(NULL,(uint16_t)(0x4526u+q->y),&offset)) return false;
        edge=(uint8_t)((uint8_t)(s->aux[0xc2]+13u)-offset);s->aux[0xf0]=edge;
        q->carry=s->aux[0xbe]>=edge;value_result(q,(uint8_t)(s->aux[0xbe]-edge));
    }
    return true;
}
static bool distance_return(PopControllerState *s,PopControllerQueryResult *q,uint8_t classification) {
    q->x=classification;q->y=s->aux[0xf2];q->zero=q->y==0;q->negative=(q->y&128u)!=0;return true;
}
static bool distance_classify(PopControllerState *s,PopControllerQueryResult *q) {
    compare_result(q,q->a,14);
    if (q->carry) {value_result(q,11);return distance_return(s,q,2);}
    return distance_return(s,q,1);
}
static bool distance_to_edge(PopWorldState *world,PopControllerState *s,PopControllerQueryResult *q) {
    if (!pop_world_query(world,0xd018,q->a,s,q)) return false;
    return distance_return(s,q,0);
}
/* Original4506/488A. Geometry is independently reconstructed native C;
 * no recorded distances or tile responses are embedded in this algorithm. */
static bool contact_distance(PopWorldState *world,PopControllerState *s,PopControllerQueryResult *q) {
    uint8_t tile;
    if (!pop_geometry_update_column(s->aux) || !pop_geometry_project_actor(s->aux) ||
        !pop_geometry_bounds(s->aux)) return false;
    /* D033's terminal instructions follow its final D00F/D009 table loads. */
    q->x=s->aux[0xbd];q->y=s->aux[0xb8];q->carry=s->aux[0xb8]>=s->aux[0xe9];value_result(q,s->aux[0xbf]);
    if (!pop_world_query(world,0xd0ba,q->a,s,q)) return false;
    s->aux[0xf2]=q->a;
    if (!pop_world_query(world,0xd04e,q->a,s,q)) return false;
    if (!q->zero) {
        s->aux[0x91]=s->aux[0x44];
        if (!boundary_distance(world,s,q)) return false;
        q->x=q->a;
        if ((q->a&128u)==0) return distance_classify(s,q);
    }
    if (!pop_world_query(world,0xd0bd,q->a,s,q)) return false;
    s->aux[0xf2]=q->a;compare_result(q,q->a,12);
    if (q->zero) {
        q->x=s->aux[0x43];
        if ((q->x&128u)==0) return distance_to_edge(world,s,q);
    }
    if (!pop_world_query(world,0xd04e,q->a,s,q)) return false;
    if (!q->zero) {
        s->aux[0x91]=s->aux[0x8e];
        if (!boundary_distance(world,s,q)) return false;
        q->x=q->a;
        if ((q->a&128u)==0) return distance_classify(s,q);
    }
    if (!pop_world_query(world,0xd0bd,q->a,s,q)) return false;
    s->aux[0xf2]=q->a;tile=q->a;
    if (tile==11) return distance_to_edge(world,s,q);
    if (tile==6 || tile==22 || tile==10) {
        if (!pop_world_query(world,0xd018,q->a,s,q)) return false;
        if (!q->zero) return distance_return(s,q,0);
    } else {
        if (!pop_world_query(world,0xd04b,q->a,s,q)) return false;
        if (q->zero) return distance_to_edge(world,s,q);
    }
    value_result(q,11);return distance_return(s,q,2);
}
static bool pair_predicate(PopWorldState *world,uint8_t argument,PopControllerState *s,
                           PopControllerQueryResult *q) {
    uint8_t other=s->aux[0x8b],spec,face=s->aux[0x43];
    uint16_t base=(uint16_t)(s->aux[0x26]|((uint16_t)s->aux[0x27]<<8));
    s->aux[0xe9]=argument;
    if (!pop_world_read(world,(uint16_t)(base+q->y),&spec)) return false;
    s->aux[0xeb]=spec;
    q->carry=other>=20;
    if (other==20) {value_result(q,0);return true;}
    q->carry=other>=12;
    if (other==12 && (face&128u)==0) {value_result(q,0);return true;}
    if (!pop_world_query(world,0xd04b,other,s,q)) return false;
    if (!q->zero) {value_result(q,0);return true;}
    q->carry=argument>=11;
    if (argument==11 && (argument&spec)!=0) {value_result(q,0);return true;}
    q->carry=argument>=7;
    if (argument==7 && (face&128u)!=0) {value_result(q,0);return true;}
    if (!pop_world_query(world,0xd04b,argument,s,q)) return false;
    value_result(q,q->zero?0:1);return true;
}
static bool dispatch(PopWorldState *world,uint16_t helper,uint8_t argument,
                      PopControllerState *s,PopControllerQueryResult *q,const PopControllerIO *io) {
    uint8_t face=s->aux[0x43];
    switch (helper) {
    case 0x4506:return contact_distance(world,s,q);
    case 0xd096:return scan_bounds(world,s,q,io);
    case 0xd090:return pair_predicate(world,argument,s,q);
    case 0xd0c9:
        q->carry=argument>=20;
        if (argument==20) {value_result(q,0);return true;}
        q->x=face;
        if ((face&128u)!=0) {
            q->carry=argument>=7;
            if (argument==7) {value_result(q,0);return true;}
            q->carry=argument>=12;
            if (argument==12) {value_result(q,0);return true;}
        }
        value_result(q,1);return true;
    case 0xd09f:
        if (face&128u) {
            /* Original BIT43 preserves incoming carry and derives Z from A&43. */
            q->a=argument;q->zero=(argument&face)==0;q->negative=true;
        } else {
            q->carry=argument==0;value_result(q,(uint8_t)(0u-argument));
        }
        return true;
    default:return false;
    }
}
bool pop_world_collision_query(void *world,uint16_t helper_id,uint8_t argument,
                               PopControllerState *state,PopControllerQueryResult *result) {
    PopControllerState staged;PopControllerQueryResult returned;
    if (!world || !state || !result) return false;
    staged=*state;returned=*result;
    if (!dispatch(world,helper_id,argument,&staged,&returned,NULL)) return false;
    *state=staged;*result=returned;return true;
}

bool pop_world_collision_query_io(void *world,uint16_t helper_id,uint8_t argument,
    PopControllerState *state,PopControllerQueryResult *result,const PopControllerIO *io) {
    if (!world || !state || !result) return false;
    return dispatch(world,helper_id,argument,state,result,io);
}
