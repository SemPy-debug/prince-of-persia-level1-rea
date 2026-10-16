/* Native SDL host. Development acceptance remains separate from finite core
 * comparisons: caller/modal behavior and original pacing are still being
 * completed. There is no emulator, Python or WSL dependency in this program. */
#include <SDL.h>
#include "game.h"
#include "game_loop.h"
#include "render_commit.h"
#include "restart.h"
#include "game_render.h"
#include "video.h"
#include "audio.h"
#include "audio_pcm.h"
#include "music.h"
#include "paused_render.h"
#include "player_render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Reference-observed mean outer interval68,243.76cycles and resource3's
 * remaining1850006-1663027cycles/45polls. Source rendering costs vary;
 * these host budgets approximate wall time and do not claim CPU execution. */
enum { SCREEN_W=560,SCREEN_H=420,CPU_HZ=1023000,DEV_TICK_CYCLES=68244,
       DEV_PAUSED_DRAW_CYCLES=4155 };
typedef struct Host {
    PopGame game;PopGameLoop loop;PopAudioPcmState pcm;PopMusic music;
    SDL_Window *window;SDL_Renderer *renderer;SDL_Texture *texture;
    SDL_AudioDeviceID audio;
    uint8_t page[8192],rgba[POP_VIDEO_RGBA_BYTES];
    int16_t *samples;size_t sample_capacity;uint32_t last_cycles;
    bool keys[SDL_NUM_SCANCODES],pressed[SDL_NUM_SCANCODES],paused,muted,quit,failed,victory;
    bool game_over,time_expired,music_started,music_final_pending;
    unsigned music_polls;
    unsigned input_events,rendered;uint64_t converted_samples,queued_samples,speaker_edges,last_report_ms;
    const char *report;const char *failure;
} Host;

