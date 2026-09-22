/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_laptime.h - lap times as text for the bottom screen.
 *
 * The engine keeps a lap in two shapes, both in bss.c:
 *   * minutes / seconds / hundredths - LAP_PREVIOUS_* (the clock that runs *while* a lap is on,
 *     race___40db4h.c updates it every frame), LAP_BEST_* (best lap of this race) and LAP_RECORD_*
 *     (the record of the player's car for this track, loaded at race setup in ___33010h.c and
 *     overwritten as soon as he beats it)
 *   * a raw tick counter - D(___243cb8h) holds a *finished* lap, 70 ticks per second
 *
 * Both end up as the same "m:ss.cc" text; the tick variant uses the engine's own arithmetic
 * ((int)(1.42 * (ticks % 70))) so a time printed here is exactly the time the game itself shows.
 *
 * Plain C without any platform header, so it is unit-tested on the host - like dr3_blit.c and
 * dr3_minimap.c (see tests/test_dr3.c).
 */
#ifndef DR3_LAPTIME_H
#define DR3_LAPTIME_H

#define DR3_LAPTIME_TICKS_PER_SEC 70          /* one second of engine ticks */
#define DR3_LAPTIME_UNSET         "-:--.--"   /* "no time yet" - as wide as a real one, so columns line up */

/* m:ss.cc, e.g. 0:42.90.  Negative input is clamped to zero and hundredths/seconds run over into the
   next unit (123 hundredths = 1.23 s), so no input can produce a field the screen cannot show. */
void dr3_laptime_format(char *out, int outlen, int min, int sec, int hundredths);

/* the same for a raw tick counter, using the engine's own conversion (race___40db4h.c) */
void dr3_laptime_format_ticks(char *out, int outlen, int ticks);

/* 0 while a triple is still the engine's "nothing was driven yet" state (all three fields zero) */
int  dr3_laptime_is_set(int min, int sec, int hundredths);

/* the triple as text, or DR3_LAPTIME_UNSET when it is not set yet */
void dr3_laptime_format_or_unset(char *out, int outlen, int min, int sec, int hundredths);

#endif /* DR3_LAPTIME_H */
