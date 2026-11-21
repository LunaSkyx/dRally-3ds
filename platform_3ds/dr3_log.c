/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#include "dr3_log.h"
/*
 * drally.h carries the difficulty macros (dr3_chaos_active, DR3_DIFFICULTY_CHAOS) and the engine
 * globals behind them.  It includes *this* header itself, and the guard in dr3_log.h keeps that from
 * recursing; without it the extra car switch could not see which level the player picked.
 */
#include "drally.h"

#if defined(DR3_CHAOS)
#include "drmemory.h"       /* dr3_drheap_guard() - the heap check that runs with the trace */
#include "dr3_chaos.h"      /* DR3_CHAOS_EXTRA, DR3_CHAOS_EXTRA_DEFAULT - the extra car switch */
#include "dr3_grid.h"       /* DR3_CHAOS_BOSS_CHANCE_DEFAULT */
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(DR3_HOST)
/*
 * The PC harness (-DDR3_HOST, dr3_host.h) has no memory card: what the 3DS writes to sdmc:/ lives in the
 * directory the harness runs in.  The names stay the same, so a run is read exactly like a 3DS one.
 */
#define DR3_LOG_PATH          "drally_host.log"
#else
#define DR3_LOG_PATH          "sdmc:/drally_3ds.log"
#endif
#define DR3_LOG_MAX  200000    /* stop writing after ~200k lines, the SD card does not deserve this */

#if defined(DR3_CHAOS)
/*
 * The log switch (dr3_log.h): sdmc:/chaos_log.txt holds one number, 0 silences everything and 1 turns it
 * back on.  It is 0 unless that file says otherwise, read before the first line is written and again at
 * every race setup - so the full diagnostics (frame dumps, stripe checksums, band measurement, step trace)
 * are one file edit away, no rebuild needed.
 */
#if defined(DR3_HOST)
#define DR3_LOG_SWITCH_PATH   "chaos_log.txt"
#define DR3_TIME_SWITCH_PATH  "chaos_time.txt"
#else
#define DR3_LOG_SWITCH_PATH   "sdmc:/chaos_log.txt"
#define DR3_TIME_SWITCH_PATH  "sdmc:/chaos_time.txt"
#endif

static int dr3_log_on;
static int dr3_time_on;
static int dr3_log_switch_read;
#endif

static FILE *dr3_log_file;
static int   dr3_log_open_tried;
static int   dr3_log_count;

#if defined(DR3_CHAOS)
void dr3_log_quiet_reload(void)
{
    FILE *f  = fopen(DR3_LOG_SWITCH_PATH, "r");
    int   on = 0;
    int   tm = 0;

    if (f) {
        if (fscanf(f, "%d", &on) != 1) on = 0;      /* a missing or unreadable file means: silent */
        fclose(f);
    }

    f = fopen(DR3_TIME_SWITCH_PATH, "r");
    if (f) {
        if (fscanf(f, "%d", &tm) != 1) tm = 0;
        fclose(f);
    }

    dr3_log_on           = (on != 0);
    dr3_time_on          = (tm != 0);
    dr3_log_switch_read  = 1;
}

int dr3_log_enabled(void)
{
    if (!dr3_log_switch_read) dr3_log_quiet_reload();       /* so the very first line already obeys it */

    return dr3_log_on;
}

/* the timing switch (see dr3_log.h): the frame time breakdown and nothing else */
int dr3_log_timing(void)
{
    if (!dr3_log_switch_read) dr3_log_quiet_reload();

    return dr3_time_on;
}
#endif

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
    FILE   *f;

#if defined(DR3_CHAOS)
    if (!dr3_log_enabled()) return;     /* the switch: nothing is written and the file is not even opened */
#endif

    f = dr3_log_handle();

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

    /* NOTE: nothing goes to stdout here on purpose.  libctru's consoleInit() makes stdout draw onto
       the bottom screen, which would overwrite the controls/standings/statistics we show there. */
}

int dr3_log_lines(void)
{
    return dr3_log_count;
}

/*
 * Writes a line although the main log switch is off: the frame time breakdown and the port's profiler
 * use this, so a timing run does not drag the band measurement, the watchdog and the frame dumps in -
 * they all go through dr3_log() and stay silent.  The file is opened by the first call, so a timing run
 * gets a fresh log.
 */
void dr3_log_write(const char *fmt, ...)
{
    va_list ap;
    FILE   *f = dr3_log_handle();

    if (!f) return;

    ++dr3_log_count;

    va_start(ap, fmt);
    if (dr3_log_count < DR3_LOG_MAX) {
        vfprintf(f, fmt, ap);
        fputc('\n', f);
        fflush(f);                      /* a handful of lines per second - this one is cheap */
    }
    va_end(ap);
}

#if (defined(__3DS__) || defined(DR3_HOST)) && defined(DR3_CHAOS)
/* flushes what the chaos build has written so far (see the heap guard in drmemory.c) */
void dr3_log_flush(void)
{
    FILE *f = dr3_log_handle();

    if (f) fflush(f);
}
#endif

#if (defined(__3DS__) || defined(DR3_HOST)) && defined(DR3_CHAOS)

#if defined(DR3_HOST)
#include <SDL.h>
#else
#include <3ds.h>
#endif

/*
 * Frame time breakdown for the chaos build: the game slows down when the frame rate drops (it drives the
 * physics with the frame counter), and Oasis with twelve cars drops to ~13 presents/s - far below what
 * the extra cars alone account for.  The call sites in race_main.c time a section with dr3_chaos_tick()
 * and hand the elapsed ticks to dr3_chaos_time(), which logs the sums once a second.
 *
 * Switched by sdmc:/chaos_time.txt (see dr3_log.h): the breakdown is written even while the main log
 * switch is off, and the expensive diagnostics - which would slow the frame down while it is being
 * measured - are not started at all.  The phases themselves are DR3_CT_* in dr3_log.h.
 */
#define DR3_CT_COUNT    33

static const char * const dr3_ct_name[DR3_CT_COUNT] = {
    "percar", "coll", "skids", "tcopy", "spect", "dust", "shadow", "cars", "expl", "weap",
    "objsort", "objdraw", "hud", "present", "FRAME",
    "pc-sprite", "pc-phys", "pc-skid", "pc-a", "pc-b", "pc-offroad",
    "copy", "maskcopy", "blit", "tint", "flush", "swap",
    "pre", "wait",
    "tint-scan", "tint-shades", "tint-mask", "tint-hp"
};

static unsigned long long dr3_ct_sum[DR3_CT_COUNT];
static unsigned long long dr3_ct_second;
static unsigned int       dr3_ct_frames;

