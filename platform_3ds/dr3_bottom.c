/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#include "dr3_bottom.h"
#include "dr3_bottom_ui.h"
#include "dr3_chaos.h"
#if defined(DR3_CHAOS)
#include "dr3_arena.h"
#include "dr3_boss.h"
#include "dr3_dm.h"
#endif
#include "dr3_input_map.h"
#include "dr3_laptime.h"
#include "dr3_log.h"
#include "dr3_minimap.h"

#include "drally.h"
#include "drally_race.h"

/* this file also serves the profiler/missing-file console via real printf */
#if defined(printf)
#undef printf
#endif
#include "drally_structs_fixed.h"
#include "drally_structs_free.h"

#include <3ds.h>
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Engine globals (bss.c / data.c / car_data.c / config_c.c) */
extern __BYTE__        ___1a01e0h[];
extern __BYTE__        ___1a1ef8h[];
extern __BYTE__       *___1a0fb8h;      /* copper.pal: player's chosen car colour */
extern __BYTE__       *___1a0fe0h;      /* carcol.pal: AI drivers' car colours    */
extern const cardata_t ___18e298h[7];
extern __DWORD__       ___24cc58h_msx_volume;
extern __DWORD__       ___24cc54h_sfx_volume;
extern __BYTE__        ___185a14h_UseWeapons[];
extern __DWORD__       ___196a94h_difficulty;

/* Running race state (bss.c) */
extern struct_35e_t ___1e6ed0h[DR3_MAX_CARS];
extern __BYTE__     ___1de7d0h[];
extern __BYTE__     ___1de580h[];
extern int          MY_CAR_IDX;
extern int          NUM_OF_CARS;
extern int          NUM_OF_LAPS;

/* Lap times (bss.c) */
extern int      LAP_PREVIOUS_MIN;
extern int      LAP_PREVIOUS_SEC;
extern int      LAP_PREVIOUS_100;
extern int      LAP_BEST_MIN;
extern int      LAP_BEST_SEC;
extern int      LAP_BEST_100;
extern int      LAP_RECORD_MIN;
extern int      LAP_RECORD_SEC;
extern int      LAP_RECORD_100;
extern __BYTE__ ___243cb8h[];

#define DR3_BOTTOM_TOP 10

static int dr3_bottom_ready;

#define DR3_VIEW_MENU 0
#define DR3_VIEW_RACE 1

#if !defined(DR3_PROFILE)
static int          dr3_bottom_view;
static char         dr3_bottom_track_id[16];
static char         dr3_bottom_track_name[24];
static int          dr3_bottom_map_dirty;
static int          dr3_bottom_map_full = 1;
static int          dr3_bottom_force_redraw = 1;
static int          dr3_bottom_rec_seen[3] = { -1, -1, -1 };
static unsigned int dr3_bottom_rec_flash_ms;
#endif

/* ----------------------------------------------------- legacy text block (profiler build) --- */

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

typedef struct { char name[16]; uint32_t color; int points; int is_player; } dr3_standing_t;

static uint32_t dr3_bottom_driver_rgb(unsigned int color_idx, int is_player)
{
    const unsigned char *pal = is_player ? (const unsigned char *)___1a0fb8h
                                         : (const unsigned char *)___1a0fe0h;
    const unsigned char *c;

    if (!pal || (color_idx >= 256u)) return 0u;
    c = pal + (3u * color_idx);
    if ((c[0] | c[1] | c[2]) == 0) return 0u;
    return ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2];
}

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

        if ((name[0] < 33) || (name[1] == 0)) continue;
        for (j = 0; name[j]; ++j) {
            if (((unsigned char)name[j] < 32) || ((unsigned char)name[j] > 126)) break;
        }
        if (name[j] != 0) continue;

        snprintf(out[n].name, sizeof(out[n].name), "%s", name);
        out[n].is_player = (i == me);
        out[n].color     = dr3_bottom_driver_rgb((unsigned int)r[i].color, out[n].is_player);
        out[n].points    = (int)r[i].points;
        ++n;
    }

    return n;
}

static void dr3_bottom_sort(dr3_standing_t *list, int n)
{
    int i, j;

    for (i = 1; i < n; ++i) {
        const dr3_standing_t key = list[i];

        for (j = i - 1; (j >= 0) && (list[j].points < key.points); --j) list[j + 1] = list[j];
        list[j + 1] = key;
    }
}

