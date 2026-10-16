#include "video.h"

/* Palette and standard-HGR neighborhood behavior informed by the general,
 * MIT-licensed Apple2js canvas renderer by Will Scullin and contributors.
 * This per-dot implementation is independent game display code; see the
 * retained reference, exact commit and license in research/video-evidence.
 * It executes no Apple instructions and has no game/emulator runtime input. */
static const uint8_t isolated_color[2][2][3] = {
    {{255, 68, 253}, {20, 245, 60}},
    {{20, 207, 253}, {255, 106, 60}}
};
/* Same MIT-licensed general display reference's standard lores palette. */
static const uint8_t lores_color[16][3]={
    {0,0,0},{227,30,96},{96,78,189},{255,68,253},
    {0,163,96},{156,156,156},{20,207,253},{208,195,255},
    {96,114,3},{255,106,60},{156,156,156},{255,160,208},
    {20,245,60},{208,221,141},{114,255,208},{255,255,255}
};

bool pop_video_lores_fill_rgba(uint8_t fill_byte,uint8_t *rgba,size_t rgba_bytes){
    if(rgba==NULL||rgba_bytes<POP_VIDEO_RGBA_BYTES)return false;
    for(unsigned y=0;y<POP_VIDEO_HEIGHT;++y){
        const uint8_t *rgb=lores_color[y%8u<4u?fill_byte&15u:fill_byte>>4];
        for(unsigned x=0;x<POP_VIDEO_WIDTH;++x){
            uint8_t *p=rgba+((size_t)y*POP_VIDEO_WIDTH+x)*4u;
            p[0]=rgb[0];p[1]=rgb[1];p[2]=rgb[2];p[3]=255;
        }
    }
    return true;
}

static bool dot(const uint8_t *row, int x)
{
    return x >= 0 && x < 280 &&
           ((row[(unsigned)x / 7u] >> ((unsigned)x % 7u)) & 1u) != 0;
}

bool pop_video_hgr_rgba(const uint8_t *page, size_t page_bytes,
                        uint8_t *rgba, size_t rgba_bytes)
{
    uintptr_t input_address, output_address;
    if (page == NULL || rgba == NULL || page_bytes < 8192u ||
        rgba_bytes < POP_VIDEO_RGBA_BYTES) {
        return false;
    }
    input_address = (uintptr_t)page;
    output_address = (uintptr_t)rgba;
    if (input_address <= output_address ?
        output_address - input_address < 8192u :
        input_address - output_address < POP_VIDEO_RGBA_BYTES) {
        return false;
    }
    for (unsigned y = 0; y < POP_VIDEO_HEIGHT; ++y) {
        const unsigned offset = ((y & 7u) << 10) |
                                ((y & 0x38u) << 4) | ((y >> 6) * 40u);
        const uint8_t *row = page + offset;
        for (unsigned x = 0; x < 280; ++x) {
            const bool left = dot(row, (int)x - 1);
            const bool center = dot(row, (int)x);
            const bool right = dot(row, (int)x + 1);
            unsigned column = x / 7u;
            uint8_t rgb[3] = {0, 0, 0};
            uint8_t *output = rgba + ((size_t)y * 280u + x) * 8u;
            /* Canonical left-to-right refresh includes the preceding dot in
             * each byte's neighborhood. Its final write of a last-column dot
             * therefore uses the following byte's phase color set. */
            if (x % 7u == 6u && column < 39u) {
                ++column;
            }
            if (center && (left || right)) {
                rgb[0] = rgb[1] = rgb[2] = 255;
            } else if (center || (left && right)) {
                const unsigned phase = (row[column] >> 7) & 1u;
                const unsigned parity = (x & 1u) ^ (center ? 0u : 1u);
                for (unsigned channel = 0; channel < 3; ++channel) {
                    unsigned value = isolated_color[phase][parity][channel];
                    rgb[channel] = (uint8_t)(center ? value : value * 3u / 4u);
                }
            }
            for (unsigned channel = 0; channel < 3; ++channel) {
                output[channel] = output[channel + 4] = rgb[channel];
            }
            output[3] = output[7] = 255;
        }
    }
    return true;
}