/*
 * The per frame recorder behind "perf start" / "perf report" of the scenario harness (dr3_scenario.h).
 * The one second sums above hide a micro stutter - one slow frame in sixty is invisible in an average.
 * While it is on, every phase's time is also summed *per frame* (in microseconds) and a frame is closed at
 * DR3_CT_FRAME; its period (the time since the previous frame ended) is kept next to it.  The report names
 * the percentiles, the stutter frames and, for the worst of them, the phases that cost more than usual.
 */
#define DR3_PF_MAX     2048
#define DR3_PF_OTHER   DR3_CT_COUNT          /* pseudo slot: period - (pre + FRAME + wait): ISR, audio, I/O, the log */
#define DR3_PF_SLOTS   (DR3_CT_COUNT + 1)

static uint32_t           dr3_pf_slot[DR3_PF_MAX][DR3_PF_SLOTS];
static uint32_t           dr3_pf_period[DR3_PF_MAX];
static uint32_t           dr3_pf_cur[DR3_CT_COUNT];
static int                dr3_pf_n;
static int                dr3_pf_on;
static unsigned long long dr3_pf_last;

unsigned long long dr3_chaos_tick(void)
{
#if defined(DR3_HOST)
    /*
     * The harness measures with SDL's own clock.  The values are scaled to the 3DS' tick rate (268 MHz),
     * so the "one second" the report below sums over means the same in both builds and the numbers are
     * read the same way.
     */
    static unsigned long long dr3_host_freq;

    if (!dr3_host_freq) dr3_host_freq = (unsigned long long)SDL_GetPerformanceFrequency();

    return (unsigned long long)SDL_GetPerformanceCounter() * 268000000ull /
           ((dr3_host_freq != 0) ? dr3_host_freq : 1);
#else
    return (unsigned long long)svcGetSystemTick();
#endif
}

void dr3_chaos_time(int slot, unsigned long long t0)
{
    unsigned long long now = dr3_chaos_tick();
    int                i;

    /* the timing switch (chaos_time.txt) or the main log switch: neither on means no measuring */
    if (!dr3_log_enabled() && !dr3_log_timing()) return;
    if ((slot < 0) || (slot >= DR3_CT_COUNT)) return;

    dr3_ct_sum[slot] += now - t0;

    if (dr3_pf_on) {

        dr3_pf_cur[slot] += (uint32_t)((now - t0) / 268ull);        /* 268 ticks per microsecond */

        if (slot == DR3_CT_FRAME) {

            if ((dr3_pf_n < DR3_PF_MAX) && (dr3_pf_last != 0)) {

                const uint32_t period = (uint32_t)((now - dr3_pf_last) / 268ull);
                const uint32_t known  = dr3_pf_cur[DR3_CT_PRE] + dr3_pf_cur[DR3_CT_FRAME] + dr3_pf_cur[DR3_CT_WAIT];
                int            k;

                for (k = 0; k < DR3_CT_COUNT; ++k) dr3_pf_slot[dr3_pf_n][k] = dr3_pf_cur[k];
                dr3_pf_slot[dr3_pf_n][DR3_PF_OTHER] = (period > known) ? (period - known) : 0;
                dr3_pf_period[dr3_pf_n] = period;
                ++dr3_pf_n;
            }
            dr3_pf_last = now;
            memset(dr3_pf_cur, 0, sizeof(dr3_pf_cur));
        }
    }

    if (slot == DR3_CT_FRAME) ++dr3_ct_frames;

    /* 268 MHz: log the breakdown every 268 million ticks (a second) */
    if ((now - dr3_ct_second) < 268000000ull) return;

    if (dr3_ct_second != 0) {

        char line[512];
        int  at, i2;

        /*
         * Two lines: the top level phases first (what the frame is made of) and then the detail of the
         * two biggest suspects - the six per car loops and every step of the race screen's present.
         * The slots overlap (see dr3_log.h), so each number is read on its own.
         */
        at = 0;
        at += snprintf(line + at, sizeof(line) - at, "[dr3] chaos frame time:");

        for (i = 0; i <= DR3_CT_FRAME; ++i) {

            at += snprintf(line + at, sizeof(line) - at, " %s %ums", dr3_ct_name[i],
                           (unsigned)(dr3_ct_sum[i] / 268000ull));
        }

        at += snprintf(line + at, sizeof(line) - at,
                       " | %u frame(s), %.1f ms per frame, render part %ums",
                       dr3_ct_frames,
                       1000.0 / (double)((dr3_ct_frames != 0) ? dr3_ct_frames : 1),
                       (unsigned)(dr3_ct_sum[DR3_CT_FRAME] / 268000ull));

        dr3_log_write("%s", line);

        at = 0;
        at += snprintf(line + at, sizeof(line) - at, "[dr3] chaos frame detail:");

        for (i2 = DR3_CT_PC_SPRITE; i2 < DR3_CT_COUNT; ++i2) {

            at += snprintf(line + at, sizeof(line) - at, " %s %ums", dr3_ct_name[i2],
                           (unsigned)(dr3_ct_sum[i2] / 268000ull));
        }

        dr3_log_write("%s", line);
    }

    for (i = 0; i < DR3_CT_COUNT; ++i) dr3_ct_sum[i] = 0;

    dr3_ct_frames = 0;
    dr3_ct_second = now;
}

void dr3_chaos_perf_start(void)
{
    dr3_pf_n    = 0;
    dr3_pf_last = 0;
    memset(dr3_pf_cur, 0, sizeof(dr3_pf_cur));
    dr3_pf_on   = 1;
    dr3_log_write("[perf] recording per frame (up to %d frames)", DR3_PF_MAX);
}

static int dr3_pf_cmp(const void *a, const void *b)
{
    const uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;

    return (x > y) - (x < y);
}

static uint32_t dr3_pf_pct(const uint32_t *sorted, int n, int pct)
{
    int i = (n * pct) / 100;

    if (i >= n) i = n - 1;
    return sorted[i];
}

static const char *dr3_pf_name(int slot) { return (slot == DR3_PF_OTHER) ? "other" : dr3_ct_name[slot]; }