static void dr3_bottom_rank_line(char *out, size_t out_size, int rank, const dr3_standing_t *s)
{
    snprintf(out, out_size, "%2d %c %-11.11s%4d", rank, s->is_player ? '*' : ' ', s->name, s->points);
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
            ++rows;
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

/* ----------------------------------------------------- offscreen buffer & flicker probe --- */

#define DR3_BOTTOM_FB_W      240          /* pixels per column (= screen height) */
#define DR3_BOTTOM_FB_COLS   320          /* columns           (= screen width)  */

static uint16_t dr3_bottom_offscreen[DR3_BOTTOM_FB_W * DR3_BOTTOM_FB_COLS] __attribute__((aligned(8)));
static int      dr3_bottom_legacy;

void dr3_bottom_set_legacy(int on) { dr3_bottom_legacy = on; }

#define DR3_PROBE_MAX 30000

static uint16_t          dr3_probe_lit[DR3_PROBE_MAX];
static volatile int      dr3_probe_on, dr3_probe_stop;
static volatile unsigned dr3_probe_n;
static Thread            dr3_probe_thread;
static u64               dr3_probe_t0;

static unsigned dr3_probe_count_lit(void)
{
    uint16_t        w = 0, h = 0;
    const uint16_t *fb = (const uint16_t *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &w, &h);
    unsigned        i, n = 0, total = (unsigned)w * (unsigned)h;

    if (!fb) return 0;
    for (i = 0; i < total; i += 8) n += (fb[i] != 0);
    return n;
}

static void dr3_probe_main(void *arg)
{
    (void)arg;

    while (!dr3_probe_stop) {
        svcSleepThread(1000000LL);
        if (dr3_probe_on && (dr3_probe_n < DR3_PROBE_MAX)) {
            const unsigned n = dr3_probe_count_lit();

            dr3_probe_lit[dr3_probe_n] = (uint16_t)(n > 65535u ? 65535u : n);
            dr3_probe_n = dr3_probe_n + 1;
        }
    }
}

void dr3_bottom_probe_start(void)
{
    if (!dr3_bottom_console_ensure()) return;

    dr3_probe_n  = 0;
    dr3_probe_t0 = svcGetSystemTick();
    dr3_probe_on = 1;

    if (!dr3_probe_thread) {
        dr3_probe_stop   = 0;
        dr3_probe_thread = threadCreate(dr3_probe_main, NULL, 16 * 1024, 0x18, -2, false);
        if (!dr3_probe_thread) dr3_log("[dr3] bottom probe: no thread - nothing will be sampled");
    }
}

void dr3_bottom_probe_report(const char *name, char *out, size_t out_size)
{
    const unsigned n = dr3_probe_n;
    unsigned       i, blank = 0, run = 0, longest = 0, median, lim;
    const unsigned ms = (unsigned)((svcGetSystemTick() - dr3_probe_t0) / 268000u);

    dr3_probe_on = 0;

    if (n < 10) { snprintf(out, out_size, "[probe] %s: only %u sample(s) - no verdict", name, n); return; }

    {
        static unsigned hist[9601];
        unsigned        acc = 0;

        memset(hist, 0, sizeof(hist));
        for (i = 0; i < n; ++i) hist[dr3_probe_lit[i] > 9600 ? 9600 : dr3_probe_lit[i]]++;
        median = 0;
        for (i = 0; i <= 9600; ++i) { acc += hist[i]; if (acc * 2 >= n) { median = i; break; } }
    }

    lim = (median * 6u) / 10u;
    for (i = 0; i < n; ++i) {
        if (dr3_probe_lit[i] < lim) { ++blank; if (++run > longest) longest = run; }
        else run = 0;
    }

    snprintf(out, out_size, "[probe] %s: %u samples over %u ms, settled page %u lit samples, %u sample(s) blank or half drawn (%u.%02u %%), longest blank run %u ms",
             name, n, ms, median, blank, (blank * 100u) / n, ((blank * 10000u) / n) % 100u, longest);
}

int dr3_bottom_console_ensure(void)
{
    if (dr3_bottom_ready) return 1;
    if (!SDL_WasInit(SDL_INIT_VIDEO)) return 0;

    dr3_bottom_ready = 1;
    consoleInit(GFX_BOTTOM, NULL);
    return 1;
}

void dr3_bottom_flush(void)
{
    gfxFlushBuffers();
    gfxScreenSwapBuffers(GFX_BOTTOM, false);
}

int dr3_bottom_save_bmp(const char *path)
{
    uint16_t        w = 0, h = 0;
    const uint16_t *fb = (const uint16_t *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &w, &h);
    unsigned char   hdr[54];
    unsigned char  *row;
    FILE           *f;
    const int       sw = (int)h, sh = (int)w;
    const int       stride = (sw * 3 + 3) & ~3;
    int             x, y;

    if (!fb || (gfxGetScreenFormat(GFX_BOTTOM) != GSP_RGB565_OES)) return 0;

    f = fopen(path, "wb");
    if (!f) return 0;

    row = (unsigned char *)malloc((size_t)stride);
    if (!row) { fclose(f); return 0; }

    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'B'; hdr[1] = 'M';
    {
        const unsigned int size = 54u + (unsigned int)stride * (unsigned int)sh;
        const unsigned int img  = (unsigned int)stride * (unsigned int)sh;

        hdr[2] = size; hdr[3] = size >> 8; hdr[4] = size >> 16; hdr[5] = size >> 24;
        hdr[10] = 54; hdr[14] = 40;
        hdr[18] = sw; hdr[19] = sw >> 8;
        hdr[22] = sh; hdr[23] = sh >> 8;
        hdr[26] = 1;  hdr[28] = 24;
        hdr[34] = img; hdr[35] = img >> 8; hdr[36] = img >> 16; hdr[37] = img >> 24;
    }
    fwrite(hdr, 1, sizeof(hdr), f);

    for (y = sh - 1; y >= 0; --y) {
        memset(row, 0, (size_t)stride);
        for (x = 0; x < sw; ++x) {
            const unsigned int p = fb[(size_t)x * (size_t)w + (size_t)(sh - 1 - y)];
            const unsigned int r = (p >> 11) & 31u, g = (p >> 5) & 63u, b = p & 31u;

            row[x * 3 + 0] = (unsigned char)((b << 3) | (b >> 2));
            row[x * 3 + 1] = (unsigned char)((g << 2) | (g >> 4));
            row[x * 3 + 2] = (unsigned char)((r << 3) | (r >> 2));
        }
        fwrite(row, 1, (size_t)stride, f);
    }

    free(row);
    fclose(f);
    return 1;
}

/* ----------------------------------------------------------- tap to darken / wake --- */

static int dr3_bottom_hidden;

int dr3_bottom_is_hidden(void) { return dr3_bottom_hidden; }

void dr3_bottom_toggle_hidden(void)
{
#if defined(DR3_PROFILE)
    dr3_bottom_hidden = !dr3_bottom_hidden;
    dr3_log("[dr3] bottom screen %s (tap to switch it back)", dr3_bottom_hidden ? "hidden" : "shown");
#else
    const int in_race = dr3_minimap_ready();

    if (dr3_bottom_hidden) {
        dr3_bottom_hidden       = 0;
        dr3_bottom_view         = in_race ? DR3_VIEW_RACE : DR3_VIEW_MENU;
        dr3_bottom_map_dirty    = 1;
        dr3_bottom_map_full     = 1;
        dr3_bottom_force_redraw = 1;
        dr3_log("[dr3] bottom screen: %s page restored", in_race ? "race dashboard" : "menu command center");
    }
    else {
        dr3_bottom_hidden       = 1;
        dr3_bottom_force_redraw = 1;
        dr3_log("[dr3] bottom screen darkened (tap to restore)");
    }
#endif
}

void dr3_bottom_touch(void) { dr3_bottom_touch_ex(-1, -1); }

void dr3_bottom_touch_ex(int ignore_y0, int ignore_y1)
{
    static unsigned int last_ms;
    static int          was_down;
    touchPosition       touch;
    const unsigned int  now = SDL_GetTicks();
    int                 down;

    if ((now - last_ms) < 30) return;
    last_ms = now;

    hidTouchRead(&touch);
    down = (touch.px || touch.py) ? 1 : 0;

    if (down && !was_down) {
        const unsigned int y = touch.py;

        if (!((ignore_y0 >= 0) && (y >= (unsigned int)ignore_y0) && (y < (unsigned int)ignore_y1))) {
            dr3_bottom_toggle_hidden();
        }
    }

    was_down = down;
}

#if !defined(DR3_PROFILE)
/* ---------------------------------------------------------- Death Rally graphical pages --- */

#define DR3_BOTTOM_RACE_MS  125     /* 8 Hz during races for smooth car markers + standings */
#define DR3_BOTTOM_MENU_MS  250     /* 4 Hz polling in menus (skips draw unless state changed) */
#define DR3_RECORD_FLASH_MS 3000

static int dr3_bottom_pixel_size(GSPGPU_FramebufferFormat fmt, int *canvas_fmt)
{
    switch (fmt) {
    case GSP_RGBA8_OES: *canvas_fmt = DR3_CANVAS_RGBA8888; return 4;
    case GSP_BGR8_OES:  *canvas_fmt = DR3_CANVAS_BGR888;   return 3;
    default:            *canvas_fmt = DR3_CANVAS_RGB565;   return 2;
    }
}

static uint32_t dr3_bottom_player_rgb(void)
{
    const racer_t *r  = (const racer_t *)___1a01e0h;
    const int      me = (int)D(___1a1ef8h);

    if ((me < 0) || (me >= 20)) return 0u;
    return dr3_bottom_driver_rgb((unsigned int)r[me].color, 1);
}

#if defined(DR3_CHAOS)
static uint32_t dr3_bottom_car_rgb(int n)
{
    const unsigned char *base;
    unsigned int         r, g, b;

    if ((n < 0) || (n >= DR3_MAX_CARS)) return 0u;
    if (n == dr3_chaos_boss_car()) return 0xC000FFu;

    /* s_54 stores each car's RGB as three 32-bit values at +0x34, +0x38, +0x3c (___33010h.c) */
    base = (const unsigned char *)___1de7d0h + (0x54 * n);
    r = (unsigned int)base[0x34];
    g = (unsigned int)base[0x38];
    b = (unsigned int)base[0x3c];
    if ((r | g | b) == 0u) return 0u;

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}
#endif

static int dr3_bottom_read_cars(dr3_map_car_t *out, int max)
{
    const struct_35e_t *s      = ___1e6ed0h;
    const int           me     = ((MY_CAR_IDX >= 0) && (MY_CAR_IDX < DR3_MAX_CARS)) ? MY_CAR_IDX : 0;
    const uint32_t      my_rgb = dr3_bottom_player_rgb();
    int                 n      = NUM_OF_CARS;
    int                 i;

    if (n < 0) n = 0;
    if (n > DR3_MAX_CARS) n = DR3_MAX_CARS;
    if (n > max) n = max;

    for (i = 0; i < n; ++i) {
        out[i].x         = s[i].XLocation;
        out[i].y         = s[i].YLocation;
        out[i].is_player = (i == me);
#if defined(DR3_CHAOS)
        out[i].color     = (i == me) ? my_rgb : dr3_bottom_car_rgb(i);
#else
        out[i].color     = (i == me) ? my_rgb : 0u;
#endif
        out[i].valid     = (out[i].x >= 1.0f) && (out[i].y >= 1.0f);
    }

    return n;
}

static void dr3_bottom_clean_name(char *out, size_t out_size, const char *src)
{
    char tmp[13];
    int  j;

    memcpy(tmp, src, 12);
    tmp[12] = 0;
    for (j = 11; (j >= 0) && ((tmp[j] == (char)32) || (tmp[j] == 0)); --j) tmp[j] = 0;
    for (j = 0; tmp[j]; ++j) {
        if (((unsigned char)tmp[j] < 32u) || ((unsigned char)tmp[j] > 126u)) tmp[j] = ' ';
    }
    snprintf(out, out_size, "%s", tmp);
}

static int dr3_bottom_clamp_pct(int v)
{
    if (v < 0) return 0;
    if (v > 100) return 100;
    return v;
}

static void dr3_bottom_compact_binding(char *out, size_t out_size, const char *func)
{
    char   raw[24];
    size_t i, j = 0;

    dr3_input_binding_name(raw, sizeof(raw), func);
    for (i = 0; raw[i] && (j + 1 < out_size); ++i) {
        if (raw[i] == ',') {
            out[j++] = '/';
        }
        else if (raw[i] != ' ') {
            out[j++] = raw[i];
        }
    }
    out[j] = 0;
}

static void dr3_bottom_build_race_state(dr3_ui_race_t *st)
{
    const struct_35e_t *s35    = ___1e6ed0h;
    const int           me     = ((MY_CAR_IDX >= 0) && (MY_CAR_IDX < DR3_MAX_CARS)) ? MY_CAR_IDX : 0;
    const uint32_t      my_rgb = dr3_bottom_player_rgb();
    int                 n      = NUM_OF_CARS;
    int                 i, alive = 0;

    memset(st, 0, sizeof(*st));

    if (n < 0) n = 0;
    if (n > DR3_MAX_CARS) n = DR3_MAX_CARS;
    if (n > DR3_UI_MAX_RACERS) n = DR3_UI_MAX_RACERS;

    /* Mode classification + header title */
    st->mode_kind = DR3_UI_RACE_NORMAL;
    snprintf(st->title, sizeof(st->title), "%s", dr3_bottom_track_name[0] ? dr3_bottom_track_name : "DEATH RALLY");

#if defined(DR3_CHAOS)
    if (dr3_boss_is_swarm()) {
        st->mode_kind = DR3_UI_RACE_BOSS_P1;
        snprintf(st->title, sizeof(st->title), "PHASE 1/3: THE SWARM");
    }
    else if (dr3_boss_is_escort_race()) {
        st->mode_kind = DR3_UI_RACE_BOSS_P2;
        snprintf(st->title, sizeof(st->title), "PHASE 2/3: THE ESCORT");
    }
    else if (dr3_boss_is_final_duel()) {
        st->mode_kind = DR3_UI_RACE_BOSS_P3;
        snprintf(st->title, sizeof(st->title), "FINAL: GIANT ADVERSARY");
    }
    else if (dr3_chaos_arena()) {
        st->mode_kind = DR3_UI_RACE_ARENA;
        snprintf(st->title, sizeof(st->title), "BATTLE ARENA");
    }
    else if (dr3_chaos_active() && (dr3_chaos_mode() == DR3_CHAOS_MODE_DM)) {
        st->mode_kind = DR3_UI_RACE_DM;
    }
    else if (dr3_chaos_active() && (dr3_chaos_mode() == DR3_CHAOS_MODE_DUEL)) {
        st->mode_kind = DR3_UI_RACE_DUEL;
    }
#endif

    /* Running clock on header right */
    if (dr3_laptime_is_set(LAP_PREVIOUS_MIN, LAP_PREVIOUS_SEC, LAP_PREVIOUS_100)) {
        char t[12];
        dr3_laptime_format(t, sizeof(t), LAP_PREVIOUS_MIN, LAP_PREVIOUS_SEC, LAP_PREVIOUS_100);
#if defined(DR3_CHAOS)
        snprintf(st->clock_str, sizeof(st->clock_str), "%s %s",
                 (dr3_chaos_arena() || dr3_boss_is_arena_phase()) ? "TIME" : "LAP", t);
#else
        snprintf(st->clock_str, sizeof(st->clock_str), "LAP %s", t);
#endif
    }
    else {
        snprintf(st->clock_str, sizeof(st->clock_str), "TAP:OFF");
    }

    /* Lap times & track record flash */
    if (D(___243cb8h) > 0) dr3_laptime_format_ticks(st->last_str, sizeof(st->last_str), (int)D(___243cb8h));
    else                   snprintf(st->last_str, sizeof(st->last_str), "%s", DR3_LAPTIME_UNSET);

    dr3_laptime_format_or_unset(st->best_str, sizeof(st->best_str), LAP_BEST_MIN, LAP_BEST_SEC, LAP_BEST_100);
    dr3_laptime_format_or_unset(st->rec_str,  sizeof(st->rec_str),  LAP_RECORD_MIN, LAP_RECORD_SEC, LAP_RECORD_100);

    if ((LAP_RECORD_MIN != dr3_bottom_rec_seen[0]) || (LAP_RECORD_SEC != dr3_bottom_rec_seen[1]) ||
        (LAP_RECORD_100 != dr3_bottom_rec_seen[2])) {
        if (dr3_bottom_rec_seen[0] >= 0) dr3_bottom_rec_flash_ms = SDL_GetTicks() + DR3_RECORD_FLASH_MS;
        dr3_bottom_rec_seen[0] = LAP_RECORD_MIN;
        dr3_bottom_rec_seen[1] = LAP_RECORD_SEC;
        dr3_bottom_rec_seen[2] = LAP_RECORD_100;
    }
    if (dr3_bottom_rec_flash_ms && ((int)(dr3_bottom_rec_flash_ms - SDL_GetTicks()) > 0)) {
        st->new_record_flash = 1;
    }

    /* Racers table for live standings */
    st->n_racers = n;
    for (i = 0; i < n; ++i) {
        dr3_ui_racer_t *dst  = &st->racers[i];
        const int       hp   = (int)(*(const int32_t *)(const void *)(___1de580h + 0x94 * i + 0x18));
        int             hmax = 102400;

        /* s_54 (___1de7d0h + 0x54 * i) carries the driver name for ALL 0..20 cars; fall back to s_94+0x44 */
        dr3_bottom_clean_name(dst->name, sizeof(dst->name), (const char *)(___1de7d0h + 0x54 * i));
        if (dst->name[0] < 33) {
            dr3_bottom_clean_name(dst->name, sizeof(dst->name), (const char *)(___1de580h + 0x94 * i + 0x44));
        }
        if ((dst->name[0] < 33) && (i == me)) {
            snprintf(dst->name, sizeof(dst->name), "PLAYER");
        }
        dst->position  = (int)s35[i].Position;
        dst->lap       = (int)s35[i].Lap;
        dst->finished  = (int)s35[i].Finished;
        dst->hp        = (hp > 0) ? hp : 0;
        dst->is_player = (i == me);

#if defined(DR3_CHAOS)
        if (dr3_chaos_hp_max[i] > 0) hmax = dr3_chaos_hp_max[i];
        if (dr3_boss_is_final_duel() && (i == 0)) {
            hmax         = DR3_BOSS_P3_HP_MAX;
            dst->is_boss = 1;
        }
        else if (i == dr3_chaos_boss_car()) {
            dst->is_boss = 1;
        }
        if (dr3_boss_is_swarm() && (i != me)) {
            hmax = DR3_BOSS_P1_ENEMY_HP;
        }
        if (st->mode_kind == DR3_UI_RACE_DM) {
            dst->lap = dr3_dm_laps(i) + 1;
        }
        dst->color = (i == me) ? my_rgb : dr3_bottom_car_rgb(i);
#else
        dst->color = (i == me) ? my_rgb : 0u;
#endif
        dst->hp_max = (hmax > 0) ? hmax : 102400;
        if (dst->hp > 0) ++alive;
    }

    st->alive_count = alive;
    st->total_laps  = (NUM_OF_LAPS > 0) ? NUM_OF_LAPS : 1;

    /* Player vehicle telemetry */
    if ((me >= 0) && (me < n)) {
        static const int base_mph[7] = { 60, 75, 90, 110, 130, 160, 160 };
        const int        car_id      = (int)(*(const uint32_t *)(const void *)(___1de580h + 0x94 * me + 0x00)) & 0xff;
        const float      max_spd     = *(const float *)(const void *)(___1de580h + 0x94 * me + 0x04);
        const int        hp          = (int)(*(const int32_t *)(const void *)(___1de580h + 0x94 * me + 0x18));
        const int        mines       = (int)(*(const int32_t *)(const void *)(___1de580h + 0x94 * me + 0x28));
        const int        wpn_on      = (int)(*(const int32_t *)(const void *)(___1de580h + 0x94 * me + 0x2c));
        const int        ammo        = (int)(*(const int32_t *)(const void *)(___1de580h + 0x94 * me + 0x30));
        const int        turbo       = (int)(*(const int32_t *)(const void *)(___1de580h + 0x94 * me + 0x34));
        const int        top_mph     = ((car_id >= 0) && (car_id < 7)) ? base_mph[car_id] : 120;

        st->player_pos = (int)s35[me].Position;
        st->player_lap = (int)s35[me].Lap;
#if defined(DR3_CHAOS)
        if (st->mode_kind == DR3_UI_RACE_DM) st->player_lap = dr3_dm_laps(me) + 1;
#endif
        if (max_spd > 0.01f) {
            float r = s35[me].__b0 / max_spd;
            if (r < 0.0f) r = 0.0f;
            if (r > 1.35f) r = 1.35f;
            st->speed_mph = (int)(r * (float)top_mph + 0.5f);
        }
        st->hull_pct   = dr3_bottom_clamp_pct((hp > 0) ? ((hp + 512) / 1024) : 0);
        st->turbo_pct  = dr3_bottom_clamp_pct((int)((100.0 * (double)turbo) / 104196.0));
        st->ammo_pct   = dr3_bottom_clamp_pct((int)((100.0 * (double)ammo)  / 102400.0));
        st->weapons_on = (wpn_on != 0);
        st->mines      = (mines > 0) ? mines : 0;
    }

    /* Boss & Arena extras + minimap closing ring */
#if defined(DR3_CHAOS)
    if (dr3_boss_is_swarm()) {
        const int my_alive = ((me >= 0) && (me < n) && (st->racers[me].hp > 0)) ? 1 : 0;
        st->swarm_left  = (alive > my_alive) ? (alive - my_alive) : 0;
        st->swarm_total = DR3_BOSS_P1_ENEMIES;
        dr3_minimap_set_ring(0);
    }
    else if (dr3_boss_is_final_duel()) {
        int bhp = 0, bmax = DR3_BOSS_P3_HP_MAX, benr = 0;
        dr3_boss_hud_bar(&bhp, &bmax, &benr);
        st->boss_hp       = bhp;
        st->boss_hp_max   = bmax;
        st->boss_enraged  = benr;
        st->boss_defeated = dr3_boss_defeated();
        dr3_minimap_set_ring(dr3_boss_p3_ring_active() ? dr3_boss_p3_ring_r() : 0);
    }
    else if (dr3_chaos_arena()) {
        const char *l1 = NULL, *l2 = NULL;
        const int   bk    = dr3_arena_banner_now(&l1, &l2);
        const int   ticks = (int)getCounter(1);
        const int   phase = dr3_arena_ring_phase(ticks);
        st->ring_state = (bk == 2) ? 2 : ((phase >= 1) ? 1 : 0);
        dr3_minimap_set_ring((phase >= 1) ? dr3_arena_ring_radius(ticks) : 0);
    }
    else {
        dr3_minimap_set_ring(0);
    }
#endif
}

static void dr3_bottom_build_menu_state(dr3_ui_menu_t *st)
{
    static const char *const diff_names[5] = {
        "SPEED FREAK", "PETROL HEAD", "FULL THROTTLE", "30TH ANNIV", "30TH CHAOS"
    };
    dr3_standing_t list[20];
    const racer_t *r  = (const racer_t *)___1a01e0h;
    const int      me = (int)D(___1a1ef8h);
    int            n, i;

    memset(st, 0, sizeof(*st));

    /* Live options */
    st->music_pct   = dr3_bottom_clamp_pct((int)(((___24cc58h_msx_volume * 100u) + 0x8000u) >> 16));
    st->sfx_pct     = dr3_bottom_clamp_pct((int)(((___24cc54h_sfx_volume * 100u) + 0x8000u) >> 16));
    st->weapons_on  = (D(___185a14h_UseWeapons) != 0);
    st->shadows_on  = (dRally_Race_getSettings(RACE_SHADOWS) != 0);
    st->textures_on = (dRally_Race_getSettings(RACE_TEXTURES) != 0);

    dr3_bottom_compact_binding(st->qsave_key, sizeof(st->qsave_key), "QUICKSAVE");
    dr3_bottom_compact_binding(st->qload_key, sizeof(st->qload_key), "QUICKLOAD");

    /* Difficulty & Mode badges */
    {
        const unsigned int d = (unsigned int)___196a94h_difficulty;
        snprintf(st->badge, sizeof(st->badge), "%s", (d < 5u) ? diff_names[d] : "DEATH RALLY");
    }

#if defined(DR3_CHAOS)
    st->chaos_active = dr3_chaos_active() ? 1 : 0;
    st->boss_phase   = dr3_boss_phase();
    st->cheat_active = dr3_boss_cheat_active();

    if (st->chaos_active) {
        const char *mname = dr3_chaos_mode_name(dr3_chaos_mode_setting());
        snprintf(st->mode_name, sizeof(st->mode_name), "%s", mname ? mname : "RANDOM");
    }
    else {
        snprintf(st->mode_name, sizeof(st->mode_name), "CLASSIC");
    }

    if (st->cheat_active) {
        snprintf(st->badge, sizeof(st->badge), "GODMODE TEST");
    }
    else if (st->boss_phase == DR3_BOSS_PHASE_1) {
        snprintf(st->badge, sizeof(st->badge), "BOSS 1/3 SWARM");
    }
    else if (st->boss_phase == DR3_BOSS_PHASE_2) {
        snprintf(st->badge, sizeof(st->badge), "BOSS 2/3 ESCORT");
    }
    else if (st->boss_phase == DR3_BOSS_PHASE_3) {
        snprintf(st->badge, sizeof(st->badge), "FINAL BOSS 3/3");
    }
#else
    snprintf(st->mode_name, sizeof(st->mode_name), "CLASSIC");
#endif

    /* Championship standings */
    n = dr3_bottom_read_standings(list, 20);
    dr3_bottom_sort(list, n);
    st->n_standings = (n > DR3_UI_MAX_STANDINGS) ? DR3_UI_MAX_STANDINGS : n;
    for (i = 0; i < st->n_standings; ++i) {
        snprintf(st->standings[i].name, sizeof(st->standings[i].name), "%s", list[i].name);
        st->standings[i].color     = list[i].color;
        st->standings[i].points    = list[i].points;
        st->standings[i].is_player = list[i].is_player;
    }

    /* Active career / garage telemetry (only when player has a valid loaded driver) */
    if ((n > 0) && (me >= 0) && (me < 20)) {
        char pname[16];
        dr3_bottom_clean_name(pname, sizeof(pname), r[me].name);

        if ((pname[0] >= 33) && (pname[1] != 0)) {
            const unsigned int car = (unsigned int)r[me].car;
            int                rk  = 0;

            for (i = 0; i < n; ++i) {
                if (list[i].is_player) { rk = i + 1; break; }
            }

            st->has_career   = 1;
            snprintf(st->player_name, sizeof(st->player_name), "%s", pname);
            st->player_color = dr3_bottom_driver_rgb((unsigned int)r[me].color, 1);
            st->cash         = (int)r[me].money;
            st->rank         = rk;
            st->wins         = (int)r[me].wins;
            st->races        = (int)r[me].races;
            st->engine       = (int)r[me].engine;
            st->tires        = (int)r[me].tires;
            st->armor        = (int)r[me].armor;
            st->hull_pct     = dr3_bottom_clamp_pct(100 - (int)r[me].damage);
            st->has_mines    = (r[me].mines != 0);
            st->has_spikes   = (r[me].spikes != 0);
            st->has_rocket   = (r[me].rocket_fuel != 0);
            st->has_sabotage = (r[me].sabotage != 0);

            if (car < 7u) {
                snprintf(st->car_name, sizeof(st->car_name), "%s", ___18e298h[car].name);
                st->engine_max = (int)___18e298h[car].n_engine_upgrades;
                st->tires_max  = (int)___18e298h[car].n_tire_upgrades;
                st->armor_max  = (int)___18e298h[car].n_armor_upgrades;
            }
            else {
                snprintf(st->car_name, sizeof(st->car_name), "VAGABOND");
                st->engine_max = 1;
                st->tires_max  = 2;
                st->armor_max  = 1;
            }
        }
    }
}

static void dr3_bottom_blit_offscreen(void *fb, uint16_t w, uint16_t h, int bpp)
{
    if ((bpp == 2) && (w == DR3_BOTTOM_FB_W) && (h == DR3_BOTTOM_FB_COLS)) {
        memcpy(fb, dr3_bottom_offscreen, (size_t)DR3_BOTTOM_FB_W * (size_t)DR3_BOTTOM_FB_COLS * 2u);
    }
}

static void dr3_bottom_draw_race_page(void)
{
    dr3_ui_race_t st;
    dr3_map_car_t cars[DR3_MAX_CARS];
    dr3_canvas_t  canvas;
    uint16_t      w = 0, h = 0;
    void         *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &w, &h);
    int           n_cars, bpp;

    if (!fb || (w == 0) || (h == 0)) return;

    bpp = dr3_bottom_pixel_size(gfxGetScreenFormat(GFX_BOTTOM), &canvas.fmt);
    canvas.stride_x = (int)w;
    canvas.stride_y = -1;
    canvas.w        = (int)h;
    canvas.h        = (int)w;

    if (dr3_bottom_map_dirty) {
        dr3_log("[dr3] bottom race UI: fb %dx%d fmt %d (%d bpp) map rect (%d,%d %dx%d) track %dx%d",
                (int)w, (int)h, (int)gfxGetScreenFormat(GFX_BOTTOM), bpp,
                DR3_UI_RACE_MAP_X, DR3_UI_RACE_MAP_Y, DR3_UI_RACE_MAP_W, DR3_UI_RACE_MAP_H,
                dr3_minimap_w(), dr3_minimap_h());
        dr3_bottom_map_dirty = 0;
    }

    dr3_bottom_build_race_state(&st);
    n_cars = dr3_bottom_read_cars(cars, DR3_MAX_CARS);

    if (!dr3_bottom_legacy && (bpp == 2) && (w == DR3_BOTTOM_FB_W) && (h == DR3_BOTTOM_FB_COLS)) {
        canvas.px = (uint8_t *)dr3_bottom_offscreen + (size_t)(DR3_BOTTOM_FB_W - 1) * 2u;
        dr3_ui_draw_race(&canvas, &st, cars, n_cars, dr3_bottom_map_full);
        dr3_bottom_blit_offscreen(fb, w, h, bpp);
    }
    else {
        canvas.px = (uint8_t *)fb + (size_t)(w - 1) * (size_t)bpp;
        dr3_ui_draw_race(&canvas, &st, cars, n_cars, dr3_bottom_map_full);
    }

    dr3_bottom_map_full = 0;
    dr3_bottom_flush();
}