static const char *glyph(char c){
    switch(c){
    case 'A':return "01110100011000111111100011000110001";
    case 'B':return "11110100011000111110100011000111110";
    case 'C':return "01111100001000010000100001000001111";
    case 'D':return "11110100011000110001100011000111110";
    case 'E':return "11111100001000011110100001000011111";
    case 'F':return "11111100001000011110100001000010000";
    case 'G':return "01111100001000010111100011000101111";
    case 'H':return "10001100011000111111100011000110001";
    case 'I':return "11111001000010000100001000010011111";
    case 'L':return "10000100001000010000100001000011111";
    case 'M':return "10001110111010110001100011000110001";
    case 'N':return "10001110011010110011100011000110001";
    case 'O':return "01110100011000110001100011000101110";
    case 'P':return "11110100011000111110100001000010000";
    case 'Q':return "01110100011000110001101011001001101";
    case 'R':return "11110100011000111110101001001010001";
    case 'S':return "01111100001000001110000010000111110";
    case 'T':return "11111001000010000100001000010000100";
    case 'U':return "10001100011000110001100011000101110";
    case 'V':return "10001100011000110001100010101000100";
    case 'W':return "10001100011000110001101011101110001";
    case 'X':return "10001100010101000100010101000110001";
    case 'Y':return "10001100010101000100001000010000100";
    case '0':return "01110100011001110101110011000101110";
    case '1':return "00100011000010000100001000010001110";
    case '2':return "01110100010000100010001000100011111";
    case '3':return "11110000010000101110000010000111110";
    case '4':return "00010001100101010010111110001000010";
    case '5':return "11111100001000011110000010000111110";
    case '6':return "01110100001000011110100011000101110";
    case '7':return "11111000010001000100010000100001000";
    case '8':return "01110100011000101110100011000101110";
    case '9':return "01110100011000101111000010000101110";
    case ':':return "00000001000010000000001000010000000";
    case '-':return "00000000000000011111000000000000000";
    default:return "00000000000000000000000000000000000";
    }
}
static void text(SDL_Renderer *r,int x,int y,const char *s){
    for(;*s;++s,x+=6){const char *g=glyph(*s);
        for(int row=0;row<7;++row)for(int col=0;col<5;++col)
            if(g[row*5+col]=='1')SDL_RenderDrawPoint(r,x+col,y+row);
    }
}
static void save_report(const Host *h){
    if(!h->report)return;
    FILE *file=fopen(h->report,"wb");if(!file)return;
    fprintf(file,"{\"status\":\"%s\",\"lifecycle\":\"%s\",\"development_only\":true,\"ticks\":%llu,\"rendered\":%u,\"input_events\":%u,"
        "\"paused\":%s,\"audio_open\":%s,\"converted_samples\":%llu,\"queued_samples\":%llu,\"speaker_edges\":%llu,"
        "\"room\":%u,\"player_x\":%u,\"player_y\":%u,\"pose\":%u,\"health\":%u,"
        "\"game_over\":%s,\"time_expired\":%s,\"music_polls\":%u,"
        "\"required_helper\":%u,\"modal_resource\":%u,\"keyboard_focus\":%s,\"window_flags\":%u,\"video_driver\":\"%s\","
        "\"failure\":\"%s\",\"pacing\":\"reference-mean68244-plus-audio;paused4155-plus-music;approximate-host-time\"}\n",
        h->failed?"incomplete-boundary":"development",h->quit?"exited":"running",(unsigned long long)h->game.ticks,h->rendered,
        h->input_events,h->paused?"true":"false",h->audio?"true":"false",
        (unsigned long long)h->converted_samples,(unsigned long long)h->queued_samples,(unsigned long long)h->speaker_edges,
        h->game.state.aux[0xcb],h->game.state.aux[0x51],h->game.state.aux[0x52],
        h->game.state.aux[0x50],h->game.state.aux[0xce],h->game_over?"true":"false",h->time_expired?"true":"false",h->music_polls,
        h->game.required_helper,h->loop.modal.active?h->loop.modal.resource:0,
        SDL_GetKeyboardFocus()==h->window?"true":"false",(unsigned)SDL_GetWindowFlags(h->window),
        SDL_GetCurrentVideoDriver()?SDL_GetCurrentVideoDriver():"unknown",
        h->failure?h->failure:"");fclose(file);
}
static void routine_report(Host *h){
    uint64_t now=SDL_GetTicks64();
    if(now-h->last_report_ms>=1000){h->last_report_ms=now;save_report(h);}
}
static void set_paused(Host *h,bool paused){
    if(h->paused==paused)return;
    h->paused=paused;
    if(h->audio)SDL_PauseAudioDevice(h->audio,paused?1:0);
}
static void fail(Host *h,const char *reason){
    h->failed=true;set_paused(h,true);h->failure=reason;
    SDL_SetWindowTitle(h->window,"Prince of Persia - Level 1 (development boundary; R restarts)");
    save_report(h);
}
static bool upload_page(Host *h){
    if(!pop_video_hgr_rgba(h->page,sizeof h->page,h->rgba,sizeof h->rgba))return false;
    return SDL_UpdateTexture(h->texture,NULL,h->rgba,POP_VIDEO_WIDTH*4)==0;
}
static bool refresh_texture(Host *h){
    return pop_game_render_view_hgr(&h->game,h->page,sizeof h->page)&&upload_page(h);
}
static bool present_transient_fill(Host *h){
    if(!h->loop.visual.low_resolution)return true;
    if(!pop_video_lores_fill_rgba(h->loop.visual.fill_byte,h->rgba,sizeof h->rgba)||
       SDL_UpdateTexture(h->texture,NULL,h->rgba,POP_VIDEO_WIDTH*4)!=0)return false;
    const SDL_Rect game={0,0,SCREEN_W,384};
    if(SDL_RenderCopy(h->renderer,h->texture,NULL,&game)!=0)return false;
    SDL_RenderPresent(h->renderer);
    /* E003's transient lores selection occurs before22DD. There is no added
     * host delay: the source22DD/2439 E006 path restores HIRES at E1D2. */
    return true;
}
static void reset_loop_metadata(Host *h){
    memset(&h->loop,0,sizeof h->loop);
    h->loop.visual.page=h->game.state.aux[0]==0?2:1;
    memset(&h->music,0,sizeof h->music);h->music_started=false;h->music_final_pending=false;
}
static bool restart_level(Host *h){
    if(!pop_game_restart(&h->game,pop_render_commit))return false;
    reset_loop_metadata(h);
    if(h->audio)SDL_ClearQueuedAudio(h->audio);
    if(!pop_audio_pcm_reset(&h->pcm,CPU_HZ,48000))return false;
    h->failed=false;h->victory=false;h->game_over=false;h->time_expired=false;
    h->failure=NULL;h->last_cycles=DEV_TICK_CYCLES;memset(h->pressed,0,sizeof h->pressed);
    SDL_SetWindowTitle(h->window,"Prince of Persia - Level 1 (development)");
    return refresh_texture(h);
}
static bool reset_game(Host *h){
    if(h->game.initialized){
        if(!pop_game_new_game(&h->game,pop_render_commit))return false;
        reset_loop_metadata(h);
    }else if(!pop_game_loop_reset(&h->loop,&h->game))return false;
    h->music_started=false;h->music_final_pending=false;
    if(!pop_audio_pcm_reset(&h->pcm,CPU_HZ,48000))return false;
    if(h->audio)SDL_ClearQueuedAudio(h->audio);
    h->failed=false;h->victory=false;h->game_over=false;h->time_expired=false;h->failure=NULL;h->last_cycles=DEV_TICK_CYCLES;
    memset(h->keys,0,sizeof h->keys);memset(h->pressed,0,sizeof h->pressed);
    SDL_SetWindowTitle(h->window,"Prince of Persia - Level 1 (development)");
    return refresh_texture(h);
}
static bool queue_audio(Host *h,const PopAudioEvents *sound,uint32_t leading_cycles){
    PopAudioEvents timed=*sound;size_t written=0;
    if(sound->duration_cycles>UINT32_MAX-leading_cycles){fail(h,"audio-duration");return false;}
    uint32_t cycles=sound->duration_cycles+leading_cycles;
    for(uint32_t i=0;i<timed.count;++i)timed.edges[i]+=leading_cycles;
    timed.duration_cycles=cycles;
    uint64_t required_samples=((uint64_t)cycles*h->pcm.sample_rate+h->pcm.phase)/h->pcm.clock_hz;
    if(required_samples>UINT32_MAX/sizeof *h->samples){fail(h,"audio-capacity");return false;}
    if(required_samples>h->sample_capacity){
        int16_t *samples=realloc(h->samples,(size_t)required_samples*sizeof *samples);
        if(!samples){fail(h,"audio-allocation");return false;}
        h->samples=samples;h->sample_capacity=(size_t)required_samples;
    }
    if(!pop_audio_pcm_render(&h->pcm,&timed,cycles,h->samples,h->sample_capacity,&written)){
        fail(h,"pcm-conversion");return false;
    }
    if(h->muted)memset(h->samples,0,written*sizeof *h->samples);
    if(h->audio&&SDL_QueueAudio(h->audio,h->samples,(Uint32)(written*sizeof *h->samples))!=0){fail(h,"audio-device");return false;}
    h->last_cycles=cycles;
    h->converted_samples+=written;if(h->audio)h->queued_samples+=written;
    h->speaker_edges+=sound->count;return true;
}
static PopGameInput sampled_game_input(const Host *h){
    bool sampled[SDL_NUM_SCANCODES];
    for(unsigned i=0;i<SDL_NUM_SCANCODES;++i)sampled[i]=h->keys[i]||h->pressed[i];
    return (PopGameInput){
        (uint8_t)((sampled[SDL_SCANCODE_RIGHT]?1:0)-(sampled[SDL_SCANCODE_LEFT]?1:0)),
        (uint8_t)((sampled[SDL_SCANCODE_DOWN]?1:0)-(sampled[SDL_SCANCODE_UP]?1:0)),
        (uint8_t)((sampled[SDL_SCANCODE_LSHIFT]||sampled[SDL_SCANCODE_RSHIFT])?128:0)};
}
static bool update(Host *h){
    if(h->loop.modal.active){
        if(h->music_final_pending){
            if(!pop_game_loop_dismiss_message(&h->loop,&h->game)){fail(h,"music-finish");return false;}
            h->music_started=false;h->music_final_pending=false;
            if(h->loop.complete){
                h->victory=true;set_paused(h,true);
                SDL_SetWindowTitle(h->window,"Prince of Persia - Level 1 complete");return true;
            }
        }else{
            PopAudioEvents sound={0};bool finished=false;
            if(!h->music_started){
                if(!pop_music_begin(&h->music,h->loop.modal.resource,&sound)){
                    fail(h,"music-resource");return false;
                }
                h->music_started=true;
                return queue_audio(h,&sound,0);
            }
            PopLoopPausedDraw item;
            if(!pop_loop_modal_torch_step(&h->loop.modal,&h->game.state,&item)||
               !pop_paused_torch_apply(&item,h->page,sizeof h->page)||!upload_page(h)){
                fail(h,"paused-torch");return false;
            }
            if((h->game.state.aux[0x203]&h->game.state.aux[0x31a])!=0){
                if(!pop_music_next(&h->music,&sound,&finished)){fail(h,"music-poll");return false;}
            }else finished=true;
            ++h->music_polls;
            if(!queue_audio(h,&sound,DEV_PAUSED_DRAW_CYCLES))return false;
            h->music_final_pending=finished;return true;
        }
    }
    const PopGameInput input=sampled_game_input(h);
    PopGameLoopResult result=pop_game_loop_before_presentation(&h->loop,&h->game,input);
    if(result==POP_LOOP_MESSAGE)return true;
    if(result==POP_LOOP_GAME_OVER||result==POP_LOOP_TIME_EXPIRED){
        h->game_over=true;h->time_expired=result==POP_LOOP_TIME_EXPIRED;set_paused(h,true);
        SDL_SetWindowTitle(h->window,h->time_expired?"Prince of Persia - Time expired":"Prince of Persia - Game over");
        save_report(h);return true;
    }
    if(result==POP_LOOP_RESTART_LEVEL){
        if(!restart_level(h)){fail(h,"restart-presentation");return false;}
        return true;
    }
    if(result!=POP_LOOP_FRAME_READY){fail(h,"gameplay-helper");return false;}
    memset(h->pressed,0,sizeof h->pressed);
    if(!present_transient_fill(h)){fail(h,"transient-display");return false;}
    uint16_t required=0;
    if(!pop_render_commit(&h->game.runtime,&h->game.state,0x22dd,&required)){
        h->game.required_helper=required;fail(h,"render-semantic-commit");return false;
    }
    h->loop.visual.low_resolution=false;h->loop.visual.fill_required=false;
    h->loop.visual.page=h->game.state.aux[0]==0?2:1;
    if(!refresh_texture(h)){fail(h,"dynamic-view");return false;}
    PopAudioEvents sound;
    if(!pop_audio_events(h->game.runtime.sounds,h->game.runtime.sound_count,h->game.state.aux[0x203]!=0,&sound)){
        fail(h,"speaker-events");return false;
    }
    if(!queue_audio(h,&sound,DEV_TICK_CYCLES+
        pop_player_render_delay_cycles(&h->game.state)))return false;
    result=pop_game_loop_after_presentation(&h->loop,&h->game);
    if(result==POP_LOOP_MESSAGE){h->music_started=false;h->music_final_pending=false;}
    else if(result==POP_LOOP_LEVEL_COMPLETE){h->victory=true;set_paused(h,true);SDL_SetWindowTitle(h->window,"Prince of Persia - Level 1 complete");}
    else if(result!=POP_LOOP_FRAME_READY){fail(h,"caller-helper");return false;}
    routine_report(h);return true;
}
static void draw(Host *h){
    SDL_SetRenderDrawColor(h->renderer,0,0,0,255);SDL_RenderClear(h->renderer);
    const SDL_Rect game={0,0,SCREEN_W,384};SDL_RenderCopy(h->renderer,h->texture,NULL,&game);
    SDL_SetRenderDrawColor(h->renderer,210,202,180,255);
    char status[96];snprintf(status,sizeof status,"ROOM %u  HEALTH %u  %s",h->game.state.aux[0xcb],h->game.state.aux[0xce],
        h->failed?"CHECKPOINT PENDING":(h->victory?"LEVEL 1 COMPLETE - R TO REPLAY":
        (h->game_over?(h->time_expired?"TIME EXPIRED - R TO REPLAY":"GAME OVER - R TO REPLAY"):
        (h->paused?"PAUSED - ENTER TO PLAY":""))));
    if(h->loop.modal.active)snprintf(status,sizeof status,h->paused?"MUSIC PAUSED - ENTER TO PLAY":"ORIGINAL MUSIC");
    text(h->renderer,8,390,status);
    text(h->renderer,8,404,"ARROWS MOVE  SHIFT ACTION  SPACE PAUSE  R RESTART  M MUTE  ESC QUIT");
    SDL_RenderPresent(h->renderer);++h->rendered;
}
static void handle_event(Host *h,const SDL_Event *event,bool smoke){
    const SDL_Event e=*event;
    if(e.type==SDL_QUIT)h->quit=true;
    if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_FOCUS_LOST&&!smoke){
        set_paused(h,true);memset(h->keys,0,sizeof h->keys);memset(h->pressed,0,sizeof h->pressed);
    }
    if(e.type==SDL_KEYDOWN||e.type==SDL_KEYUP){
        ++h->input_events;
        if(e.key.keysym.scancode>=0&&e.key.keysym.scancode<SDL_NUM_SCANCODES){
            h->keys[e.key.keysym.scancode]=e.type==SDL_KEYDOWN;
            if(e.type==SDL_KEYDOWN&&!h->paused&&!h->loop.modal.active)
                h->pressed[e.key.keysym.scancode]=true;
        }
        if(e.type==SDL_KEYDOWN&&!e.key.repeat)switch(e.key.keysym.scancode){
            case SDL_SCANCODE_ESCAPE:h->quit=true;break;
            case SDL_SCANCODE_RETURN:case SDL_SCANCODE_SPACE:
                if(!h->failed&&!h->victory&&!h->game_over){
                    set_paused(h,!h->paused);memset(h->pressed,0,sizeof h->pressed);
                }
                break;
            case SDL_SCANCODE_M:h->muted=!h->muted;break;
            case SDL_SCANCODE_R:if(!reset_game(h))fail(h,"reset-view");set_paused(h,true);break;
            default:break;
        }
    }
}
int main(int argc,char **argv){
    Host host={0};bool smoke=false,no_audio=false,smoke_down_sent=false,smoke_up_sent=false;
    unsigned smoke_frames=32;char default_report[2048];
    for(int i=1;i<argc;++i){
        if(!strcmp(argv[i],"--smoke"))smoke=true;
        else if(!strcmp(argv[i],"--no-audio"))no_audio=true;
        else if(!strcmp(argv[i],"--report")&&i+1<argc)host.report=argv[++i];
        else if(!strcmp(argv[i],"--frames")&&i+1<argc)smoke_frames=(unsigned)strtoul(argv[++i],NULL,10);
        else return 2;
    }
    if(smoke){SDL_setenv("SDL_VIDEODRIVER","dummy",1);SDL_setenv("SDL_AUDIODRIVER","dummy",1);}
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS,"permonitorv2");
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_EVENTS)!=0)return 3;
    if(!no_audio)(void)SDL_InitSubSystem(SDL_INIT_AUDIO);
    if(!host.report){char *base=SDL_GetBasePath();if(base){snprintf(default_report,sizeof default_report,"%sdev-state.json",base);host.report=default_report;SDL_free(base);}}
    host.window=SDL_CreateWindow("Prince of Persia - Level 1 (development)",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
                                 SCREEN_W*2,SCREEN_H*2,SDL_WINDOW_RESIZABLE);
    if(!host.window){SDL_Quit();return 4;}
    host.renderer=SDL_CreateRenderer(host.window,-1,smoke?SDL_RENDERER_SOFTWARE:
                                    SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!host.renderer)host.renderer=SDL_CreateRenderer(host.window,-1,SDL_RENDERER_SOFTWARE);
    if(!host.renderer){SDL_DestroyWindow(host.window);SDL_Quit();return 5;}
    SDL_RenderSetLogicalSize(host.renderer,SCREEN_W,SCREEN_H);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
    host.texture=SDL_CreateTexture(host.renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STREAMING,POP_VIDEO_WIDTH,POP_VIDEO_HEIGHT);
    SDL_AudioSpec spec={0};spec.freq=48000;spec.format=AUDIO_S16SYS;spec.channels=1;spec.samples=1024;
    if(!no_audio&&SDL_WasInit(SDL_INIT_AUDIO))host.audio=SDL_OpenAudioDevice(NULL,0,&spec,NULL,0);
    if(!host.texture||!reset_game(&host)){if(host.audio)SDL_CloseAudioDevice(host.audio);SDL_Quit();return 6;}
    host.paused=!smoke;
    if(host.audio)SDL_PauseAudioDevice(host.audio,host.paused?1:0);
    Uint64 next=SDL_GetPerformanceCounter();const Uint64 frequency=SDL_GetPerformanceFrequency();
    Uint64 remaining=0;bool was_paused=host.paused;
    while(!host.quit){
        if(smoke&&((host.game.ticks>=3&&!smoke_down_sent)||(host.game.ticks>=12&&!smoke_up_sent))){
            SDL_Event key={0};key.type=!smoke_down_sent?SDL_KEYDOWN:SDL_KEYUP;
            if(!smoke_down_sent)smoke_down_sent=true;else smoke_up_sent=true;
            key.key.keysym.scancode=SDL_SCANCODE_RIGHT;SDL_PushEvent(&key);
        }
        SDL_Event e;
        while(SDL_PollEvent(&e))handle_event(&host,&e,smoke);
        Uint64 now=SDL_GetPerformanceCounter();
        if(host.paused!=was_paused){
            if(host.paused)remaining=next>now?next-now:0;
            else next=now+remaining;
            was_paused=host.paused;
        }
        if(!host.paused&&now>=next){if(!update(&host)&&smoke)host.quit=true;next=now+(frequency*host.last_cycles)/CPU_HZ;}
        draw(&host);routine_report(&host);
        if(smoke&&host.game.ticks>=smoke_frames)host.quit=true;
        if(smoke&&host.game_over){fail(&host,"smoke-game-over-before-target");host.quit=true;}
        if(smoke&&host.victory)host.quit=true;
        SDL_Delay(4);
    }
    save_report(&host);if(host.audio)SDL_CloseAudioDevice(host.audio);free(host.samples);SDL_DestroyTexture(host.texture);
    SDL_DestroyRenderer(host.renderer);SDL_DestroyWindow(host.window);SDL_Quit();return host.failed?1:0;
}
