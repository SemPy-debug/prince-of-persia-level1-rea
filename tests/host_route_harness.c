/* Whole native route through actual production update/event/render/audio code.
 * SDL dummy video only: this does not claim Windows GUI or OS keyboard input. */
#define main pop_host_application_main
#include "../src/windows_main.c"
#undef main
#include <assert.h>
#include "generated_host_route.h"

static void controls(Host *h,const char *action){
    const SDL_Scancode codes[]={SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,SDL_SCANCODE_LSHIFT};
    const char *names[]={"left","right","up","down","act"};
    for(unsigned i=0;i<5;++i){
        const bool down=strstr(action,names[i])!=NULL;
        if(h->keys[codes[i]]!=down){
            SDL_Event e={0};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.keysym.scancode=codes[i];
            handle_event(h,&e,true);
        }
    }
}

int main(int argc,char **argv){
    assert(argc==2);
    Host h={0};h.report=argv[1];
    assert(SDL_setenv("SDL_VIDEODRIVER","dummy",1)==0);
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_EVENTS)==0);
    h.window=SDL_CreateWindow("Native route verification",0,0,560,420,SDL_WINDOW_HIDDEN);
    assert(h.window!=NULL);
    h.renderer=SDL_CreateRenderer(h.window,-1,SDL_RENDERER_SOFTWARE);assert(h.renderer!=NULL);
    h.texture=SDL_CreateTexture(h.renderer,SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,POP_VIDEO_WIDTH,POP_VIDEO_HEIGHT);assert(h.texture!=NULL);
    assert(reset_game(&h));
    size_t segment=0;uint64_t boundary=pop_host_route[0].frames;
    unsigned iterations=0,rooms=0;
    while(!h.failed&&!h.victory&&!h.game_over&&iterations++<20000){
        while(h.game.ticks>=boundary&&segment+1<POP_HOST_ROUTE_COUNT){
            ++segment;boundary+=pop_host_route[segment].frames;
        }
        controls(&h,pop_host_route[segment].action);
        if(!update(&h))break;
        assert(h.game.state.aux[0xcb]>=1&&h.game.state.aux[0xcb]<=24);
        rooms|=1u<<(h.game.state.aux[0xcb]-1u);
    }
    h.quit=true;save_report(&h);
    fprintf(stdout,"{\"success\":%s,\"frames\":%llu,\"iterations\":%u,"
        "\"visited_room_mask\":%u,\"victory\":%s,\"next_level\":%u,"
        "\"health\":%u,\"sword\":%u,\"guard_health\":%u,"
        "\"input_events\":%u,\"converted_samples\":%llu,\"music_polls\":%u,"
        "\"required_helper\":%u,\"failure\":\"%s\",\"os_input_claim\":false}\n",
        h.victory&&!h.failed?"true":"false",(unsigned long long)h.game.ticks,iterations,
        rooms,h.victory?"true":"false",h.game.state.aux[0x31c],h.game.state.aux[0xce],
        h.game.state.aux[0x30a],h.game.state.aux[0xcc],h.input_events,
        (unsigned long long)h.converted_samples,h.music_polls,h.game.required_helper,
        h.failure?h.failure:"");
    if(h.audio)SDL_CloseAudioDevice(h.audio);
    free(h.samples);SDL_DestroyTexture(h.texture);
    SDL_DestroyRenderer(h.renderer);SDL_DestroyWindow(h.window);SDL_Quit();
    return h.victory&&!h.failed?0:1;
}
