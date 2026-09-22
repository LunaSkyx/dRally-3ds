/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#include "dr3_bottom.h"
#include "dr3_input_map.h"
#include "dr3_laptime.h"
#include "dr3_log.h"
#include "dr3_minimap.h"

#include "drally.h"

/* this file draws via the real printf (libctru's console on the bottom screen) */
#if defined(printf)
#undef printf
#endif
#include "drally_structs_fixed.h"
#include "drally_structs_free.h"

#include <3ds.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>

/* the 20 racers of the running game and the index of the player's car (bss.c) */
extern __BYTE__  ___1a01e0h[];
extern __BYTE__  ___1a1ef8h[];

/* the cars of the running race: positions in track pixels, lap and position (bss.c) */
extern struct_35e_t ___1e6ed0h[4];
extern int       MY_CAR_IDX;
extern int       NUM_OF_CARS;
extern int       NUM_OF_LAPS;

/*
 * The lap times of the race that is running (bss.c):
 *   LAP_PREVIOUS_*  the clock of the lap that is on right now (race___40db4h.c hands it the running
 *                   tick counter every frame, so it *is* the live lap time)
 *   LAP_BEST_*      the best lap of this race (zeroed when a race starts, ___33010h.c)
 *   LAP_RECORD_*    the record of the player's car for this track (loaded at race setup from the
 *                   lap record table, ___33010h.c, and overwritten as soon as he beats it)
 *   ___243cb8h      the lap that was just finished, as a raw tick counter (70 ticks per second)
 */
extern int       LAP_PREVIOUS_MIN;
extern int       LAP_PREVIOUS_SEC;
extern int       LAP_PREVIOUS_100;
extern int       LAP_BEST_MIN;
extern int       LAP_BEST_SEC;
extern int       LAP_BEST_100;
extern int       LAP_RECORD_MIN;
extern int       LAP_RECORD_SEC;
extern int       LAP_RECORD_100;
extern __BYTE__  ___243cb8h[];

/* the per-driver colours used by the front end (3 bytes per entry, menu_main.c) */
extern __BYTE__ *___1a0fb8h;

#define DR3_BOTTOM_TOP 10          /* how many drivers the standings list shows */

static int  dr3_bottom_ready;

/*
 * Two pages share the bottom screen: the controls/standings block (text, printed by libctru's
 * console) and the minimap (pixels written straight into the framebuffer, see dr3_minimap.c).
 * Outside a race a tap switches that block off and on again.  In a race the map is the *only* page:
 * the tap there switches between the map and dark (the standings are front end information, what a
 * race needs is the map).  Without a loaded track - or in the profiler build, which owns the screen
 * itself - there is no map page at all.
 */
#define DR3_VIEW_TEXT 0
#define DR3_VIEW_MAP  1

#if !defined(DR3_PROFILE)
static int         dr3_bottom_view;
static char        dr3_bottom_track_id[16];
static char        dr3_bottom_track_name[24]; /* the human readable name, e.g. "Suburbia" */
static int         dr3_bottom_map_dirty;      /* the geometry line still has to be logged */
static int         dr3_bottom_map_full = 1;   /* the next map draw has to repaint everything */
static char        dr3_bottom_map_hdr[96];    /* text currently on the map page */
static char        dr3_bottom_map_times[96];
static char        dr3_bottom_map_ftr[96];

/*
 * The record of the player's car, as it was last seen.  When it changes the player just beat it (the
 * engine also plays SFX_LAP_RECORD and writes the new time back into the record table) - that is worth
 * a moment on the screen.  The -1 marks "not seen yet", so the record that comes with the race does
 * not flash.
 */
static int          dr3_bottom_rec_seen[3] = { -1, -1, -1 };
static unsigned int dr3_bottom_rec_flash_ms;   /* until this tick the flash is shown */
#endif

/* ------------------------------------------------------------------ controls --- */

static const char *const dr3_bottom_controls[] = {
    "D-pad     steer",
    "R         gas",
    "L         brake",
    "A         horn",
    "B         boost",
    "Y         shoot",
    "X         mine",
    "ZL        boost",
    "ZR        shoot",
    "START     pause",
    "SELECT    keyboard",
    "L+R+START quit",
    "(menu: A=ok B=next)",
    "(tap: screen on/off)"
};

