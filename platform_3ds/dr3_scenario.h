/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_scenario.h - scripted, unattended runs of the 3DS build (in Azahar or on the console).
 *
 * The build is inert unless the SD card holds  sdmc:/dr3_scenario.txt  - without the file nothing here
 * costs more than one failed fopen() at start-up.  With it, a run drives itself: keys are pressed, the
 * screen is saved as BMP, markers go into the log and the game quits - so a change can be tested without
 * anyone sitting in front of the emulator (scripts/3ds/az_run.ps1 does the launching and the reading).
 *
 * File format, one command per line ('#' starts a comment).  A trigger is either "at <frame>" (presented
 * frames) or "atms <ms>" (milliseconds since the first poll) - menus that wait for a key present no frame
 * while they wait, so the front end is driven by atms and the race by either:
 *
 *   at|atms N key NAME tap          press and release (NAME = SDL key name: Return, Escape, Up, Space ...)
 *   at|atms N key NAME down|up      hold / release (a held gas pedal for a whole race)
 *   at|atms N screenshot FILE.bmp   save the next presented top-screen frame to sdmc:/dr3_shots/FILE.bmp
 *   at|atms N log TEXT              write "[scn] TEXT" into the log
 *   at|atms N quit                  leave the game
 *   quit_after_frames N             same as  at N quit
 *   quit_after_ms N                 same as  atms N quit
 *
 * SEQUENTIAL commands (no "at"/"atms" in front) run in file order and are event driven - this is the FAST
 * way to drive the game: nothing waits a fixed time, every step goes on the moment its condition holds,
 * and every wait has a timeout (default 30 s) so a stuck screen fails the run instead of hanging it:
 *
 *   until ready [spam KEY every MS] [timeout MS]   the front end has loaded and a racer exists
 *   until event NAME [timeout MS]                  an engine event happened (dr3_scenario_event):
 *                                                  race_start, race_over
 *   until race_frames N [timeout MS]               N presented frames since race_start
 *   until frames N [timeout MS]                    N presented frames since the run started
 *   quickrace [slot 0..2] [track 0..18]            skip menus/shop/sign up, start the race now (dr3_quick.h);
 *                                                  the steps after it run INSIDE the race
 *   key NAME tap|down|up    sleep MS    screenshot FILE.bmp    log TEXT    quit
 *   bprobe start | bprobe report NAME   flicker probe of the bottom screen (blank / half drawn samples)
 *   bottom legacy on|off    the OLD bottom screen redraw behaviour, for before/after runs
 *   autopilot on|off        the engine's AI drives the PLAYER's car too (a key script cannot steer) - realistic races
 *   perf start              record every frame's period and phase times (needs the timing switch: az_run.ps1 -Perf)
 *   perf report NAME        log "[perf] NAME: ..." - percentiles, stutter frames, what the worst ones cost
 *
 * A timeout writes "[scn] FAIL: ..." and ends the run.  A script that runs out of steps quits by itself.
 * The two styles can be mixed (timed steps fire independently of the sequence).
 *
 * Every step is logged ("[scn] ...") through dr3_log_write(), which ignores the chaos_log switch, and a
 * run that ends through the script writes "[scn] done" - the runner waits for that line.
 */
#ifndef DR3_SCENARIO_H
#define DR3_SCENARIO_H

#include <stdint.h>
#include "dr3_blit.h"

#if !defined(DR3_HOST)

/* called from every poll of the event pump: loads the file once, runs what is due */
void dr3_scenario_tick(void);

/* 1 once the script asked to quit (the pump then hands the engine an SDL_QUIT) */
int  dr3_scenario_quit_requested(void);

/* 1 while a script is loaded */
int  dr3_scenario_active(void);

/* 1 while a script switched the autopilot on: the engine's own AI drives the player's car (race_main.c) */
int  dr3_scenario_autopilot(void);

/* The engine reports something a script can wait for ("until event NAME"): "race_start" when race_main
   begins, "race_over" when it ends.  Cheap, and a no-op when no script is loaded. */
void dr3_scenario_event(const char *name);

#else

/* the PC harness (DR3_HOST, which is built as __3DS__ too) has no scenario runner - it has its own: dr3_host.h - the engine's calls are no-ops there */
static inline void dr3_scenario_tick(void) {}
static inline int  dr3_scenario_quit_requested(void) { return 0; }
static inline int  dr3_scenario_active(void) { return 0; }
static inline int  dr3_scenario_autopilot(void) { return 0; }
static inline void dr3_scenario_event(const char *name) { (void)name; }

#endif /* !DR3_HOST */

/*
 * Called by dr3_fb_present() with the finished (rotated, 240 x 400) top-screen buffer, right before it is
 * flushed: counts the frame and saves the screenshot if one is due.
 *   fb      the hardware framebuffer (column-major, fb_w = 240 pixels per column, last row = screen top)
 *   fb_w    240            fb_h    400
 *   lut     the pixel format (channel shifts)
 */
#if !defined(DR3_HOST)
void dr3_scenario_present(const uint32_t *fb, int fb_w, int fb_h, const dr3_lut32_t *lut);
#endif

#endif /* DR3_SCENARIO_H */