void dr3_chaos_perf_report(const char *label)
{
    static uint32_t col[DR3_PF_MAX];
    static uint32_t med[DR3_PF_SLOTS], p95[DR3_PF_SLOTS], avg[DR3_PF_SLOTS];
    static uint32_t sorted[DR3_PF_MAX];
    static unsigned char taken[DR3_PF_MAX];
    const int       n = dr3_pf_n;
    int             i, k, shown;
    uint32_t        p50, thresh;
    unsigned long long total = 0;
    char            line[480];
    int             at, stutters = 0;

    dr3_pf_on = 0;

    if (n < 10) { dr3_log_write("[perf] %s: only %d frame(s) recorded - nothing to report", label, n); return; }

    for (i = 0; i < n; ++i) { sorted[i] = dr3_pf_period[i]; total += dr3_pf_period[i]; }
    qsort(sorted, (size_t)n, sizeof(uint32_t), dr3_pf_cmp);
    p50 = dr3_pf_pct(sorted, n, 50);

    dr3_log_write("[perf] %s: %d frames, avg %.2f ms (%.1f fps), p50 %.2f, p95 %.2f, p99 %.2f, max %.2f ms",
                  label, n, (double)total / n / 1000.0, 1000000.0 * n / (double)total,
                  p50 / 1000.0, dr3_pf_pct(sorted, n, 95) / 1000.0, dr3_pf_pct(sorted, n, 99) / 1000.0, sorted[n - 1] / 1000.0);

    /* a stutter: clearly slower than the usual frame (1.5x the median and at least 4 ms more) */
    thresh = p50 + ((p50 / 2) > 4000u ? (p50 / 2) : 4000u);
    for (i = 0; i < n; ++i) if (dr3_pf_period[i] > thresh) ++stutters;
    dr3_log_write("[perf] %s: %d stutter frame(s) over %.1f ms (%.1f%% of the frames)", label, stutters,
                  thresh / 1000.0, 100.0 * stutters / n);

    /* the frame indices of the stutters: a constant gap means something periodic (a timer, a log flush) */
    at = snprintf(line, sizeof(line), "[perf] %s: stutter frames at:", label);
    shown = 0;
    for (i = 0; (i < n) && (shown < 40); ++i)
        if (dr3_pf_period[i] > thresh) { at += snprintf(line + at, sizeof(line) - at, " %d(%.0fms)", i, dr3_pf_period[i] / 1000.0); ++shown; }
    if (shown) dr3_log_write("%s", line);

    /* what a frame is made of: per phase, the average, the median and the p95 */
    for (k = 0; k < DR3_PF_SLOTS; ++k) {
        unsigned long long sum = 0;

        for (i = 0; i < n; ++i) { col[i] = dr3_pf_slot[i][k]; sum += col[i]; }
        avg[k] = (uint32_t)(sum / (unsigned long long)n);
        qsort(col, (size_t)n, sizeof(uint32_t), dr3_pf_cmp);
        med[k] = dr3_pf_pct(col, n, 50);
        p95[k] = dr3_pf_pct(col, n, 95);
    }

    at = snprintf(line, sizeof(line), "[perf] %s: ms/frame avg (p95) -", label);
    {
        int order[DR3_PF_SLOTS], j;

        for (k = 0; k < DR3_PF_SLOTS; ++k) order[k] = k;
        for (k = 0; k < DR3_PF_SLOTS; ++k)                              /* by average, biggest first */
            for (j = k + 1; j < DR3_PF_SLOTS; ++j)
                if (avg[order[j]] > avg[order[k]]) { int t = order[k]; order[k] = order[j]; order[j] = t; }
        for (k = 0; k < DR3_PF_SLOTS; ++k) {
            if (avg[order[k]] < 200) break;                             /* under 0.2 ms: not worth a column */
            at += snprintf(line + at, sizeof(line) - at, " %s %.2f(%.2f)", dr3_pf_name(order[k]),
                           avg[order[k]] / 1000.0, p95[order[k]] / 1000.0);
            if (at > (int)sizeof(line) - 40) break;
        }
        dr3_log_write("%s", line);
    }

    /* the worst frames and what cost more than usual in each (the slots overlap - read the biggest few) */
    memset(taken, 0, sizeof(taken));
    for (shown = 0; shown < 6; ++shown) {

        int worst = -1;

        for (i = 0; i < n; ++i)
            if (!taken[i] && ((worst < 0) || (dr3_pf_period[i] > dr3_pf_period[worst]))) worst = i;
        if (worst < 0) break;
        taken[worst] = 1;

        {
            int      top[3] = { -1, -1, -1 }, j;
            int32_t  ex[3]  = { 0, 0, 0 };

            for (k = 0; k < DR3_PF_SLOTS; ++k) {
                const int32_t e = (int32_t)dr3_pf_slot[worst][k] - (int32_t)med[k];

                for (j = 0; j < 3; ++j)
                    if ((top[j] < 0) || (e > ex[j])) {
                        int m;
                        for (m = 2; m > j; --m) { top[m] = top[m - 1]; ex[m] = ex[m - 1]; }
                        top[j] = k; ex[j] = e;
                        break;
                    }
            }
            dr3_log_write("[perf] %s: worst #%d = frame %d, period %.1f ms (median %.1f) <- %s +%.1f, %s +%.1f, %s +%.1f ms",
                          label, shown + 1, worst, dr3_pf_period[worst] / 1000.0, p50 / 1000.0,
                          dr3_pf_name(top[0]), ex[0] / 1000.0, dr3_pf_name(top[1]), ex[1] / 1000.0,
                          dr3_pf_name(top[2]), ex[2] / 1000.0);
        }
    }
}

#endif

#if (defined(__3DS__) || defined(DR3_HOST)) && defined(DR3_CHAOS)
/*
 * The watchdog and the band measurement for the flickering lines above the car.
 *
 * The drawing helpers hand every pixel they write to the watchdog, which logs - a handful of lines per
 * caller, refreshed every 64 frames - whatever lands in the rows just above the player's sprite: nothing
 * legitimate draws there, so the log names the layer that paints over the car.
 *
 * s_35e's screen position lives at +4 (x) and +8 (y) of each 0x35e byte entry (___1e6ed0h).
 */
#define DR3_CAR_BAND_X  (-4)        /* left edge relative to the sprite's own left edge */
#define DR3_CAR_BAND_W  48          /* the sprite is 40 wide */
#define DR3_CAR_BAND_H  20          /* rows above the sprite */
#define DR3_CAR_BAND_IN 4           /*   and the sprite's own first rows */
#define DR3_BAND_ROWS   (DR3_CAR_BAND_H + DR3_CAR_BAND_IN)

/*
 * The three fillers in gfx.c paint into &BACKBUFFER[0x60] and take the viewport's own x, while the car
 * sprite is drawn at BACKBUFFER + (__4 + 76) without that offset (race___4f300h.c).  A write the filler
 * reports at x therefore lands at x + this in the back buffer - the offset has to be added before it is
 * compared with the car, or the watchdog watches a window 96 pixels to the right of the car.
 */
#define DR3_CAR_BAND_VP 0x60

static int         dr3_watch_hits[8];   /* lines per caller, reset every 64 frames */
static int         dr3_watch_frames;