/* ---------------------------------------------------------------- standings --- */

typedef struct { char name[16]; int points; int is_player; } dr3_standing_t;

/* Reads the racer array the engine keeps for the running game (racer_t: name at +0, points at
   +0x44).  Returns the number of drivers written; 0 when no game is loaded yet. */
static int dr3_bottom_read_standings(dr3_standing_t *out, int max)
{
    const racer_t *r  = (const racer_t *)___1a01e0h;
    const int      me = (int)D(___1a1ef8h);
    int            i, n = 0;

    for (i = 0; i < 20 && n < max; ++i) {
        char name[13];
        int  j;

        if (r[i].name[0] == 0) continue;

        memcpy(name, r[i].name, 12);
        name[12] = 0;
        for (j = 11; (j >= 0) && ((name[j] == (char)32) || (name[j] == 0)); --j) name[j] = 0;

        /* guard against uninitialised memory (before a game is loaded) and empty slots: only accept
           short names made of plain printable characters */
        if ((name[0] < 33) || (name[1] == 0)) continue;
        for (j = 0; name[j]; ++j) {
            if (((unsigned char)name[j] < 32) || ((unsigned char)name[j] > 126)) break;
        }
        if (name[j] != 0) continue;

        snprintf(out[n].name, sizeof(out[n].name), "%s", name);
        out[n].points    = (int)r[i].points;
        out[n].is_player = (i == me);
        ++n;
    }

    return n;
}

/* highest points first (at most 20 entries) */
static void dr3_bottom_sort(dr3_standing_t *list, int n)
{
    int i, j;

    for (i = 1; i < n; ++i) {
        const dr3_standing_t key = list[i];

        for (j = i - 1; (j >= 0) && (list[j].points < key.points); --j) list[j + 1] = list[j];
        list[j + 1] = key;
    }
}

/* -------------------------------------------------------------------- draw --- */

int dr3_bottom_console_ensure(void)
{
    if (dr3_bottom_ready) return 1;
    if (!SDL_WasInit(SDL_INIT_VIDEO)) return 0;      /* gfx is not up yet - try again later */

    /*
     * Deliberately *single* buffered: libctru's console remembers the frame buffer it was initialised
     * with, so with double buffering its text would end up in the buffer that is not being shown (the
     * text blinked) and my pixels and its cells disagreed about which buffer is current.  Flicker is
     * avoided the other way round instead: the static map is painted once and the frequent updates
     * only repaint the few pixels a car marker occupies (dr3_minimap_draw_incremental).
     */
    dr3_bottom_ready = 1;
    consoleInit(GFX_BOTTOM, NULL);

    return 1;
}

void dr3_bottom_flush(void)
{
    gfxFlushBuffers();
    gfxScreenSwapBuffers(GFX_BOTTOM, false);
}

/* --------------------------------------------------------------- tap to hide --- */

/* Fixed height of the block we print, so that lines which disappear (e.g. "your rank") are
   overwritten instead of staying on screen - and so that we never have to clear the whole screen.
   2 header rows + 14 control rows + a blank + the player row = 18. */
#define DR3_BOTTOM_PRINT_ROWS 18

/* the console is 40 columns wide; staying below that keeps a row from wrapping (which would scroll
   the whole screen).  Used by the controls/standings block and by the two map page text rows. */
#define DR3_BOTTOM_TEXT_W 38

static int dr3_bottom_hidden;

int dr3_bottom_is_hidden(void) { return dr3_bottom_hidden; }

void dr3_bottom_touch(void) { dr3_bottom_touch_ex(-1, -1); }

#if !defined(DR3_PROFILE)
/*
 * A tap on the bottom screen: while a track is loaded (a race) it switches between the minimap and
 * dark - nothing else, the standings page belongs to the front end.  Outside a race it switches the
 * controls/standings block off and on again, as in the first release.
 */
