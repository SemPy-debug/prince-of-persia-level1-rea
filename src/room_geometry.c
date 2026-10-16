#include "room_geometry.h"
#include <string.h>

extern const PopGeometryOwner pop_geometry_owners[17];
extern const PopGeometryGroup pop_geometry_groups[24][40];
extern const PopGeometryVariant pop_geometry_variants[];
extern const PopRoomDrawTemplate pop_geometry_background[];
extern const PopForegroundDraw pop_geometry_foreground[];
extern const uint8_t pop_geometry_world_reads[24][90];

bool pop_room_geometry_uses_world_cell(unsigned room,unsigned index)
{
    return room>=1&&room<=24&&index<720&&
        (pop_geometry_world_reads[room-1u][index/8u]&(1u<<(index%8u)))!=0;
}

bool pop_room_geometry_program(const PopWorldState *world, unsigned room,
    PopRoomDrawTemplate *background, size_t background_capacity,
    PopForegroundDraw *foreground, size_t foreground_capacity,
    PopRoomVisualTemplate *program)
{
    if (world == NULL || room == 0 || room > 24 || background == NULL ||
        foreground == NULL || program == NULL) return false;
    uint32_t removed = 0, needed = 0;
    for(unsigned group=0;group<40;++group)needed|=pop_geometry_groups[room-1u][group].dependencies;
    for (unsigned bit = 0; bit < 17; ++bit) {
        if((needed&(UINT32_C(1)<<bit))==0)continue;
        const PopGeometryOwner *owner = &pop_geometry_owners[bit];
        const uint8_t type = world->types[owner->index];
        if (type == owner->replacement_type && world->specs[owner->index] == 0)
            removed |= UINT32_C(1) << bit;
        else if (type != owner->original_type) return false;
    }
    size_t bg_count = 0, fg_count = 0;
    const PopGeometryVariant *selected[40];
    for (unsigned group = 0; group < 40; ++group) {
        const PopGeometryGroup *entry = &pop_geometry_groups[room - 1][group];
        selected[group] = NULL;
        for (unsigned n = 0; n < entry->variant_count; ++n) {
            const PopGeometryVariant *variant =
                &pop_geometry_variants[entry->variant_offset + n];
            if (variant->signature == (removed & entry->dependencies)) {
                selected[group] = variant;
                break;
            }
        }
        if (selected[group] == NULL) return false;
        bg_count += selected[group]->background_count;
        fg_count += selected[group]->foreground_count;
        if (bg_count > background_capacity || fg_count > foreground_capacity)
            return false;
    }
    bg_count = fg_count = 0;
    for (unsigned group = 0; group < 40; ++group) {
        const PopGeometryVariant *v = selected[group];
        memcpy(background + bg_count, pop_geometry_background + v->background_offset,
               v->background_count * sizeof(*background));
        memcpy(foreground + fg_count, pop_geometry_foreground + v->foreground_offset,
               v->foreground_count * sizeof(*foreground));
        bg_count += v->background_count;
        fg_count += v->foreground_count;
    }
    *program = (PopRoomVisualTemplate){room, background, bg_count, foreground, fg_count};
    return true;
}