static void dr3_bottom_draw_menu_page(int force)
{
    static dr3_ui_menu_t last_menu;
    static int           last_menu_valid;
    dr3_ui_menu_t        st;
    dr3_canvas_t         canvas;
    uint16_t             w = 0, h = 0;
    void                *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &w, &h);
    int                  bpp;

    if (!fb || (w == 0) || (h == 0)) return;

    dr3_bottom_build_menu_state(&st);
    if (!force && last_menu_valid && (memcmp(&st, &last_menu, sizeof(st)) == 0)) return;

    last_menu       = st;
    last_menu_valid = 1;

    bpp = dr3_bottom_pixel_size(gfxGetScreenFormat(GFX_BOTTOM), &canvas.fmt);
    canvas.stride_x = (int)w;
    canvas.stride_y = -1;
    canvas.w        = (int)h;
    canvas.h        = (int)w;

    if (!dr3_bottom_legacy && (bpp == 2) && (w == DR3_BOTTOM_FB_W) && (h == DR3_BOTTOM_FB_COLS)) {
        canvas.px = (uint8_t *)dr3_bottom_offscreen + (size_t)(DR3_BOTTOM_FB_W - 1) * 2u;
        dr3_ui_draw_menu(&canvas, &st);
        dr3_bottom_blit_offscreen(fb, w, h, bpp);
    }
    else {
        canvas.px = (uint8_t *)fb + (size_t)(w - 1) * (size_t)bpp;
        dr3_ui_draw_menu(&canvas, &st);
    }

    {
        static int n_drawn;
        if (n_drawn < 40) {
            ++n_drawn;
            dr3_log("[dr3] bottom screen: Death Rally menu UI drawn #%d (career=%d standings=%d)",
                    n_drawn, st.has_career, st.n_standings);
        }
    }

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

    if (dr3_minimap_ascii(preview, sizeof(preview), 40, 18) > 0) dr3_log("minimap preview:\n%s", preview);

    dr3_bottom_rec_seen[0] = dr3_bottom_rec_seen[1] = dr3_bottom_rec_seen[2] = -1;
    dr3_bottom_rec_flash_ms = 0;

    if (!dr3_bottom_is_hidden()) {
        dr3_bottom_view         = DR3_VIEW_RACE;
        dr3_bottom_map_dirty    = 1;
        dr3_bottom_map_full     = 1;
        dr3_bottom_force_redraw = 1;
    }
}