static void dr3_bottom_cycle_view(void)
{
    const int in_race = dr3_minimap_ready();

    if (dr3_bottom_hidden) {
        dr3_bottom_hidden    = 0;
        dr3_bottom_view      = in_race ? DR3_VIEW_MAP : DR3_VIEW_TEXT;
        dr3_bottom_map_dirty = 1;
        dr3_bottom_map_full  = 1;

        dr3_log("[dr3] bottom screen: %s page", in_race ? "minimap" : "ranking");
        return;
    }

    dr3_bottom_hidden = 1;
    dr3_log("[dr3] bottom screen hidden (tap to switch it back)");
}
#endif

void dr3_bottom_touch_ex(int ignore_y0, int ignore_y1)
{
    static unsigned int last_ms;
    static int          was_down;
    touchPosition       touch;
    const unsigned int  now = SDL_GetTicks();
    int                 down;

    if ((now - last_ms) < 30) return;          /* 33 Hz is plenty for tapping */
    last_ms = now;

    hidTouchRead(&touch);
    down = (touch.px || touch.py) ? 1 : 0;

    if (down && !was_down) {
        const unsigned int y = touch.py;

        if (!((ignore_y0 >= 0) && (y >= (unsigned int)ignore_y0) && (y < (unsigned int)ignore_y1))) {
#if defined(DR3_PROFILE)
            dr3_bottom_hidden = !dr3_bottom_hidden;
            dr3_log("[dr3] bottom screen %s (tap to switch it back)", dr3_bottom_hidden ? "hidden" : "shown");
#else
            dr3_bottom_cycle_view();
#endif
        }
    }

    was_down = down;
}

/* One line of the standings: rank, player marker, name and points.  The name is padded so the points
   always sit in the same column, whatever the name length is.  12 character names still fit (the
   right column allows 21 characters). */
static void dr3_bottom_rank_line(char *out, size_t out_size, int rank, const dr3_standing_t *s)
{
    snprintf(out, out_size, "%2d %c %-11s%4d", rank, s->is_player ? '*' : ' ', s->name, s->points);
}

int dr3_bottom_build_lines(char out[DR3_BOTTOM_LINES][DR3_BOTTOM_LINE_LEN])
{
    dr3_standing_t list[20];
    int            n, i, rows = 0;
    const int      n_controls = (int)(sizeof(dr3_bottom_controls) / sizeof(dr3_bottom_controls[0]));

    memset(out, 0, (size_t)DR3_BOTTOM_LINES * DR3_BOTTOM_LINE_LEN);

    n = dr3_bottom_read_standings(list, 20);
    dr3_bottom_sort(list, n);

    snprintf(out[rows], DR3_BOTTOM_LINE_LEN, "%-19s%-21s", "CONTROLS", "RANKING");
    ++rows;
    snprintf(out[rows], DR3_BOTTOM_LINE_LEN, "%-19s%-21s", "-------------------", "--------------------");
    ++rows;

    for (i = 0; i < n_controls && rows < DR3_BOTTOM_LINES; ++i) {
        char right[64] = "";

        if (i < DR3_BOTTOM_TOP && i < n) dr3_bottom_rank_line(right, sizeof(right), i + 1, &list[i]);

        snprintf(out[rows], DR3_BOTTOM_LINE_LEN, "%-19s%.21s", dr3_bottom_controls[i], right);
        ++rows;
    }

    if (rows < DR3_BOTTOM_LINES) {
        if (n == 0) {
            snprintf(out[rows], DR3_BOTTOM_LINE_LEN, "%-19s%-21s", "", "(no game loaded)");
            ++rows;
        }
        else if (!list[0].is_player) {
            /* keep the player in sight even when he is not in the top ten - a row further down so it
               does not run into the controls list */
            ++rows;                     /* blank line */

            for (i = 0; i < n; ++i) {
                if (list[i].is_player) {
                    char right[64];

                    dr3_bottom_rank_line(right, sizeof(right), i + 1, &list[i]);
                    snprintf(out[rows], DR3_BOTTOM_LINE_LEN, "%-19s%.21s", "your rank", right);
                    ++rows;
                    break;
                }
            }
        }
    }

    return rows;
}

#if !defined(DR3_PROFILE)
/* ------------------------------------------------------------------ map page --- */

