#include "drally.h"

#if defined(__3DS__)
#include "platform_3ds/dr3_input.h"
#include "platform_3ds/dr3_log.h"
#endif
#if defined(DR3_HOST)
#include "platform_3ds/dr3_host.h"      /* the PC harness: the timed key script (see dr3_host.c) */
#endif

extern void_cb ___2432c8h;

void dRally_Keyboard_make(SDL_Scancode);
void dRally_Keyboard_break(SDL_Scancode);

void IO_Loop(void){

    SDL_Event e;

#if defined(DR3_HOST)
    /* the harness' script runs here: it is called thousands of times a second, so a key can be pushed
       while the engine waits for one (a waiting menu does not present a frame) */
    dr3_host_poll();
#endif
#if defined(__3DS__)
    {
        static int first = 1;
        if(first){ first = 0; dr3_log("[dr3] IO_Loop first call (input/event pump alive)"); }
    }
#endif
#if defined(DR3_HOST)
    /* on the PC the harness pushes SDL keyboard events, so the real event queue is what has to be read */
    while(SDL_PollEvent(&e)){
#elif defined(__3DS__)
    /* SDL2's n3ds backend reports joystick events; dr3_poll_event() turns the pad into the
       keyboard scancodes this engine expects (see platform_3ds/dr3_input.c). */
    while(dr3_poll_event(&e)){
#else
    while(SDL_PollEvent(&e)){
#endif

        if(e.type == SDL_KEYDOWN){

            dRally_Keyboard_make(e.key.keysym.scancode);
        }
        else if(e.type == SDL_KEYUP){
           
            dRally_Keyboard_break(e.key.keysym.scancode);
        }
        else if(e.type == SDL_QUIT){
            printf("[dRally] TODO: exit not handled properly\n");
            ___2432c8h();
        }
    }
}