static void dr3_car_screen(int *px, int *py)
{
    extern __BYTE__ ___1e6ed0h[];
    extern unsigned  MY_CAR_IDX;

    const unsigned char *car = ___1e6ed0h + 0x35e * (int)MY_CAR_IDX;

    /* both are back buffer pixels: +76 is the sprite's left edge, -20 its top edge (helper01) */
    *px =  (int)(unsigned)(car[4] | (car[5] << 8) | (car[6] << 16) | ((unsigned)car[7] << 24)) + 76;
    *py = ((int)(unsigned)(car[8] | (car[9] << 8) | (car[10] << 16) | ((unsigned)car[11] << 24))) - 20;
}

void dr3_chaos_watch(int slot, const char *who, int x, int y, int value){

    int cx, cy, bx;

    if (!dr3_log_enabled()) return;     /* the switch: the watchdog costs nothing while the log is off */
    if ((slot < 0) || (slot >= 8)) return;
    if (dr3_watch_hits[slot] >= 4) return;

    dr3_car_screen(&cx, &cy);

    bx = x + DR3_CAR_BAND_VP;       /* the fillers' viewport x -> back buffer x */

    if ((bx < cx + DR3_CAR_BAND_X) || (bx >= cx + DR3_CAR_BAND_X + DR3_CAR_BAND_W)) return;
    if ((y < cy - DR3_CAR_BAND_H) || (y >= cy)) return;

    ++dr3_watch_hits[slot];
    dr3_log("[dr3] chaos: WATCH %s wrote %02x at %d,%d (car sprite top left %d,%d)",
            who, value & 0xff, bx, y, cx, cy);
    dr3_log_flush();
}

/*
 * The remap filler (___5e769h, gfx.c) is called by the track shadows, the object shadows and the car
 * lights; each of them names itself here first, so the watchdog's "remap" line says which one wrote.
 */
static const char *dr3_remap_who = "?";

void dr3_chaos_remap_who(const char *who)
{
    dr3_remap_who = who;
}

const char *dr3_chaos_remap_who_get(void)
{
    return dr3_remap_who;
}

/*
 * The pass switch (dr3_chaos.h): which drawing pass of the frame is switched off right now.  It takes effect
 * at once - the pass is simply not called any more - so the grey lines can be watched while pressing.
 */
static const char * const dr3_pass_name[DR3_PASS_COUNT + 1] = {
    "nothing (every pass is drawn)",
    "4f300h player sprite (your own car)",
    "4f300h player headlight beam (your own)",
    "4f300h other cars' headlight beams",
    "4f300h the whole car pass",
    "53464h objects",
    "4f170h track shadows",
    "50ef4h tyre dust",
    "52d7ch spectators",
    "51ce0h explosions",
    "50a48h gun fire",
    "51204h rocket fuel",
    "50ba4h shots",
    "51eb4h cash text",
    "42218h banner",
    "hud"
};

static int dr3_pass_skip = DR3_PASS_NONE;

int dr3_chaos_skip_pass(int which)
{
    return (which != DR3_PASS_NONE) && (which == dr3_pass_skip);
}

int dr3_chaos_skip_cycle(void)
{
    dr3_pass_skip = (dr3_pass_skip >= DR3_PASS_COUNT) ? DR3_PASS_NONE : (dr3_pass_skip + 1);

    if (dr3_pass_skip == DR3_PASS_NONE)
        dr3_log("[dr3] chaos: PASS SWITCH: every pass is drawn again (press again to switch off %s)",
                dr3_pass_name[1]);
    else
        dr3_log("[dr3] chaos: PASS SWITCH: %s is switched OFF now - %s (press again to switch off %s)",
                dr3_pass_name[dr3_pass_skip],
                (dr3_pass_skip >= DR3_PASS_COUNT) ? "that was the last one" : "look at the lines while racing",
                dr3_pass_name[(dr3_pass_skip >= DR3_PASS_COUNT) ? 0 : (dr3_pass_skip + 1)]);

    return dr3_pass_skip;
}

/*
 * The extra car ladder (dr3_chaos.h): how much of an extra car is drawn.  It lives next to the extra car
 * switch because both are the same idea - flip one thing while the game runs, race, look.
 */
static int dr3_extra_stage = DR3_EXTRA_STAGE_ALL;

int dr3_chaos_extra_stage(void)
{
    return dr3_extra_stage;
}

int dr3_chaos_extra_draw(void)
{
    return dr3_extra_stage >= DR3_EXTRA_STAGE_SPRITE;
}

int dr3_chaos_extra_lights(void)
{
    return dr3_extra_stage >= DR3_EXTRA_STAGE_LIGHTS;
}

int dr3_chaos_extra_effects(void)
{
    return dr3_extra_stage >= DR3_EXTRA_STAGE_ALL;
}

/*
 * The weapon and tyre passes are per car loops in race_main.c.  While the ladder sits below the top step
 * they stop after the four cars of the original race, so an extra car's shots, flames, rocket and tyre
 * smoke are not drawn at all - that is one whole family of layers in one switch.
 */
int dr3_chaos_extra_pass_limit(int cars)
{
    if (dr3_chaos_extra_effects()) return cars;

    return (cars < DR3_HUD_CARS) ? cars : DR3_HUD_CARS;
}

int dr3_chaos_extra_stage_cycle(void)
{
    static const char * const what[DR3_EXTRA_STAGE_ALL + 1] = {
        "the extra cars are invisible (in the race, but nothing of them is drawn)",
        "the extra cars draw their sprite (no lights, no weapon or tyre effects)",
        "the extra cars draw their sprite and lights (no weapon or tyre effects)",
        "the extra cars draw everything (sprite, lights, weapon and tyre effects)"
    };

    dr3_extra_stage = (dr3_extra_stage >= DR3_EXTRA_STAGE_ALL) ? 0 : (dr3_extra_stage + 1);

    dr3_log("[dr3] chaos: EXTRA CAR STAGE %d/%d - %s (takes effect at once; the next press gives stage %d)",
            dr3_extra_stage, DR3_EXTRA_STAGE_ALL, what[dr3_extra_stage],
            (dr3_extra_stage >= DR3_EXTRA_STAGE_ALL) ? 0 : (dr3_extra_stage + 1));

    return dr3_extra_stage;
}

/*
 * The extra car switch (dr3_chaos.h).  It lives with the chaos build's other runtime helpers - the
 * platform_3ds/dr3_log.c object is linked into every 3DS build, so no new file (and no Makefile entry)
 * is needed, and the release build never references any of it.
 *
 * The dark side of the switch: it says how many cars the *next* race adds on top of the ones the game
 * asked for.  The grid is placed when a race starts (___33010h.c fills the entries, race___42824h.c
 * puts them on the road, race_main.c sizes the race), so the running race keeps its cars.
 */
