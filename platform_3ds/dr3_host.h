/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_host.h - the PC harness (-DDR3_HOST).
 *
 * The 3DS build can only be driven by hand: Azahar is a GUI, its screen is not scriptable and the guest's
 * stdout does not reach a console - every check of the extra cars so far meant "start the emulator, play
 * the front end, watch the screen".  This build is the same engine (DR3_CHAOS included, so it is the
 * twenty car difficulty), compiled for the PC with SDL2 and *driven by a script*:
 *
 *     dr3_host.txt     what the harness does: log switches, how many frames, key presses, when a race
 *                      starts, when a frame is dumped (see dr3_host.c for the commands)
 *     dr3_keys.txt     optional key script, one "<ms> <key> down|up" per line
 *
 * Everything the engine does is the same code as on the 3DS - the harness only pushes SDL keyboard
 * events into the same queue the real keyboard uses (dRally_Keyboard_make/break via IO_Loop), so a race
 * started by the script is the race the menu would have started.
 *
 * What it does NOT cover: the port's own framebuffer path (dr3_fb.c, libctru) and the 3DS timings.  The
 * engine side (grid, sprites, passes, standings, heap) is what this measures - see doc/3ds.md.
 *
 * Only the host project (tests/dRally3DSHost.vcxproj) defines DR3_HOST.  Every call site in the shared
 * files is guarded with "#if defined(DR3_HOST)", so the 3DS builds carry no reference to any of it.
 */
#ifndef DR3_HOST_H
#define DR3_HOST_H

#if defined(DR3_HOST)

#include <SDL.h>

/* reads dr3_host.txt / dr3_keys.txt from the current directory and sets the log switches */
void dr3_host_init(int argc, char ** argv);

/* final report to stdout + a last flush of the engine log */
void dr3_host_shutdown(void);

/* called from IO_Loop(): runs the timed part of the script (keys, quick race, F9/F10) */
void dr3_host_poll(void);

/* called once per presented engine frame (the port's __PRESENTSCREEN__) */
void dr3_host_present(SDL_Surface * screen);

/*
 * Called right after the port has converted the frame and coloured the cars of a race (drally_linux_c.c,
 * platform_3ds/dr3_blit.c) - what the PC really shows, where dr3_host_present() above sees the engine's own
 * 8-bit screen.  `px` is the 32-bit image as it went into the texture (ARGB8888).  "screenevery <ms>" (or
 * --screenevery on the command line) makes it write that image too (frames\w_%04d.bmp), so the colours of
 * a chaos race can be looked at.
 */
void dr3_host_present_final(const void * px, int w, int h, int pitch);

/* the direct race start: sets up the field the way the signup does and calls race_main */
void dr3_host_quick_race(int track_slot);

/*
 * The autopilot ("auto 1", or --auto 1 on the command line): the engine's own AI (race___4b62ch) drives
 * the player's car as well, and the input trace no longer writes his controls (race_main.c, both call
 * sites behind DR3_HOST).  Off by default - without it a run is the engine driven by the keys, exactly
 * as before.
 *
 * Why it exists: a key script cannot steer.  dr3_keys.txt holds gas/turbo/gun for the whole race, and
 * the chaos grid - the whole standings on one grid - is a queue of twenty cars behind the start line
 * that is about 750 px long on TR8, so the last place (the player's, rank 20 of 20) stands around the
 * first bend of the road, ~50 px short of a right angle corner.  With gas alone the car goes straight on
 * at that corner, leaves the road and grinds the edge of the map at the clamp of the physics (y = 20) for
 * the rest of the race - it never catches the field.  A run that has to see a wreck (the chaos kill bonus
 * needs the player to be the one who took the last HP, race___4dcach.c) needs the player to race: with
 * the autopilot he brakes, steers, rams, shoots and drops mines like every other car, and the keys of the
 * key script no longer reach the car at all.
 */
int dr3_host_autopilot(void);

/*
 * The race length of the run ("laps <n>", or --laps n on the command line; the engine asks for it in
 * ___33010h.c right before race_main).  Zero or less (the default) keeps the length the race set up.
 *
 * Why it exists: the report after the race - "Your Statistics" and the box below it, ___36adch.c - comes
 * after the last lap, and a twenty car field on a five lap race is minutes of run time.  With "laps 1" the
 * same report, the rank money and the kills of the race are a short run away.
 */
int dr3_host_laps(int asked);

#endif /* DR3_HOST */

#endif /* DR3_HOST_H */