/*
 * libctru's console font is 8x8 on the 320x240 bottom screen: 40 columns, 30 rows.  Three text rows
 * belong to this page - row 1 (track name and the running lap clock), row 2 (last, best and record
 * lap) and row 30 (position and lap of the race) - and the minimap is painted into the band between
 * them.  Those rows are the only text here: the console only repaints the cells it prints, so the map
 * pixels survive until the page is left (then the screen is cleared).
 */
#define DR3_BOTTOM_MAP_Y   16      /* = two text rows: the header plus the lap times below it */
#define DR3_BOTTOM_MAP_H   216     /* 240 - 3*8: the band between the times row and the footer */
#define DR3_BOTTOM_MAP_W   320

/* the lap clock is part of this page, so it is refreshed more often than the text block (500 ms) */
#define DR3_BOTTOM_MAP_MS  125     /* eight times a second */

/* how long the "*** NEW RECORD ***" notice stays on the times row after the record was beaten */
#define DR3_RECORD_FLASH_MS 3000

/*
 * The colour the player picked for his car: the engine keeps the per-driver colours in
 * menu_main.c's ___1a0fb8h table (3 bytes per entry, indexed by racer_t.color).  Returns 0 when it is
 * not available yet, then the marker falls back to yellow.
 */
static uint32_t dr3_bottom_player_rgb(void)
{
    const racer_t *r  = (const racer_t *)___1a01e0h;
    const int      me = (int)D(___1a1ef8h);
    unsigned int   idx;
    const unsigned char *c;

    if (!___1a0fb8h || (me < 0) || (me >= 20)) return 0;

    idx = (unsigned int)r[me].color;
    if (idx >= 20) return 0;

    c = (const unsigned char *)___1a0fb8h + (3 * idx);
    if ((c[0] | c[1] | c[2]) == 0) return 0;          /* not loaded yet */

    return ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2];
}

/* Reads the cars of the running race (positions in track pixels).  Returns how many were written. */
static int dr3_bottom_read_cars(dr3_map_car_t *out, int max)
{
    const struct_35e_t *s     = ___1e6ed0h;
    const int           me    = ((MY_CAR_IDX >= 0) && (MY_CAR_IDX < 4)) ? MY_CAR_IDX : 0;
    const uint32_t      my_rgb = dr3_bottom_player_rgb();
    int                 n     = NUM_OF_CARS;
    int                 i;

    if (n < 0) n = 0;
    if (n > 4) n = 4;
    if (n > max) n = max;

    for (i = 0; i < n; ++i) {
        out[i].x         = s[i].XLocation;
        out[i].y         = s[i].YLocation;
        out[i].is_player = (i == me);
        out[i].color     = (i == me) ? my_rgb : 0u;   /* 0 = the default colour for the role */

        /* before the first frame of a race the array holds zeros - nothing to draw then */
        out[i].valid     = (out[i].x >= 1.0f) && (out[i].y >= 1.0f);
    }

    return n;
}

/* Bytes per pixel of the bottom screen frame buffer plus the canvas format that matches it.
   libctru's console switches the bottom screen to RGB565, the top screen stays 32-bit (SDL). */
static int dr3_bottom_pixel_size(GSPGPU_FramebufferFormat fmt, int *canvas_fmt)
{
    switch (fmt) {
    case GSP_RGBA8_OES:
        *canvas_fmt = DR3_CANVAS_RGBA8888;
        return 4;

    case GSP_BGR8_OES:
        *canvas_fmt = DR3_CANVAS_BGR888;
        return 3;

    default:            /* RGB565 / RGB5A1 / RGBA4 - all of them are 2 bytes per pixel */
        *canvas_fmt = DR3_CANVAS_RGB565;
        return 2;
    }
}