static int dr3_extra_cars = DR3_CHAOS_EXTRA_DEFAULT;

/* the switch also lives in a file, so it can be flipped from the PC (DR3_EXTRA_CARS_PATH) */
#if defined(DR3_HOST)
#define DR3_EXTRA_CARS_PATH "chaos_extras.txt"
#else
#define DR3_EXTRA_CARS_PATH "sdmc:/chaos_extras.txt"
#endif

static void dr3_extra_cars_put(void);

static int dr3_extra_cars_overridden = 0;

int dr3_chaos_extra_cars_override(void)
{
    return dr3_extra_cars_overridden;
}

int dr3_chaos_extra_cars(void)
{
    /*
     * The difficulty decides whether the extra cars race at all: only the "Chaos" level puts them on
     * the road (drally.h: dr3_chaos_active - the player picks it in the licence screen).  Every other
     * level runs exactly as the original does.  The switch below (chaos_extras.txt) only tunes how
     * many of them there are, so the twenty car test build still works.
     */
    if (!dr3_chaos_active()) return 0;

    return dr3_extra_cars;
}

void dr3_chaos_extra_cars_set(int cars)
{
    if (cars < 0) cars = 0;
    if (cars > DR3_CHAOS_EXTRA) cars = DR3_CHAOS_EXTRA;

    dr3_extra_cars = cars;
    dr3_extra_cars_overridden = 1;
}

int dr3_chaos_extra_cars_toggle(void)
{
    /*
     * One more each time it is pressed, wrapping around at DR3_CHAOS_EXTRA: 0 -> 1 -> 2 ... -> 8 -> 0.
     * That is the bisect: press once, race, see whether the fault is there, press again.  The log says
     * what the next press gives and where the file is, so the count never has to be guessed.
     */
    dr3_chaos_extra_cars_set((dr3_extra_cars >= DR3_CHAOS_EXTRA) ? 0 : (dr3_extra_cars + 1));

    dr3_log("[dr3] chaos: EXTRA CARS %d (the next press gives %d, or put that number into %s)"
            " - takes effect with the next race",
            dr3_extra_cars, (dr3_extra_cars >= DR3_CHAOS_EXTRA) ? 0 : (dr3_extra_cars + 1),
            DR3_EXTRA_CARS_PATH);

    /* keep the file in step, so its content is what the game is set to (see reload below) */
    dr3_extra_cars_put();

    return dr3_extra_cars;
}

static void dr3_extra_cars_put(void)
{
    FILE *f = fopen(DR3_EXTRA_CARS_PATH, "w");

    if (!f) return;

    fprintf(f, "%d\n", dr3_extra_cars);
    fclose(f);
}

/*
 * The same switch from outside the emulator: sdmc:/chaos_extras.txt holds one number (0 = the race the
 * game asked for, 8 = the twelve car grid).  It is read at every race setup, so the file can be edited
 * on the PC while the emulator runs - the sdmc folder is a normal folder there.
 */
void dr3_chaos_extra_cars_reload(void)
{
    FILE *f = fopen(DR3_EXTRA_CARS_PATH, "r");
    int   cars;

    if (!f) {
        dr3_extra_cars_overridden = 0;
        return;
    }

    if (fscanf(f, "%d", &cars) == 1) {

        const int before = dr3_extra_cars;

        dr3_chaos_extra_cars_set(cars);

        if (dr3_extra_cars != before)
            dr3_log("[dr3] chaos: %s says %d extra car(s), was %d",
                    DR3_EXTRA_CARS_PATH, dr3_extra_cars, before);
    }
    else {
        dr3_log("[dr3] chaos: %s has no number in it - the switch stays at %d",
                DR3_EXTRA_CARS_PATH, dr3_extra_cars);
    }

    fclose(f);
}

/*
 * The race mode of a chaos race (drally.h: DR3_CHAOS_MODE_...).  Exactly the shape of the extra car switch
 * above - one number, a file and a key combo, read at every race setup - and like that switch it only ever
 * applies to races that are chaos races at all (dr3_chaos_active): every other level of the game keeps the
 * original rule set.
 */
static int dr3_mode = DR3_CHAOS_MODE_DEFAULT;
static int dr3_mode_next = -1;          /* what the wheel rolled for the next race, -1 = nothing */

#if defined(DR3_HOST)
#define DR3_MODE_PATH "chaos_mode.txt"
#else
#define DR3_MODE_PATH "sdmc:/chaos_mode.txt"
#endif

static void dr3_mode_put(void);

const char * dr3_chaos_mode_name(int mode)
{
    switch (mode) {

        case DR3_CHAOS_MODE_DUEL:   return "DUEL";
        case DR3_CHAOS_MODE_DM:     return "DM";
        case DR3_CHAOS_MODE_ARENA:  return "ARENA";
        case DR3_CHAOS_MODE_RANDOM: return "RANDOM";

        default:                    return "STANDARD";
    }
}

int dr3_chaos_mode(void)
{
    /*
     * Only the 30th Anniversary (former Chaos) difficulty uses the wheel / special race modes; the three
     * classic difficulties stay authentic Death Rally.
     */
    if (!dr3_chaos_active()) return DR3_CHAOS_MODE_STANDARD;

    return (dr3_mode_next >= 0) ? dr3_mode_next : dr3_mode;
}

/*
 * What is *set* (and what the screen arms the wheel on): RANDOM means "let the wheel roll", a fixed mode means
 * "always that one".  Classic difficulties always report STANDARD so the wheel stays out of them.
 */
int dr3_chaos_mode_setting(void)
{
    if (!dr3_chaos_active()) return DR3_CHAOS_MODE_STANDARD;

    return dr3_mode;
}
/*
 * Whether the race is over (dr3_chaos.h).  `___243d14h` is the engine's own state of the race screen:
 * race_main.c sets it to 1 when the race over sequence starts (and to -1 while a race is being aborted),
 * race___49a34h.c sets it back to 0 when the next race starts.  Anything but 0 therefore means the RACE
 * OVER screen and the fade to black are on their way - the moment the port has to stop drawing its own
 * overlays over that screen (dr3_blit.c: the health lines and the sudden death banner).
 */
int dr3_chaos_race_over(void)
{
    extern __BYTE__ ___243d14h[];
    extern __BYTE__ ___2438c8h[];

    return ((int)D(___243d14h) != 0) || ((int)D(___2438c8h) > 0x12c);
}

/*
 * What the wheel rolled: the race that follows reads it through dr3_chaos_mode(), the setting stays untouched.
 */
void dr3_chaos_mode_commit(int mode)
{
    if (mode < 0) mode = 0;
    if (mode >= DR3_CHAOS_MODE_NUM) mode = DR3_CHAOS_MODE_NUM - 1;

    dr3_mode_next = mode;
}

