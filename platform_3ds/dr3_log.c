/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#include "dr3_log.h"

#if defined(DR3_CHAOS)
#include "drmemory.h"       /* dr3_drheap_guard() - the heap check that runs with the trace */
#include "dr3_chaos.h"      /* DR3_CHAOS_EXTRA, DR3_CHAOS_EXTRA_DEFAULT - the extra car switch */
#endif

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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
    "52d7ch spectators",
    "50ef4h tyre dust",
    "4f170h track shadows",
    "4f300h cars",
    "51ce0h explosions",
    "50a48h gun fire",
    "51204h rocket fuel",
    "53464h objects",
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
#define DR3_EXTRA_CARS_PATH "sdmc:/chaos_extras.txt"

static void dr3_extra_cars_put(void);

int dr3_chaos_extra_cars(void)
{
    return dr3_extra_cars;
}

void dr3_chaos_extra_cars_set(int cars)
{
    if (cars < 0) cars = 0;
    if (cars > DR3_CHAOS_EXTRA) cars = DR3_CHAOS_EXTRA;

    dr3_extra_cars = cars;
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

    if (!f) return;

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
    if ((dr3_band_x + DR3_CAR_BAND_W) > 320) return;
    if ((dr3_band_y + DR3_BAND_ROWS) > 200) return;

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

    /* a trace point is a cheap place to look at the engine's heap (see drmemory.c) */
    if (shown < 4000) dr3_drheap_guard(what);

    if (shown >= 4000) return;

    dr3_log("[dr3] chaos step %-12s %d", what, n);
    dr3_log_flush();            /* few calls only - the tail of the log has to be the last step */
    ++shown;
}
#endif

