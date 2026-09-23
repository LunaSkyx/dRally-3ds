/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#include "dr3_log.h"

#if defined(DR3_CHAOS)
#include "drmemory.h"       /* dr3_drheap_guard() - the heap check that runs with the trace */
#endif

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
#if defined(DR3_CHAOS)
        /*
         * The chaos test build traces the race car by car, which is hundreds of lines per frame: one
         * fflush per line stalls on the SD card and costs more than the frame it is tracing (72 -> 18
         * presents/s).  Flushing every 32 lines keeps the cost out and still loses at most a fraction
         * of a frame when the game hangs.
         */
        if ((dr3_log_count & 0x1f) == 0) fflush(f);
#else
        fflush(f);
#endif
    }
    va_end(ap);

#if defined(DR3_CHAOS)
    /*
     * The chaos test build's heap guard (drmemory.c) reports from inside an allocation, right before the
     * game can hang in it, so the report has to reach the SD card at once.
     */
    dr3_log_flush();
#endif

    /* NOTE: nothing goes to stdout here on purpose.  libctru's consoleInit() makes stdout draw onto
       the bottom screen, which would overwrite the controls/standings/statistics we show there. */
}

int dr3_log_lines(void)
{
    return dr3_log_count;
}

#if defined(__3DS__) && defined(DR3_CHAOS)
/* flushes what the chaos build has written so far (see the heap guard in drmemory.c) */
void dr3_log_flush(void)
{
    FILE *f = dr3_log_handle();

    if (f) fflush(f);
}
#endif

#if defined(__3DS__) && defined(DR3_CHAOS)

#include <3ds.h>

/*
 * Frame time breakdown for the chaos build: the game slows down when the frame rate drops (it drives the
 * physics with the frame counter), and Oasis with twelve cars drops to ~13 presents/s - far below what
 * the extra cars alone account for.  The call sites in race_main.c time a section with dr3_chaos_tick()
 * and hand the elapsed ticks to dr3_chaos_time(), which logs the sums once a second.
 */
#define DR3_CT_COUNT    6

static const char * const dr3_ct_name[DR3_CT_COUNT] = {
    "per-car", "collision", "render", "weapons", "map", "frame"
};

static unsigned long long dr3_ct_sum[DR3_CT_COUNT];
static unsigned long long dr3_ct_second;
static unsigned int       dr3_ct_frames;

unsigned long long dr3_chaos_tick(void)
{
    return (unsigned long long)svcGetSystemTick();
}

void dr3_chaos_time(int slot, unsigned long long t0)
{
    unsigned long long now = dr3_chaos_tick();
    int                i;

    if ((slot < 0) || (slot >= DR3_CT_COUNT)) return;

    dr3_ct_sum[slot] += now - t0;

    if (slot == 0) ++dr3_ct_frames;

    /* 268 MHz: log the breakdown every 268 million ticks (a second) */
    if ((now - dr3_ct_second) < 268000000ull) return;

    if (dr3_ct_second != 0) {

        char line[256];
        int  at = 0;

        at += snprintf(line + at, sizeof(line) - at, "[dr3] chaos frame time per second:");

        for (i = 0; i < DR3_CT_COUNT; ++i) {

            at += snprintf(line + at, sizeof(line) - at, " %s %ums", dr3_ct_name[i],
                           (unsigned)(dr3_ct_sum[i] / 268000ull));
        }

        dr3_log("%s", line);
    }

    for (i = 0; i < DR3_CT_COUNT; ++i) dr3_ct_sum[i] = 0;

    dr3_ct_frames = 0;
    dr3_ct_second = now;
}

#endif

#if defined(__3DS__) && defined(DR3_CHAOS)
/*
 * The watchdog for the flickering lines above the car.  The drawing helpers hand every pixel they write
 * to this, and it logs - a few lines per caller - whatever lands in the rows just above the player's
 * sprite: nothing legitimate draws there, so the log names the layer that paints over the car.
 *
 * s_35e's screen position lives at +4 (x) and +8 (y) of each 0x35e byte entry (___1e6ed0h).
 */
void dr3_chaos_watch(int slot, const char *who, int x, int y, int value){

    extern __BYTE__  ___1e6ed0h[];
    extern unsigned  MY_CAR_IDX;

    static int shown[8];

    const unsigned char * car;
    int                   cx, cy;

    if ((slot < 0) || (slot >= 8)) return;
    if (shown[slot] >= 6) return;

    car = ___1e6ed0h + 0x35e * (int)MY_CAR_IDX;

    cx =  (int)(unsigned)(car[4] | (car[5] << 8) | (car[6] << 16) | ((unsigned)car[7] << 24)) + 76;
    cy = ((int)(unsigned)(car[8] | (car[9] << 8) | (car[10] << 16) | ((unsigned)car[11] << 24))) - 20;

    if ((x < cx - 4) || (x > cx + 44)) return;      /* the car's width  */
    if ((y < cy - 14) || (y >= cy)) return;         /* the rows above it */

    ++shown[slot];
    dr3_log("[dr3] chaos: WATCH %s wrote %02x at %d,%d (car sprite top left %d,%d)",
            who, value & 0xff, x, y, cx, cy);
    dr3_log_flush();
}
#endif

#if defined(DR3_CHAOS)
/*
 * The chaos test build's step trace (see dr3_chaos.h): logs what the game is doing on the paths outside
 * the race loop and stops after a few thousand lines, so the tail of drally_3ds.log points at the step a
 * hang or a crash happened in.
 */
void dr3_chaos_trace(const char *what, int n)
{
    static int shown;

    /* a trace point is a cheap place to look at the engine's heap (see drmemory.c) */
    if (shown < 4000) dr3_drheap_guard(what);

    if (shown >= 4000) return;

    dr3_log("[dr3] chaos step %-12s %d", what, n);
    dr3_log_flush();            /* few calls only - the tail of the log has to be the last step */
    ++shown;
}
#endif

