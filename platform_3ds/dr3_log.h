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
#else
#define dr3_chaos_trace(what, n) ((void)0)
#define dr3_log_flush()          ((void)0)
#endif

#endif /* DR3_LOG_H */
