/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_log.h - tiny file logger for the Nintendo 3DS build.
 *
 * Azahar is a GUI application, so the guest's stdout never reaches a console we can read.  This
 * logger appends to an absolute path on the SD card (sdmc:/drally_3ds.log), which is a normal
 * folder on the PC (%APPDATA%\azahar\sdmc) - so printf-style debugging works from the host.
 */
#ifndef DR3_LOG_H
#define DR3_LOG_H

void dr3_log(const char *fmt, ...);

/* line counter so we can see how far the engine got even if the log is huge */
int  dr3_log_lines(void);

#if defined(__3DS__) && defined(DR3_CHAOS)
/*
 * Chaos test build (-DDR3_CHAOS, dr3_chaos.h): a capped step trace for the paths outside the race loop
 * (the post race screens and the front end), so a hang or a crash shows how far the game got.
 */
void dr3_chaos_trace(const char *what, int n);

/* flushes the log - used by the heap guard report in drmemory.c */
void dr3_log_flush(void);

/*
 * Watchdog for the flickering lines above the car: the drawing helpers call this for every pixel they
 * write, and it logs the ones that land in the rows just above the player's sprite (nothing legitimate
 * draws there), with the helper's name and the value.  See gfx.c and race___4f300h.c.
 */
void dr3_chaos_watch(int slot, const char *who, int x, int y, int value);

/* frame time breakdown (see dr3_log.c): tick() before the section, time(slot, tick) after it */
unsigned long long dr3_chaos_tick(void);
void               dr3_chaos_time(int slot, unsigned long long t0);
#else
#define dr3_chaos_trace(what, n) ((void)0)
#define dr3_log_flush()          ((void)0)
#define dr3_chaos_tick()         (0ull)
#define dr3_chaos_time(slot, t0) ((void)0)
#define dr3_chaos_watch(slot, who, x, y, value) ((void)0)
#endif

#endif /* DR3_LOG_H */