static void dr3_bottom_draw_map_page(void)
{
    dr3_canvas_t  canvas;
    dr3_map_car_t cars[4];
    uint16_t      w = 0, h = 0;
    uint32_t *    fb = (uint32_t *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &w, &h);
    int           n_cars, bpp = 4;

    if (!fb || (w <= 0) || (h <= 0)) return;

    /* The bottom frame buffer is stored rotated exactly like the top screen in dr3_fb.c: screen x
       runs along the buffer's tall axis and screen y backwards.  Its *pixel format* however differs:
       SDL initialises both screens as GSP_RGBA8_OES, but consoleInit() switches the bottom screen to
       GSP_RGB565_OES, because libctru's console has no 32-bit mode.  Writing 4-byte pixels there
       covers two screen pixels per store and shifts the whole image - so the format decides the
       pointer arithmetic. */
    {
        bpp = dr3_bottom_pixel_size(gfxGetScreenFormat(GFX_BOTTOM), &canvas.fmt);

        canvas.px       = (uint8_t *)fb + (size_t)(w - 1) * (size_t)bpp;
        canvas.stride_x = (int)w;
        canvas.stride_y = -1;
        canvas.w        = (int)h;
        canvas.h        = (int)w;
    }

    {
        /*
         * The map page keeps its text in the cache below and only rewrites its three text rows when
         * something in them changed - and it never clears the screen except when the page is entered,
         * because a clear would also wipe the map band.  Everything else (the markers) goes through
         * dr3_minimap_draw_incremental(), so the pixels of the static map are written exactly once
         * per race: that is what removed the flicker.
         */
        char           header[96];
        char           times[96];
        char           footer[96];
        int            lap = 0, pos = 0, text_changed;

        if (dr3_bottom_map_dirty) {
            const PrintConsole * con = consoleGetDefault();

            /* one geometry line per track: this is what proves whether the frame buffer layout the map
               assumes (rotated, screen y runs backwards) and its pixel format are really the ones in
               use - it made the first broken minimap build obvious from the log alone */
            dr3_log("[dr3] bottom fb %dx%d fmt %d (%d bytes/px, canvas fmt %d) -> canvas %dx%d "
                    "(x stride %d, y stride %d), band (0,%d) %dx%d, map %dx%d, console %dx%d chars",
                    (int)w, (int)h, (int)gfxGetScreenFormat(GFX_BOTTOM), bpp, canvas.fmt,
                    canvas.w, canvas.h, canvas.stride_x, canvas.stride_y,
                    DR3_BOTTOM_MAP_Y, DR3_BOTTOM_MAP_W, DR3_BOTTOM_MAP_H,
                    dr3_minimap_w(), dr3_minimap_h(),
                    con ? con->consoleWidth : -1, con ? con->consoleHeight : -1);

            dr3_bottom_map_dirty = 0;
        }

        if ((MY_CAR_IDX >= 0) && (MY_CAR_IDX < 4)) {
            lap = (int)___1e6ed0h[MY_CAR_IDX].Lap;
            pos = (int)___1e6ed0h[MY_CAR_IDX].Position;
        }

        {
            /*
             * Row 1: the map name on the left and the clock of the lap that is on right now on the
             * right.  LAP_PREVIOUS_* *is* that clock - the engine keeps the running lap in it
             * (race___40db4h.c) and only hands it over to LAP_BEST_* / LAP_RECORD_* when the lap is
             * finished.  Before the first lap time exists the right side carries the tap hint instead:
             * the hint is needed once, the clock is needed every lap.
             */
            char        clock[16];
            const char *title  = dr3_bottom_track_name[0] ? dr3_bottom_track_name : "-";
            int         name_w, title_w = (int)strlen(title);

            if (dr3_laptime_is_set(LAP_PREVIOUS_MIN, LAP_PREVIOUS_SEC, LAP_PREVIOUS_100)) {
                char t[12];                 /* "12:59.99" is the longest a lap time gets */

                dr3_laptime_format(t, sizeof(t), LAP_PREVIOUS_MIN, LAP_PREVIOUS_SEC, LAP_PREVIOUS_100);
                snprintf(clock, sizeof(clock), "LAP %s", t);
            }
            else {
                snprintf(clock, sizeof(clock), "%s", "tap = off");
            }

            name_w = DR3_BOTTOM_TEXT_W - (int)strlen(clock);
            if (name_w < 0) name_w = 0;
            if (title_w > name_w) title_w = name_w;

            snprintf(header, sizeof(header), "%-*.*s%s", name_w, title_w, title, clock);
        }

        {
            /*
             * Row 2: the lap times.  LAST is the lap that was just finished - the engine keeps it as a
             * tick counter in ___243cb8h - BEST is the best lap of this race and REC the record of this
             * car on this track, which the engine overwrites the moment it is beaten: the row then
             * shows the player his own new record right away.
             */
            char last[16], best[16], rec[16];

            if (D(___243cb8h) > 0) dr3_laptime_format_ticks(last, sizeof(last), (int)D(___243cb8h));
            else                   snprintf(last, sizeof(last), "%s", DR3_LAPTIME_UNSET);

            dr3_laptime_format_or_unset(best, sizeof(best), LAP_BEST_MIN, LAP_BEST_SEC, LAP_BEST_100);
            dr3_laptime_format_or_unset(rec, sizeof(rec), LAP_RECORD_MIN, LAP_RECORD_SEC, LAP_RECORD_100);

            /*
             * A record row that changed means the record was just beaten (the engine plays
             * SFX_LAP_RECORD for it as well).  The record that comes with the track must not flash -
             * that is what the -1 in dr3_bottom_rec_seen marks.
             */
            if ((LAP_RECORD_MIN != dr3_bottom_rec_seen[0]) || (LAP_RECORD_SEC != dr3_bottom_rec_seen[1]) ||
                (LAP_RECORD_100 != dr3_bottom_rec_seen[2])) {

                if (dr3_bottom_rec_seen[0] >= 0) dr3_bottom_rec_flash_ms = SDL_GetTicks() + DR3_RECORD_FLASH_MS;

                dr3_bottom_rec_seen[0] = LAP_RECORD_MIN;
                dr3_bottom_rec_seen[1] = LAP_RECORD_SEC;
                dr3_bottom_rec_seen[2] = LAP_RECORD_100;
            }

            if (dr3_bottom_rec_flash_ms && ((int)(dr3_bottom_rec_flash_ms - SDL_GetTicks()) > 0)) {
                snprintf(times, sizeof(times), "*** NEW RECORD %s ***", rec);
            }
            else {
                snprintf(times, sizeof(times), "LAST %s BEST %s REC %s", last, best, rec);
            }
        }

        snprintf(footer, sizeof(footer), "POS %d/%d  LAP %d/%d",
                 pos, (NUM_OF_CARS > 0) ? NUM_OF_CARS : 0,
                 lap, (NUM_OF_LAPS > 0) ? NUM_OF_LAPS : 0);

        text_changed = (strcmp(header, dr3_bottom_map_hdr) != 0) ||
                       (strcmp(times,  dr3_bottom_map_times) != 0) ||
                       (strcmp(footer, dr3_bottom_map_ftr) != 0);

        if (dr3_bottom_map_full) {
            /* entering the page: clear once, print the three text lines and paint the whole map */
            printf("\x1b[2J\x1b[1;1H%-38.38s\x1b[2;1H%-38.38s\x1b[30;1H%-38.38s", header, times, footer);
            snprintf(dr3_bottom_map_hdr, sizeof(dr3_bottom_map_hdr), "%s", header);
            snprintf(dr3_bottom_map_times, sizeof(dr3_bottom_map_times), "%s", times);
            snprintf(dr3_bottom_map_ftr, sizeof(dr3_bottom_map_ftr), "%s", footer);

            n_cars = dr3_bottom_read_cars(cars, 4);
            dr3_minimap_draw(&canvas, 0, DR3_BOTTOM_MAP_Y, DR3_BOTTOM_MAP_W, DR3_BOTTOM_MAP_H,
                             cars, n_cars);

            dr3_bottom_map_full = 0;
            dr3_bottom_flush();
            return;
        }

        if (text_changed) {
            /* only the three text rows - the map band below them is left alone */
            printf("\x1b[1;1H%-38.38s\x1b[2;1H%-38.38s\x1b[30;1H%-38.38s", header, times, footer);
            snprintf(dr3_bottom_map_hdr, sizeof(dr3_bottom_map_hdr), "%s", header);
            snprintf(dr3_bottom_map_times, sizeof(dr3_bottom_map_times), "%s", times);
            snprintf(dr3_bottom_map_ftr, sizeof(dr3_bottom_map_ftr), "%s", footer);
        }
    }

    n_cars = dr3_bottom_read_cars(cars, 4);
    dr3_minimap_draw_incremental(&canvas, 0, DR3_BOTTOM_MAP_Y, DR3_BOTTOM_MAP_W, DR3_BOTTOM_MAP_H,
                                 cars, n_cars);
    dr3_bottom_flush();
}

