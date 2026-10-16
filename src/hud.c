#include "hud.h"
#include "../generated/hud_data.h"
#include <string.h>

static bool glyph(uint8_t *page, uint8_t image, uint8_t x, uint8_t y,
                   uint8_t phase, uint8_t mode)
{
    PopRenderParams p = {x,y,phase,mode,0,0,40,192};
    const PopAssetTable table = (image & 128u) != 0 ?
        POP_ASSET_BACKGROUND_2 : POP_ASSET_BACKGROUND_1;
    return pop_render_copy_or(page, POP_HGR_PAGE_BYTES,
        table, image & 127u, &p);
}

bool pop_hud_apply(const PopControllerState *state, uint8_t *page, size_t size)
{
    if (state == NULL || page == NULL || size != POP_HGR_PAGE_BYTES) return false;
    const uint8_t *s = state->aux;
    uint8_t output[POP_HGR_PAGE_BYTES];
    memcpy(output, page, sizeof(output));
    if (s[0x201] == 0) {
        if (s[0xce] > 12 || s[0xd0] > 12 || s[0xce] > s[0xd0]) return false;
        unsigned drawn = 0;
        while (drawn < s[0xce]) {
            const unsigned remaining = s[0xce] - drawn;
            const unsigned chunk = remaining >= 4 ? 3 : remaining >= 3 ? 2 : 1;
            const uint8_t image = remaining == 1 ? 136 : pop_health_chunk[chunk];
            if (remaining != 1 || s[0xce] >= 2 || s[0] == 0) {
                if (!glyph(output, image, pop_health_x[drawn],191,
                           pop_health_phase[drawn],2)) return false;
                drawn += chunk;
            } else break;
        }
        while (drawn < s[0xd0]) {
            if (!glyph(output,140,pop_health_x[drawn],191,pop_health_phase[drawn],0)) return false;
            ++drawn;
        }
        if (s[0xcc] != 0 && s[0x6d] != 24 && s[0x6d] != 4 &&
            (s[0x6d] != 1 || s[0x3f4] == 12)) {
            if (s[0xcc] > 11) return false;
            drawn = 0;
            bool terminal_heart = false;
            while (drawn < s[0xcc]) {
                const unsigned remaining = s[0xcc] - drawn;
                const unsigned chunk = remaining >= 4 ? 3 : remaining >= 3 ? 2 : 1;
                const uint8_t image = remaining == 1 ? 136 : pop_health_chunk[chunk];
                if (remaining != 1 || s[0xcc] >= 2 || s[0] == 0) {
                    if (!glyph(output,image,pop_guard_health_x[drawn],191,
                               pop_guard_health_phase[drawn],130)) return false;
                    drawn += chunk;
                    if (remaining == 1) { terminal_heart = true; break; }
                } else break;
            }
            if (!terminal_heart && !glyph(output,140,pop_guard_health_x[drawn],191,
                                pop_guard_health_phase[drawn],128)) return false;
        }
    }
    /* 26DA's live negative5F/030B1 branch selects4F6F (LEVEL + current
     *03F4 digits). The completed-frame DB is already decremented. */
    if (s[0xdb] != 0 && s[0xdb] < 18 && (s[0x5f] & 128u) != 0 && s[0x30b] == 1) {
        unsigned level = s[0x3f4];
        if (level >= 13) level = 12;
        const uint8_t *base = pop_hud_descriptors + 4;
        if (!glyph(output,base[3],base[1],base[0],base[2],2)) return false;
        base += 4;
        uint8_t x = base[1], phase = level < 10 ? base[2] : 0;
        if (!glyph(output,base[3],x,base[0],phase,1)) return false;
        x = (uint8_t)(x + 6u);
        const uint8_t tens = pop_level_tens[level];
        if (tens != 0) {
            if (!glyph(output,tens,x,base[0],phase,1)) return false;
            ++x;
        }
        if (!glyph(output,pop_level_ones[level],x,base[0],phase,1)) return false;
    }
    memcpy(page, output, sizeof(output));
    return true;
}
