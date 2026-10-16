#include "items.h"
#include <stddef.h>
typedef struct Item {
    PopControllerState *state;PopControllerQueryResult *result;
    const PopControllerIO *io;PopItemWrite write;
} Item;
static void nz(PopControllerQueryResult *r,uint8_t value) {r->zero=value==0;r->negative=(value&128u)!=0;}
static void value_a(PopControllerQueryResult *r,uint8_t value) {r->a=value;nz(r,value);}
static void value_x(PopControllerQueryResult *r,uint8_t value) {r->x=value;nz(r,value);}
static void compare(PopControllerQueryResult *r,uint8_t a,uint8_t b) {r->carry=a>=b;nz(r,(uint8_t)(a-b));}
static bool query(Item *item,uint16_t helper) {
    if (item->io && item->io->query && item->io->query(item->io->context,helper,item->result->a,item->state,item->result)) return true;
    item->state->required_helper=helper;return false;
}
static bool read_value(Item *item,uint16_t address,uint8_t *value) {
    if (item->io && item->io->read && item->io->read(item->io->context,address,value)) return true;
    item->state->required_helper=address;return false;
}
static bool write_value(Item *item,uint16_t address,uint8_t value) {
    if (item->io && item->write && item->write(item->io->context,address,value)) return true;
    item->state->required_helper=address;return false;
}
/* D0B7/D8A1 thenD899: maximum health and the pending heal delta. */
static void increase_health(Item *item) {
    uint8_t *s=item->state->aux;PopControllerQueryResult *r=item->result;
    value_a(r,s[0xd0]);compare(r,r->a,10);
    if (!r->carry) {value_a(r,(uint8_t)(r->a+1u));r->carry=false;s[0xd0]=r->a;}
    value_a(r,s[0xd0]);r->carry=r->a>=s[0xce];value_a(r,(uint8_t)(r->a-s[0xce]));s[0xcf]=r->a;
}
static bool effect(Item *item) {
    uint8_t *s=item->state->aux;PopControllerQueryResult *r=item->result;
    value_a(r,s[0x4d]);if (!r->zero) return true;
    value_x(r,s[0xe0]);if (r->zero) return true;
    if (r->negative) {
        value_a(r,1);s[0x30a]=1;value_a(r,4);value_x(r,25);
        if (!query(item,0xd930)) return false;
        value_a(r,255);s[0x89]=r->a;value_a(r,3);s[0x88]=r->a;return true;
    }
    compare(r,r->x,1);
    if (r->zero) {
        value_a(r,s[0xce]);compare(r,r->a,s[0xd0]);if (r->zero) return true;
        value_a(r,153);s[0x89]=r->a;value_a(r,2);s[0x88]=r->a;
        value_a(r,12);value_x(r,25);if (!query(item,0xd930)) return false;
        value_a(r,1);s[0xcf]=r->a;return true;
    }
    compare(r,r->x,2);
    if (r->zero) {
        value_a(r,153);s[0x89]=r->a;value_a(r,5);s[0x88]=r->a;
        value_a(r,11);value_x(r,25);if (!query(item,0xd930)) return false;
        increase_health(item);return true;
    }
    compare(r,r->x,3);
    if (r->zero) {
        value_a(r,12);value_x(r,25);if (!query(item,0xd930)) return false;
        value_a(r,200);s[0xc5]=r->a;value_a(r,3);s[0x318]=r->a;return true;
    }
    compare(r,r->x,4);
    if (r->zero) {item->state->required_helper=0x04c3;return false;}
    compare(r,r->x,5);
    if (r->zero) {
        value_a(r,5);if (!query(item,0xd909)) return false;
        value_a(r,255);s[0xcf]=r->a;
    }
    return true;
}
/* E030/E7A7: type/spec mutation precedes both source dirty producers. The
 * first-level item cells haveY0..29, so the CLC selects their ordinary arrays. */
static bool pickup(Item *item,uint8_t argument) {
    uint8_t *s=item->state->aux,repeat,height;PopControllerQueryResult *r=item->result;
    uint16_t type=(uint16_t)(s[0x24]+256u*s[0x25]+r->y);
    uint16_t modifier=(uint16_t)(s[0x26]+256u*s[0x27]+r->y);
    value_a(r,argument);s[0xe0]=r->a;value_x(r,1);s[0xd8]=r->x;
    value_a(r,1);if (!write_value(item,type,r->a)) return false;
    value_a(r,0);if (!write_value(item,modifier,r->a)) return false;
    value_a(r,35);s[4]=r->a;value_a(r,2);r->carry=false;
    if (r->y>=30) {item->state->required_helper=0xd075;return false;}
    if (!read_value(item,(uint16_t)(0x5eb4u+r->y),&repeat)) return false;
    if (!repeat) {
        if (!write_value(item,(uint16_t)(0x5f0eu+r->y),s[4])) return false;
    } else {
        if (!read_value(item,(uint16_t)(0x5f0eu+r->y),&height)) return false;
        if (s[4]>=height && !write_value(item,(uint16_t)(0x5f0eu+r->y),s[4])) return false;
    }
    value_a(r,2);r->carry=false;
    return write_value(item,(uint16_t)(0x5eb4u+r->y),r->a) &&
           write_value(item,(uint16_t)(0x5e5au+r->y),r->a);
}
bool pop_item_query(uint16_t helper,uint8_t argument,PopControllerState *s,
    PopControllerQueryResult *r,const PopControllerIO *io,PopItemWrite write) {
    PopControllerState work;PopControllerQueryResult output;Item item;bool ok;
    if (!s || !r) return false;
    work=*s;output=*r;item=(Item){&work,&output,io,write};
    switch(helper) {
    case 0xf90f:ok=effect(&item);break;
    case 0xe030:ok=pickup(&item,argument);break;
    case 0xd0b7:increase_health(&item);ok=true;break;
    default:return false;
    }
    if (!ok) {s->required_helper=work.required_helper;return false;}
    *s=work;*r=output;return true;
}