/* --------------------------------------------------------------- track hooks --- */

void dr3_bottom_track_loaded(const void *mask, const void *image, const void *palette,
                             int mask_w, int mask_h, const char *track_id, const char *track_name)
{
    char preview[1024];
    int  counts[4];

    if (!dr3_minimap_build((const uint8_t *)mask, (const uint8_t *)image, (const uint8_t *)palette,
                           mask_w, mask_h)) {
        dr3_log("[dr3] minimap: no map for this track (%dx%d)", mask_w, mask_h);
        return;
    }

    snprintf(dr3_bottom_track_id, sizeof(dr3_bottom_track_id), "%s", track_id ? track_id : "-");
    snprintf(dr3_bottom_track_name, sizeof(dr3_bottom_track_name), "%s",
             (track_name && track_name[0]) ? track_name : dr3_bottom_track_id);

    dr3_minimap_class_counts(counts);
    dr3_log("[dr3] minimap: %s '%s' %dx%d track -> %dx%d map (step %d): road %d, soft %d, other %d, "
            "none %d%s",
            dr3_bottom_track_id, dr3_bottom_track_name, mask_w, mask_h,
            dr3_minimap_w(), dr3_minimap_h(), dr3_minimap_step(),
            counts[DR3_MAP_ROAD], counts[DR3_MAP_OFFROAD], counts[DR3_MAP_OTHER], counts[DR3_MAP_NONE],
            (image && palette) ? ", colours from the track image" : ", fallback colours");

    /* the shape of the map as text, so a log from a console/emulator is enough to check it */
    if (dr3_minimap_ascii(preview, sizeof(preview), 40, 18) > 0) dr3_log("minimap preview:\n%s", preview);

    /* a new race brings its own record row: the first sight of it must not flash */
    dr3_bottom_rec_seen[0] = dr3_bottom_rec_seen[1] = dr3_bottom_rec_seen[2] = -1;
    dr3_bottom_rec_flash_ms = 0;

    /* show the map right away - it is the interesting page while racing.  If the player hid the
       screen on purpose, it stays hidden. */
    if (!dr3_bottom_is_hidden()) {
        dr3_bottom_view      = DR3_VIEW_MAP;
        dr3_bottom_map_dirty = 1;
        dr3_bottom_map_full  = 1;      /* a new track: repaint the whole band */
    }
}