/* clears any rolled mode and pending duel rival (used on new game, load game, end game, and Race 1) */
void dr3_chaos_mode_clear_roll(void)
{
    dr3_mode_next = -1;
    dr3_chaos_duel_rival_set(-1);
}

/*
 * The switch of the pixel dump (race_main.c, race___4a25ch.c): the file is enough, and the answer is read once
 * - the game presents frames sixty times a second and the switch cannot change underway.
 */
int dr3_chaos_dump_on(void)
{
#if defined(DR3_HOST)
	static const char * dr3_dump_path = "chaos_dump.txt";
#else
	static const char * dr3_dump_path = "sdmc:/chaos_dump.txt";
#endif
	static int on = -1;

	if (on < 0) {

		FILE * f = fopen(dr3_dump_path, "r");

		on = (f != NULL) ? 1 : 0;

		if (f) fclose(f);

		if (on)
			dr3_log("[dr3] chaos: %s is there - the pixel dumps are on (they fill this log)", dr3_dump_path);
	}

	return on;
}

void dr3_chaos_mode_set(int mode)
{
    if (mode < 0) mode = 0;
    if (mode >= DR3_CHAOS_MODE_NUM) mode = DR3_CHAOS_MODE_NUM - 1;

    dr3_mode      = mode;
    dr3_mode_next = -1;         /* an explicit setting is the whole truth - no rolled mode on top of it */
}

/*
 * Whether a race of this career has been set up at all: the first race of a savegame is an ordinary race (the
 * wheel only ever picks races after that, ___3266ch.c).  A file of its own, written once per race setup - the
 * savegame itself has no such counter (drally_structs_fixed.h).
 */
#define DR3_RACED_PATH_HOST  "chaos_raced.txt"
#define DR3_RACED_PATH_3DS   "sdmc:/chaos_raced.txt"

const char * dr3_chaos_raced_path(void)
{
#if defined(DR3_HOST)
    return DR3_RACED_PATH_HOST;
#else
    return DR3_RACED_PATH_3DS;
#endif
}

/*
 * How far apart two seats of the roster stand, -1 when one of them is not a driver of the standings at all
 * (rank 0) or when both are the same seat - a driver is never his own rival.
 *
 * The racers are read by offset, like the game does: `racer_t` is 0x6c bytes (name at +0, rank at +0x48).
 */
static int dr3_duel_rank_gap(int seat_a, int seat_b)
{
	extern __BYTE__ ___1a01e0h[];

	int a, b;

	if (seat_a == seat_b) return -1;
	if ((seat_a < 0) || (seat_a >= 0x14) || (seat_b < 0) || (seat_b >= 0x14)) return -1;

	a = (int)D(___1a01e0h + 0x6c*seat_a + 0x48);
	b = (int)D(___1a01e0h + 0x6c*seat_b + 0x48);

	if ((a <= 0) || (b <= 0)) return -1;        /* a seat that is not in the standings */

	return (a < b) ? (b - a) : (a - b);
}

/*
 * The rival of a duel: a driver of the standings within DR3_CHAOS_DUEL_RANK_WINDOW ranks of the player's own,
 * drawn at random.  The sign up screen offers him and the race setup takes him back (drally.h), which is why
 * the rule lives here and not in either of them.
 *
 * The window is the rule; a standings that has *nobody* that close (the player at the far end of a short
 * field) would otherwise have to fall back on a seat next to the player - which can be a driver from the
 * other end of the table.  Then the nearest drivers of the whole standings are the pool instead, and how
 * wide the pool was (and how far the nearest driver stood) is written to the log: that is the line to read
 * when a rival looks wrong.
 */
int dr3_chaos_duel_pick(int player_seat)
{
	extern __BYTE__ ___1a01e0h[];

	int rand_watcom106(void);

	int my, near = -1, hit = 0, wanted, pick, rival = -1, n;

	if ((player_seat < 0) || (player_seat >= 0x14)) return -1;

	my = (int)D(___1a01e0h + 0x6c*player_seat + 0x48);

	/* who of the standings stands in the window, and how far the nearest of the rest does */
	for (n = 0; n < 0x14; ++n) {

		const int d = dr3_duel_rank_gap(player_seat, n);

		if (d < 0) continue;

		if ((near < 0) || (d < near)) near = d;

		if (d <= DR3_CHAOS_DUEL_RANK_WINDOW) ++hit;
	}

	if (near < 0) {                            /* no other driver in the standings at all */

		dr3_log("[dr3] chaos: duel: rank %d has no other driver of the standings to draw from", my);

		return -1;
	}

	/* nobody in the window: the nearest drivers are the pool, never a driver from a different class */
	wanted = (hit > 0) ? DR3_CHAOS_DUEL_RANK_WINDOW : near;

	hit = 0;

	for (n = 0; n < 0x14; ++n) {

		const int d = dr3_duel_rank_gap(player_seat, n);

		if ((d >= 0) && (d <= wanted)) ++hit;
	}

	if (hit <= 0) return -1;

	dr3_log("[dr3] chaos: duel: rank %d has %d driver(s) within %d rank(s) - the rival is drawn from them",
	        my, hit, wanted);

	pick = rand_watcom106() % hit;

	for (n = 0; n < 0x14; ++n) {

		const int d = dr3_duel_rank_gap(player_seat, n);

		if ((d < 0) || (d > wanted)) continue;

		if (pick-- == 0) { rival = n; break; }
	}

	return rival;
}

/*
 * Whether a rival named outside (by the sign up screen, ___3266ch.c) still is one of the drivers that rule
 * allows: the standings move between the screen and the race, so the setup asks this before it keeps him -
 * a rival named on an earlier visit can be far away by now, and then a new one is drawn.
 */
int dr3_chaos_duel_rival_ok(int player_seat, int rival)
{
	const int d = dr3_duel_rank_gap(player_seat, rival);

	return (d >= 0) && (d <= DR3_CHAOS_DUEL_RANK_WINDOW);
}

/*
 * Whether a race of this savegame has been run at all: the very first race after the licence is an ordinary
 * race (___3266ch.c).  The career's own counter, so it is right in a *new* game even when the last game of the
 * device has been raced (drally_structs_fixed.h: racer_t.races at +0x50).
 */
/* the scenario harness' "raced" step (dr3_scenario.c): the career counts as raced whatever its counter says */
int dr3_chaos_raced_force;

int dr3_chaos_raced(void)
{
    extern __BYTE__ ___1a01e0h[];
    extern __BYTE__ ___1a1ef8h[];

    const int me = (int)D(___1a1ef8h);

    if (dr3_chaos_raced_force) return 1;
    if ((me < 0) || (me >= 0x14)) return 0;

    return ((int)D(___1a01e0h + 0x6c*me + 0x50) > 0);
}

