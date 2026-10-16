#include "actor_draw.h"

PopActorDrawStatus pop_actor_draw_projected(const uint8_t *aux, size_t aux_bytes,
                                           uint8_t object_kind,
                                           PopSceneDraw *out)
{
    static const PopAssetTable tables[] = {
        POP_ASSET_CHARACTER_0, POP_ASSET_CHARACTER_1, POP_ASSET_CHARACTER_2,
        POP_ASSET_CHARACTER_3, POP_ASSET_CHARACTER_4
    };
    PopAsset image;
    if (aux == NULL || aux_bytes < 256 || out == NULL) return POP_ACTOR_DRAW_INVALID;
    if (object_kind != 0 && object_kind != 2 && object_kind != 3 && object_kind != 4)
        return POP_ACTOR_DRAW_UNSUPPORTED;
    /* 5022 normal guard color0 reaches5029. The color-shift branch5033
     * needs a separate source fixture; a short player-only ZP view cannot
     * establish the absolute0216 guard-color field. */
    if (object_kind == 2 && (aux_bytes <= 0x216 || aux[0x216] != 0))
        return POP_ACTOR_DRAW_UNSUPPORTED;
    /* 1E4D selects5006 for kinds0/4; kind3 goes directly to5029. */
    if (object_kind == 0 || object_kind == 4) {
        if (aux[0xa2] != 0 && aux[0] == 0) return POP_ACTOR_DRAW_SKIPPED;
        if ((aux[0xdf] & 0x80u) == 0 && (aux[0xdf] & 1u) != 0)
            return POP_ACTOR_DRAW_UNSUPPORTED;
    }
    if (aux[0x7a] >= sizeof(tables) / sizeof(tables[0]))
        return POP_ACTOR_DRAW_UNSUPPORTED;
    if (aux[0x76] > aux[0x77] || aux[0x77] > 192 ||
        aux[0x78] > aux[0x79] || aux[0x79] > 40 ||
        !pop_asset_get(tables[aux[0x7a]], aux[0x70], &image))
        return POP_ACTOR_DRAW_INVALID;
    /* Original0862 normalizes by252 pixels/36 bytes, then uses captured
     * E00[x]=floor(x/7), F00[x]=x%7. Floor division preserves negative
     * offscreen placements; C truncation alone would give the wrong phase. */
    int pixel = (int)aux[0x71] + 256 * (int)aux[0x72];
    if (pixel >= 32768) pixel -= 65536;
    int byte_x = pixel / 7;
    int phase = pixel % 7;
    if (phase < 0) {
        phase += 7;
        --byte_x;
    }
    const PopSceneDraw draw = {
        tables[aux[0x7a]], aux[0x70], {
            (uint8_t)byte_x, aux[0x73], (uint8_t)phase,
            (uint8_t)(4u | ((aux[0x74] ^ 0xffu) & 0x80u)),
            aux[0x76], aux[0x78], aux[0x79], aux[0x77]
        }
    };
    *out = draw;
    return POP_ACTOR_DRAW_READY;
}