void dr3_bottom_track_unloaded(void)
{
    dr3_minimap_reset();

    dr3_bottom_track_id[0]   = 0;
    dr3_bottom_track_name[0] = 0;
    dr3_bottom_view          = DR3_VIEW_TEXT;
    dr3_bottom_map_full      = 1;
}
#endif /* !DR3_PROFILE */

/*
 * The two bottom rows of the front end page: what the quick save / quick load buttons are.  The
 * buttons are asked for by name (dr3_input_map.c), so a rebinding in dr3_controls.txt moves the hint
 * with it.  They only exist in the front end - a race cannot be quicksaved, and during a race this
 * page is not on the screen anyway.
 */
#define DR3_BOTTOM_HINT_ROW 29          /* rows 29 and 30, the last two of the 30 row screen */

static void dr3_bottom_quick_hint(char *save_line, char *load_line)
{
    char save[24], load[24];

    dr3_input_binding_name(save, sizeof(save), "QUICKSAVE");
    dr3_input_binding_name(load, sizeof(load), "QUICKLOAD");

    snprintf(save_line, DR3_BOTTOM_LINE_LEN, "%s = quick save", save[0] ? save : "(not bound)");
    snprintf(load_line, DR3_BOTTOM_LINE_LEN, "%s = quick load", load[0] ? load : "(not bound)");
}

