/* Exercise the actual SDL host event adapter; no OS input or GUI claim. */
#define main pop_host_application_main
#include "../src/windows_main.c"
#undef main
#include <assert.h>
static void key(Host *host,SDL_Scancode code,bool down){
    SDL_Event e={0};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.keysym.scancode=code;
    handle_event(host,&e,false);
}
static void neutral(const Host *host){
    PopGameInput input=sampled_game_input(host);
    assert(input.horizontal==0&&input.vertical==0&&input.action==0);
}
int main(void){
    Host h={0};h.paused=true;
    key(&h,SDL_SCANCODE_RIGHT,true);key(&h,SDL_SCANCODE_RIGHT,false);
    key(&h,SDL_SCANCODE_RETURN,true);assert(!h.paused);neutral(&h);

    h=(Host){0};h.loop.modal.active=true;
    key(&h,SDL_SCANCODE_LSHIFT,true);key(&h,SDL_SCANCODE_LSHIFT,false);
    h.loop.modal.active=false;neutral(&h);

    h=(Host){0};h.loop.modal.active=true;
    key(&h,SDL_SCANCODE_RIGHT,true);h.loop.modal.active=false;
    assert(sampled_game_input(&h).horizontal==1); /* Still held at resume. */

    h=(Host){0};key(&h,SDL_SCANCODE_RIGHT,true);key(&h,SDL_SCANCODE_RIGHT,false);
    assert(sampled_game_input(&h).horizontal==1); /* Brief active tap survives. */
    key(&h,SDL_SCANCODE_RETURN,true);assert(h.paused);neutral(&h);

    h=(Host){0};key(&h,SDL_SCANCODE_LSHIFT,true);
    SDL_Event focus={0};focus.type=SDL_WINDOWEVENT;focus.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    handle_event(&h,&focus,false);assert(h.paused);neutral(&h);

    h=(Host){0};key(&h,(SDL_Scancode)SDL_NUM_SCANCODES,true);neutral(&h);
    /* Pause must freeze queued sound, including focus-loss pauses. */
    assert(SDL_setenv("SDL_AUDIODRIVER","dummy",1)==0);
    assert(SDL_InitSubSystem(SDL_INIT_AUDIO)==0);
    SDL_AudioSpec spec={0};spec.freq=48000;spec.format=AUDIO_S16SYS;
    spec.channels=1;spec.samples=1024;
    SDL_AudioDeviceID device=SDL_OpenAudioDevice(NULL,0,&spec,NULL,0);
    assert(device!=0);h=(Host){0};h.audio=device;SDL_PauseAudioDevice(device,0);
    key(&h,SDL_SCANCODE_RETURN,true);
    assert(h.paused&&SDL_GetAudioDeviceStatus(device)==SDL_AUDIO_PAUSED);
    key(&h,SDL_SCANCODE_RETURN,true);
    assert(!h.paused&&SDL_GetAudioDeviceStatus(device)==SDL_AUDIO_PLAYING);
    handle_event(&h,&focus,false);
    assert(h.paused&&SDL_GetAudioDeviceStatus(device)==SDL_AUDIO_PAUSED);
    SDL_CloseAudioDevice(device);SDL_QuitSubSystem(SDL_INIT_AUDIO);
    puts("{\"status\":\"passed\",\"host_input_scenarios\":8,\"os_input_claim\":false}");return 0;
}