void dr3_bottom_track_unloaded(void)
{
    dr3_minimap_reset();

    dr3_bottom_track_id[0]   = 0;
    dr3_bottom_track_name[0] = 0;
    dr3_bottom_view          = DR3_VIEW_MENU;
    dr3_bottom_map_full      = 1;
    dr3_bottom_force_redraw  = 1;
}
#endif /* !DR3_PROFILE */

/* ---------------------------------------------------------------- main update --- */

void dr3_bottom_update(void)
{
    static unsigned int last_ms;
    static int          last_hidden = -1;

#if defined(DR3_PROFILE)
    /* Profiler build owns the bottom screen through dr3_prof.c */
    (void)last_ms;
    (void)last_hidden;
    return;
#else
    static int         last_view = -1;
    const unsigned int now       = SDL_GetTicks();
    const unsigned int interval  = ((dr3_bottom_view == DR3_VIEW_RACE) && !dr3_bottom_hidden)
                                   ? (unsigned int)DR3_BOTTOM_RACE_MS : (unsigned int)DR3_BOTTOM_MENU_MS;

    if (!dr3_bottom_console_ensure()) return;
    if (!dr3_bottom_force_redraw && ((now - last_ms) < interval)) return;
    last_ms = now;

    if (dr3_bottom_hidden) {
        if ((last_hidden == 1) && !dr3_bottom_force_redraw) return;

        {
            uint16_t w = 0, h = 0;
            void    *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &w, &h);
            int      cfmt = 0;
            const int bpp = dr3_bottom_pixel_size(gfxGetScreenFormat(GFX_BOTTOM), &cfmt);

            if (fb && (w > 0) && (h > 0)) {
                memset(fb, 0, (size_t)w * (size_t)h * (size_t)bpp);
            }
        }
        last_hidden             = 1;
        dr3_bottom_force_redraw = 0;
        dr3_bottom_flush();
        return;
    }

    if ((dr3_bottom_view == DR3_VIEW_RACE) && dr3_minimap_ready()) {
        if ((last_view != DR3_VIEW_RACE) || (last_hidden == 1)) dr3_bottom_map_full = 1;
        last_view               = DR3_VIEW_RACE;
        last_hidden             = 0;
        dr3_bottom_force_redraw = 0;
        dr3_bottom_draw_race_page();
        return;
    }

    {
        const int force = (last_view != DR3_VIEW_MENU) || (last_hidden == 1) || dr3_bottom_force_redraw;
        last_view               = DR3_VIEW_MENU;
        last_hidden             = 0;
        dr3_bottom_force_redraw = 0;
        dr3_bottom_draw_menu_page(force);
    }
#endif
}