void dr3_bottom_update(void)
{
    static unsigned int last_ms;
    static char         last[DR3_BOTTOM_PRINT_ROWS][DR3_BOTTOM_LINE_LEN];
    static int          last_valid;
    static int          last_hidden;
    char                line[DR3_BOTTOM_LINES][DR3_BOTTOM_LINE_LEN];
    char                block[DR3_BOTTOM_PRINT_ROWS * (DR3_BOTTOM_LINE_LEN + 2)];
    int                 rows, i, len = 0, clear_first = 0;

#if !defined(DR3_PROFILE)
    static int          last_view = -1;                 /* no page drawn yet */
    /* the minimap follows the cars and carries the running lap clock, so that page is refreshed far
       more often than the text block */
    const unsigned int  interval = ((dr3_bottom_view == DR3_VIEW_MAP) && !dr3_bottom_hidden)
                                   ? (unsigned int)DR3_BOTTOM_MAP_MS : 500u;
#else
    const unsigned int  interval = 500u;                /* twice a second is plenty */
#endif

    if (!dr3_bottom_console_ensure()) return;
    if ((SDL_GetTicks() - last_ms) < interval) return;
    last_ms = SDL_GetTicks();

    if (dr3_bottom_hidden) {
        if (last_valid && last_hidden) return;          /* already dark */

        last_valid = 1;
        last_hidden = 1;
        printf("\x1b[2J\x1b[H");
        dr3_bottom_flush();
        return;
    }

#if !defined(DR3_PROFILE)
    if ((dr3_bottom_view == DR3_VIEW_MAP) && dr3_minimap_ready()) {
        if (last_view != DR3_VIEW_MAP) last_valid = 0;  /* the text page must be printed again later */
        last_view   = DR3_VIEW_MAP;
        last_hidden = 0;
        dr3_bottom_draw_map_page();
        return;
    }

    if (last_view != DR3_VIEW_TEXT) {                   /* coming back from the map page */
        last_view   = DR3_VIEW_TEXT;
        last_valid  = 0;
        clear_first = 1;                                /* the map pixels are still on the screen */
    }
#endif

    rows = dr3_bottom_build_lines(line);

    /* Nothing to do unless something changed - the engine own printf() output no longer reaches this
       screen (it goes to the log), so the content really is stable. */
    if (last_valid && !last_hidden && !clear_first && (rows == DR3_BOTTOM_PRINT_ROWS) &&
        (memcmp(line, last, sizeof(last)) == 0)) return;

    memcpy(last, line, sizeof(last));
    last_valid  = 1;
    last_hidden = 0;

    /* libctru console redraws the screen for every printf(), so printing the block line by line (and
       clearing the screen first) was visible as flicker.  Build the whole block and write it once -
       the quick save / quick load hint at the bottom of the screen goes into the same write. */
    for (i = 0; i < DR3_BOTTOM_PRINT_ROWS; ++i) {
        const char *text = (i < rows) ? line[i] : "                                        ";

        len += snprintf(block + len, sizeof(block) - (size_t)len, "%.*s\n", DR3_BOTTOM_LINE_LEN, text);
    }

    {
        char hint[2][DR3_BOTTOM_LINE_LEN];
        char tail[2 * (DR3_BOTTOM_LINE_LEN + 16)];
        int  j, tail_len = 0;

        dr3_bottom_quick_hint(hint[0], hint[1]);

        for (j = 0; j < 2; ++j) {
            const int hint_len = (int)strlen(hint[j]);
            const int pad      = (hint_len < DR3_BOTTOM_TEXT_W) ? ((DR3_BOTTOM_TEXT_W - hint_len) / 2) : 0;

            /* the last two rows of the screen, centred */
            tail_len += snprintf(tail + tail_len, sizeof(tail) - (size_t)tail_len, "\x1b[%d;1H%*s%.*s",
                                 DR3_BOTTOM_HINT_ROW + j, pad, "", DR3_BOTTOM_TEXT_W, hint[j]);
        }

        /* with double buffering every repaint has to be complete - clear, then the whole page - so the
           back buffer we swap in never shows leftovers of the previous page */
        printf("\x1b[2J\x1b[H%s%s", block, tail);
    }

    dr3_bottom_flush();
}
