/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#include "dr3_log.h"

#include <stdarg.h>
#include <stdio.h>

#define DR3_LOG_PATH "sdmc:/drally_3ds.log"
#define DR3_LOG_MAX  200000    /* stop writing after ~200k lines, the SD card does not deserve this */

static FILE *dr3_log_file;
static int   dr3_log_open_tried;
static int   dr3_log_count;

static FILE *dr3_log_handle(void)
{
    if (!dr3_log_open_tried) {
        dr3_log_open_tried = 1;
        dr3_log_file = fopen(DR3_LOG_PATH, "w");   /* fresh log per run */
    }
    return dr3_log_file;
}

void dr3_log(const char *fmt, ...)
{
    va_list ap;
    FILE   *f = dr3_log_handle();

    ++dr3_log_count;

    va_start(ap, fmt);
    if (f && dr3_log_count < DR3_LOG_MAX) {
        vfprintf(f, fmt, ap);
        fputc('\n', f);
        fflush(f);
    }
    va_end(ap);

    /* NOTE: nothing goes to stdout here on purpose.  libctru's consoleInit() makes stdout draw onto
       the bottom screen, which would overwrite the controls/standings/statistics we show there. */
}

int dr3_log_lines(void)
{
    return dr3_log_count;
}

#if defined(DR3_CHAOS)
/*
 * The chaos test build's step trace (see dr3_chaos.h): logs what the game is doing on the paths outside
 * the race loop and stops after a few thousand lines, so the tail of drally_3ds.log points at the step a
 * hang or a crash happened in.
 */
void dr3_chaos_trace(const char *what, int n)
{
    static int shown;

    if (shown >= 4000) return;

    dr3_log("[dr3] chaos step %-12s %d", what, n);
    ++shown;
}
#endif

