#include "paused_render.h"
#include "data.h"
#include <string.h>

extern const uint8_t pop_torch_images[18];

bool pop_paused_torch_apply(const PopLoopPausedDraw *draw,
    uint8_t *page,size_t page_bytes)
{
    if (draw==NULL||page==NULL||page_bytes!=POP_HGR_PAGE_BYTES) return false;
    if (!draw->active) return true;
    if (draw->phase>=18||draw->x>=39||draw->y<43) return false;
    const uint8_t bottom=(uint8_t)(draw->y-43u);
    if (bottom>=192) return false;
    uint8_t output[POP_HGR_PAGE_BYTES];
    memcpy(output,page,sizeof(output));
    const uint8_t image=pop_torch_images[draw->phase];
    bool ready;
    if (draw->clip==0) {
        /* DC70→4C0F/51B0→043F: original EE09 background copy. */
        ready=pop_render_background_mode2(output,sizeof(output),
            POP_ASSET_BACKGROUND_1,image,(uint8_t)(draw->x+1u),bottom);
    } else {
        /* DC70→53B5 resets MAIN clip registers, then bottom becomes
         * the already adjusted flame Y−4. The final043C uses EE06. */
        if (bottom<4) return false;
        const PopRenderParams p={(uint8_t)(draw->x+1u),bottom,0,2,0,0,40,
                                  (uint8_t)(bottom-4u)};
        ready=pop_render_copy_or(output,sizeof(output),POP_ASSET_BACKGROUND_1,image,&p);
    }
    if (!ready) return false;
    memcpy(page,output,sizeof(output));
    return true;
}

bool pop_game_render_paused_hgr(const PopGame *game,const PopLoopModal *modal,
    uint8_t *page,size_t page_bytes)
{
    if (game==NULL||modal==NULL||page==NULL||!game->initialized||
        page_bytes!=POP_HGR_PAGE_BYTES) return false;
    if (!modal->active) return pop_game_render_view_hgr(game,page,page_bytes);
    const unsigned room=game->state.aux[0xcb];
    if (room<1||room>24||(game->state.aux[0]!=0&&game->state.aux[0]!=32)) return false;
    PopGame view=*game;
    unsigned slot=0;
    for (unsigned n=30;n>0;--n) {
        const unsigned cell=n-1u,owner=(room-1u)*30u+cell;
        if ((game->runtime.world.types[owner]&31u)!=19) continue;
        uint8_t height;
        if (slot>=8||cell%10u==9||
            !pop_data_read(NULL,(uint16_t)(0x127au+cell/10u),&height)) return false;
        if (modal->paused_objects[slot]!=(cell%10u+1u)*4u||
            modal->paused_objects[9+slot]!=(uint8_t)(height-3u)||
            modal->paused_objects[18+slot]>=18) return false;
        view.runtime.world.specs[owner]=modal->paused_objects[18+slot];
        ++slot;
    }
    if ((modal->paused_objects[slot]&128u)==0) return false;
    /*2641 flips00 beforeD921 and restores it on dismissal. The pure HUD
     * expects the completed ordinary frame page state, not that temporary. */
    view.state.aux[0]^=32u;
    return pop_game_render_view_hgr(&view,page,page_bytes);
}