void dr3_chaos_raced_set(void)
{
    if (dr3_chaos_raced()) return;

    {
        FILE * f = fopen(dr3_chaos_raced_path(), "w");

        if (!f) return;

        fprintf(f, "a race of this career has been set up\n");
        fclose(f);
    }

    dr3_log("[dr3] chaos: %s written - the first race of this career is done, the wheel is armed from now on",
            dr3_chaos_raced_path());
}

int dr3_chaos_mode_toggle(void)
{
    dr3_chaos_mode_set((dr3_mode + 1 >= DR3_CHAOS_MODE_NUM) ? 0 : (dr3_mode + 1));

    dr3_log("[dr3] chaos: MODE %d (%s) - takes effect with the next race (or write %d into %s)",
            dr3_mode, dr3_chaos_mode_name(dr3_mode), dr3_mode, DR3_MODE_PATH);

    /* keep the file in step, so its content is what the game is set to (see reload below) */
    dr3_mode_put();

    return dr3_mode;
}

static void dr3_mode_put(void)
{
    FILE *f = fopen(DR3_MODE_PATH, "w");

    if (!f) return;

    fprintf(f, "%d\n", dr3_mode);
    fclose(f);
}

/*
 * The same switch from outside the emulator, read at every race setup (___33010h.c) - so the mode can be
 * changed on the PC while the emulator runs, the way the extra car switch is.
 */
void dr3_chaos_mode_reload(void)
{
    FILE *f = fopen(DR3_MODE_PATH, "r");
    int   mode;

    if (!f) {

        dr3_log("[dr3] chaos: %s is not readable - the mode stays %d (%s)",
                DR3_MODE_PATH, dr3_mode, dr3_chaos_mode_name(dr3_mode));
        return;
    }

    if (fscanf(f, "%d", &mode) == 1) {

        const int before = dr3_mode;

        dr3_chaos_mode_set(mode);

        /*
         * Always, not only on a change: this is the line that says what the wheel of the sign up screen will
         * read (___3266ch.c reads the same file when that screen opens), and it is the first thing to look at
         * when the selector seems dead.
         */
        dr3_log("[dr3] chaos: %s says mode %d (%s), was %d (%s)",
                DR3_MODE_PATH, dr3_mode, dr3_chaos_mode_name(dr3_mode), before,
                dr3_chaos_mode_name(before));
    }
    else {
        dr3_log("[dr3] chaos: %s has no number in it - the mode stays at %d (%s)",
                DR3_MODE_PATH, dr3_mode, dr3_chaos_mode_name(dr3_mode));
    }

    fclose(f);
}

/* the rival of the next duel - a seat of s_6c, -1 = the setup draws one itself (dr3_chaos_duel_pick) */
static int dr3_duel_rival = -1;

int dr3_chaos_duel_rival(void)
{
    return dr3_duel_rival;
}

void dr3_chaos_duel_rival_set(int seat)
{
    if (seat < -1) seat = -1;
    if (seat > 0x13) seat = 0x13;

    dr3_duel_rival = seat;
}

/*
 * Chaos Adversary guest invader switch (dr3_chaos.h):
 *   - chaos_boss.txt / sdmc:/chaos_boss.txt: 0 = never, 1..100 = percent chance, missing/<0 = default (25%).
 *   - dr3_boss_car: which car index holds the Adversary in the current race (-1 when absent).
 */
static int dr3_boss_chance     = DR3_CHAOS_BOSS_CHANCE_DEFAULT;
static int dr3_boss_overridden = 0;
static int dr3_boss_car        = -1;

int dr3_chaos_boss_override(void)
{
    return dr3_boss_overridden;
}

#if defined(DR3_HOST)
#define DR3_BOSS_PATH "chaos_boss.txt"
#else
#define DR3_BOSS_PATH "sdmc:/chaos_boss.txt"
#endif

int dr3_chaos_boss_chance(void)
{
    return dr3_boss_chance;
}

void dr3_chaos_boss_chance_set(int pct)
{
    if (pct < 0) pct = DR3_CHAOS_BOSS_CHANCE_DEFAULT;
    if (pct > 100) pct = 100;

    dr3_boss_chance = pct;
    dr3_boss_overridden = 1;
}

void dr3_chaos_boss_reload(void)
{
    FILE *f   = fopen(DR3_BOSS_PATH, "r");
    int   pct = DR3_CHAOS_BOSS_CHANCE_DEFAULT;

    if (!f) {
        dr3_boss_chance = DR3_CHAOS_BOSS_CHANCE_DEFAULT;
        dr3_boss_overridden = 0;
        return;
    }

    if (fscanf(f, "%d", &pct) == 1) {

        const int before = dr3_boss_chance;

        dr3_chaos_boss_chance_set(pct);
        dr3_log("[dr3] chaos: %s says %d%% adversary chance, was %d%%",
                DR3_BOSS_PATH, dr3_boss_chance, before);
    }
    else {
        dr3_boss_chance = DR3_CHAOS_BOSS_CHANCE_DEFAULT;
        dr3_log("[dr3] chaos: %s has no number in it - adversary chance stays %d%%",
                DR3_BOSS_PATH, dr3_boss_chance);
    }

    fclose(f);
}

int dr3_chaos_boss_car(void)
{
    return dr3_boss_car;
}

void dr3_chaos_boss_car_set(int car)
{
    if ((car < 0) || (car >= DR3_MAX_CARS)) car = -1;

    dr3_boss_car = car;
}

/*
 * A race is set up (___33010h.c): no car of this race has scored yet and the health line has not seen any
 * of them.  -1 means "not seen yet", so a car the race never wrote HP for gets no line drawn over it.
 */
void dr3_chaos_race_reset(void)
{
    int n;

    dr3_boss_car = -1;

    for (n = 0; n < DR3_MAX_CARS; ++n) {

        dr3_chaos_car_gain[n] = 0;
        dr3_chaos_hp_max[n]   = -1;
    }

    /*
     * The rival of a duel is *not* cleared here: the sign up screen names him (___3266ch.c) and the race
     * setup in ___33010h.c keeps a rival that is named, so clearing him per race would throw that away.
     */
}

/*
 * The random selector, drawn over and over ("spin <n>" / --spin <n> in the harness, platform_3ds/dr3_host.c).
 * dr3_chaos_spin_pick() is the very choice the sign up screen makes (___3266ch.c) - running it many times
 * shows the distribution and that only races the player may actually sign up for come out of it.  The
 * animation around it is the game's own and is not part of this.
 */
