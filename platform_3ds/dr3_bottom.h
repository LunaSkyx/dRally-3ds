/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_bottom.h - the bottom screen of the 3DS shows the controls (left) and the current driver
 * standings (right), in the style of the Death Rally front end.  While a race is running that block
 * gives way to a minimap of the current track, and there a tap only switches between the map and dark
 * (no standings during a race) - see dr3_minimap.c.
 *
 * The game never draws to the bottom screen, so it is free for this.  The console needs the gfx state
 * that SDL's n3ds video driver creates with gfxInit(), so it is initialised lazily (doing it earlier
 * jumps into a NULL pointer inside libctru).  The profiler build shares this code: it prints its
 * statistics and then appends the same block.
 */
#ifndef DR3_BOTTOM_H
#define DR3_BOTTOM_H

#define DR3_BOTTOM_LINE_LEN 64
#define DR3_BOTTOM_LINES    26          /* controls + standings need at most this many rows */

/* Initialises the bottom screen console once gfx is up.  Returns 1 when it is usable, 0 to retry
   later.  Safe to call from both the game and the profiler. */
int  dr3_bottom_console_ensure(void);

/* Fills "out" with the controls/standings block and returns the number of lines written. */
int  dr3_bottom_build_lines(char out[DR3_BOTTOM_LINES][DR3_BOTTOM_LINE_LEN]);

/* Tapping the bottom screen switches the information off and on again: while a track is loaded the
   two states are the minimap and dark, outside a race the controls/standings block and dark.
   dr3_bottom_touch() reacts to every tap, dr3_bottom_touch_ex() ignores taps whose Y coordinate is
   inside [ignore_y0, ignore_y1) - the profiler build uses that for its audio rate buttons. */
void dr3_bottom_touch(void);
void dr3_bottom_touch_ex(int ignore_y0, int ignore_y1);

/* 1 while the player switched the information off. */
int  dr3_bottom_is_hidden(void);

/* Standalone bottom screen (release build): redraws at most once a second. */
void dr3_bottom_update(void);

/* Flush + swap the bottom screen after printing. */
void dr3_bottom_flush(void);

/*
 * Minimap hooks.  The race code hands over the track mask as soon as a track is decoded
 * (race___42824h.c) and drops it again when the track memory is freed (race_memory.c), so the port
 * never keeps a pointer that the engine has released.  Outside the 3DS release build - and in the
 * profiler build, which owns the bottom screen itself - both calls compile to nothing, so the engine
 * code needs no #if of its own.
 */
#if defined(__3DS__) && defined(DR3_USE_GFX) && !defined(DR3_PROFILE)

void dr3_bottom_track_loaded(const void *mask, const void *image, const void *palette,
                             int mask_w, int mask_h, const char *track_id, const char *track_name);
void dr3_bottom_track_unloaded(void);

#else

/* the no-op variants reference their arguments, exactly like the ones in dr3_prof.h, so a caller
   that computes something for the call (e.g. the track name) does not warn about dead stores */
#define dr3_bottom_track_loaded(mask, image, palette, mask_w, mask_h, track_id, track_name) \
    ((void)(mask), (void)(image), (void)(palette), (void)(mask_w), (void)(mask_h),          \
     (void)(track_id), (void)(track_name))
#define dr3_bottom_track_unloaded() ((void)0)

#endif /* 3DS release build */

#endif /* DR3_BOTTOM_H */

