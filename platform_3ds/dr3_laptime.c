/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#include "dr3_laptime.h"

#include <stdio.h>

void dr3_laptime_format(char *out, int outlen, int min, int sec, int hundredths)
{
    if (!out || (outlen <= 0)) return;

    if (min < 0)        min = 0;
    if (sec < 0)        sec = 0;
    if (hundredths < 0) hundredths = 0;

    /* run over instead of printing a field the 38 character row cannot show.  The engine never feeds
       such a value (its hundredths top out at 97), the roll-over is only there so that a hand made
       value ends up as a sane time instead of "0:75.123". */
    if (hundredths >= 100) {
        sec += hundredths / 100;
        hundredths %= 100;
    }

    if (sec >= 60) {
        min += sec / 60;
        sec %= 60;
    }

    snprintf(out, (size_t)outlen, "%d:%02d.%02d", min, sec, hundredths);
}

void dr3_laptime_format_ticks(char *out, int outlen, int ticks)
{
    int n, hundredths;

    if (ticks < 0) ticks = 0;

    n = ticks / DR3_LAPTIME_TICKS_PER_SEC;

    /* exactly the engine's own arithmetic (race___40db4h.c: (int)(1.42 * (double)(ticks % 70))) so
       the time shown here is the time the game computes; IEEE doubles make that identical on the
       3DS (VFP) and on the host (SSE2).  The clamp only guards a hand made value. */
    hundredths = (int)(1.42 * (double)(ticks % DR3_LAPTIME_TICKS_PER_SEC));
    if (hundredths > 99) hundredths = 99;

    dr3_laptime_format(out, outlen, n / 60, n % 60, hundredths);
}

int dr3_laptime_is_set(int min, int sec, int hundredths)
{
    return (min != 0) || (sec != 0) || (hundredths != 0);
}

void dr3_laptime_format_or_unset(char *out, int outlen, int min, int sec, int hundredths)
{
    if (!out || (outlen <= 0)) return;

    if (!dr3_laptime_is_set(min, sec, hundredths)) {
        snprintf(out, (size_t)outlen, "%s", DR3_LAPTIME_UNSET);
        return;
    }

    dr3_laptime_format(out, outlen, min, sec, hundredths);
}