void dr3_chaos_spin_report(int n)
{
    int race_hits[3] = { 0, 0, 0 };
    int mode_hits[DR3_CHAOS_MODE_NUM];
    int i, race, mode;

    for (i = 0; i < DR3_CHAOS_MODE_NUM; ++i) mode_hits[i] = 0;

    if (n <= 0) n = 20;
    if (n > 1000) n = 1000;

    for (i = 0; i < n; ++i) {

        race = dr3_chaos_spin_pick(&mode);

        if ((race >= 0) && (race < 3)) ++race_hits[race];
        if ((mode >= 0) && (mode < DR3_CHAOS_MODE_NUM)) ++mode_hits[mode];
    }

    dr3_log("[dr3] chaos: spin: %d draws - race 0/1/2 = %d/%d/%d, mode RACE/CHALLENGE/DM/ARENA = %d/%d/%d/%d",
            n, race_hits[0], race_hits[1], race_hits[2],
            mode_hits[DR3_CHAOS_MODE_STANDARD], mode_hits[DR3_CHAOS_MODE_DUEL], mode_hits[DR3_CHAOS_MODE_DM],
            mode_hits[DR3_CHAOS_MODE_ARENA]);
}

/*
 * The band above the player's car.
 *
 * There are two of them per frame now.  The first one is taken right after the track background is copied
 * (that is the road the pre car passes start from - spectators, tyre dust and the track shadows all paint
 * into those rows), the second one right after the cars are drawn, which is where the post car passes are
 * measured from.  A pass that reports pixels wrote into the band over the car, and the values it reports
 * are the palette entries it put there.  The car pass itself is what the watchdog above watches.
 *
 * Three lines per pass and 32 frames are logged: the lines flicker with every frame, and a budget per
 * window keeps the log readable while still covering the whole race instead of only its first seconds.
 */
static __BYTE__    dr3_band[DR3_BAND_ROWS * DR3_CAR_BAND_W];
static int         dr3_band_x, dr3_band_y, dr3_band_on;

#define DR3_BAND_TAGS   16
static const char *dr3_band_tag[DR3_BAND_TAGS];
static int          dr3_band_tag_n;
static int          dr3_band_tag_hits[DR3_BAND_TAGS];
static int          dr3_band_frames;

void dr3_chaos_band_start(void)
{
    extern __POINTER__ BACKBUFFER;

    const __BYTE__ *bb = (const __BYTE__ *)BACKBUFFER;
    int             cx, cy, x, y;

    if (!dr3_log_enabled()) { dr3_band_on = 0; return; }        /* the switch: no snapshot, no measuring */

    /* the counter the watchdog uses: a handful of lines per caller, then a fresh start */
    if (++dr3_watch_frames >= 64) {

        int i;

        dr3_watch_frames = 0;
        for (i = 0; i < 8; ++i) dr3_watch_hits[i] = 0;
    }

    /* and a fresh line budget for every pass, so the log covers the whole race, not just its start */
    if (++dr3_band_frames >= 32) {

        int i;

        dr3_band_frames = 0;
        for (i = 0; i < DR3_BAND_TAGS; ++i) dr3_band_tag_hits[i] = 0;
    }

    dr3_band_on = 0;

    dr3_car_screen(&cx, &cy);

    dr3_band_x = cx + DR3_CAR_BAND_X;
    dr3_band_y = cy - DR3_CAR_BAND_H;

    if ((dr3_band_x < 0) || (dr3_band_y < 0)) return;
    if ((dr3_band_x + DR3_CAR_BAND_W) > DR3_RACE_W) return;
    if ((dr3_band_y + DR3_BAND_ROWS) > DR3_RACE_H) return;

    for (y = 0; y < DR3_BAND_ROWS; ++y)
        for (x = 0; x < DR3_CAR_BAND_W; ++x)
            dr3_band[y * DR3_CAR_BAND_W + x] = bb[0x200 * (dr3_band_y + y) + dr3_band_x + x];

    dr3_band_on = 1;
}

void dr3_chaos_band_mark(const char *pass)
{
    extern __POINTER__ BACKBUFFER;

    const __BYTE__ *bb = (const __BYTE__ *)BACKBUFFER;
    int             count[0x100];
    int             hits = 0, x, y, i, slot = -1;
    int             fx = 0, fy = 0, fnew = 0, fold = 0;
    int             best = 0, bestval = 0;

    if (!dr3_log_enabled()) return;     /* the switch: the band is not compared at all */
    if (!dr3_band_on) return;

    for (i = 0; i < 0x100; ++i) count[i] = 0;

    for (y = 0; y < DR3_BAND_ROWS; ++y) {

        for (x = 0; x < DR3_CAR_BAND_W; ++x) {

            const int at   = y * DR3_CAR_BAND_W + x;
            const int oldv = dr3_band[at];
            const int newv = bb[0x200 * (dr3_band_y + y) + dr3_band_x + x];

            /* the pass that follows compares against what this pass left behind */
            dr3_band[at] = (__BYTE__)newv;

            if (newv == oldv) continue;

            if (hits == 0) { fx = dr3_band_x + x; fy = dr3_band_y + y; fnew = newv; fold = oldv; }

            ++hits;
            if (++count[newv] > best) { best = count[newv]; bestval = newv; }
        }
    }

    if (hits == 0) return;

    /* the pass names are literals, so the first mark of each pass finds its slot once */
    for (i = 0; (i < dr3_band_tag_n) && (slot < 0); ++i)
        if ((dr3_band_tag[i] == pass) || (strcmp(dr3_band_tag[i], pass) == 0)) slot = i;

    if ((slot < 0) && (dr3_band_tag_n < DR3_BAND_TAGS)) {

        slot = dr3_band_tag_n++;
        dr3_band_tag[slot] = pass;
        dr3_band_tag_hits[slot] = 0;
    }

    if (slot < 0) return;

    if (dr3_band_tag_hits[slot] >= 3) return;   /* three lines per pass and 32 frames */

    ++dr3_band_tag_hits[slot];
    dr3_log("[dr3] chaos: BAND %s wrote %d px over the car (most 0x%02x, first %d,%d = 0x%02x was 0x%02x)",
            pass, hits, bestval, fx, fy, fnew, fold);
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

    if (!dr3_log_enabled()) return;     /* the switch: no trace and no heap check while the log is off */

    /* a trace point is a cheap place to look at the engine's heap (see drmemory.c) */
    if (shown < 4000) dr3_drheap_guard(what);

    if (shown >= 4000) return;

    dr3_log("[dr3] chaos step %-12s %d", what, n);
    dr3_log_flush();            /* few calls only - the tail of the log has to be the last step */
    ++shown;
}
#endif

