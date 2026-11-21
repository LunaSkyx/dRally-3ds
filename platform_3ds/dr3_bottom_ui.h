/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_bottom_ui.h - Death Rally styled 320x240 bottom-screen renderer (portable, host unit-tested).
 *
 * Renders two full-custom Death Rally pages straight into a dr3_canvas_t (320x240 RGB565 on 3DS,
 * RGBA8888 in host tests) with riveted steel panels, crimson sunburst backdrop, blue-grid status
 * cards, segmented fiery upgrade meters, and a crisp 5x7 shadowed pixel font:
 *
 *   1. Race Dashboard (dr3_ui_draw_race):
 *        - Top bar: track/boss title + live lap clock (or tap hint)
 *        - Left panel: track minimap + lap times / Boss HP / Swarm / Arena ring status
 *        - Right panel: live race standings (adaptive layout for 2, 4, and up to 21 cars!)
 *        - Bottom bar: full vehicle telemetry (POS, LAP/ALIVE, MPH, MINES, HULL, TURBO, AMMO)
 *
 *   2. Menu Command Center (dr3_ui_draw_menu):
 *        - Top bar: DEATH RALLY metallic banner + active mode/phase badge + tap hint
 *        - Top card: Driver & Garage blue-grid card (car, cash, rank, W/R, underground gear,
 *                    segmented ENG/TIR/ARM upgrade pips + HULL bar), or 30th Anniv. welcome card
 *        - Left panel: 3DS controls with keycap badges + live Options (MUS, SFX, WPN, MODE, SHD, TEX)
 *        - Right panel: Championship standings with car colour swatches & gold player highlight
 *        - Footer bar: dynamic quick-save / quick-load / quit / tap-to-darken badges
 */
#ifndef DR3_BOTTOM_UI_H
#define DR3_BOTTOM_UI_H

#include "dr3_minimap.h"
#include <stdint.h>

#define DR3_UI_W              320
#define DR3_UI_H              240
#define DR3_UI_MAX_RACERS     24
#define DR3_UI_MAX_STANDINGS  20

/* Minimap rectangle inside the race dashboard (dr3_bottom.c caches/draws into this exact box) */
#define DR3_UI_RACE_MAP_X     8
#define DR3_UI_RACE_MAP_Y     26
#define DR3_UI_RACE_MAP_W     154
#define DR3_UI_RACE_MAP_H     125

/* Special race mode identifiers passed into dr3_ui_race_t.mode_kind */
#define DR3_UI_RACE_NORMAL    0
#define DR3_UI_RACE_DUEL      1
#define DR3_UI_RACE_DM        2
#define DR3_UI_RACE_ARENA     3
#define DR3_UI_RACE_BOSS_P1   4
#define DR3_UI_RACE_BOSS_P2   5
#define DR3_UI_RACE_BOSS_P3   6

typedef struct {
    char     name[16];
    uint32_t color;          /* 0xRRGGBB driver car colour (0 = default red) */
    int      position;       /* 1..N                                         */
    int      lap;            /* current lap (1..total_laps)                  */
    int      hp;             /* current HP (0 = wrecked)                     */
    int      hp_max;         /* starting/max HP (> 0)                        */
    int      finished;       /* non-zero if crossed finish line              */
    int      is_player;
    int      is_boss;
} dr3_ui_racer_t;

typedef struct {
    char           title[32];        /* track name or boss phase title       */
    char           clock_str[20];    /* "LAP 0:18.42" or "TAP:OFF"           */
    char           last_str[16];     /* "0:18.42" or "--:--.--"              */
    char           best_str[16];
    char           rec_str[16];
    int            new_record_flash; /* non-zero when record was just beaten */
    int            mode_kind;        /* DR3_UI_RACE_*                        */

    /* player vehicle telemetry */
    int            player_pos;       /* 1..n_cars                            */
    int            player_lap;       /* 1..total_laps                        */
    int            total_laps;
    int            speed_mph;        /* 0..220                               */
    int            hull_pct;         /* 0..100 (100 = undamaged, 0 = wreck)  */
    int            turbo_pct;        /* 0..100                               */
    int            ammo_pct;         /* 0..100                               */
    int            weapons_on;       /* 0 = no guns                          */
    int            mines;            /* remaining mines                      */
    int            alive_count;      /* surviving cars (arena/dm/swarm)      */

    /* boss / arena extras */
    int            boss_hp;
    int            boss_hp_max;
    int            boss_enraged;
    int            boss_defeated;
    int            ring_state;       /* 0 = safe/off, 1 = closing, 2 = out!  */
    int            swarm_left;
    int            swarm_total;

    /* live standings entries (need not be pre-sorted; renderer sorts cleanly) */
    dr3_ui_racer_t racers[DR3_UI_MAX_RACERS];
    int            n_racers;
} dr3_ui_race_t;

typedef struct {
    char     name[16];
    uint32_t color;
    int      points;
    int      is_player;
} dr3_ui_standing_t;

typedef struct {
    char              badge[20];     /* e.g. "30TH ANNIV", "PHASE 2/3"       */
    char              mode_name[16]; /* e.g. "RANDOM", "ARENA", "STANDARD"   */
    char              qsave_key[16]; /* e.g. "ZL,X"                          */
    char              qload_key[16]; /* e.g. "ZR,Y"                          */

    int               has_career;
    char              player_name[16];
    char              car_name[16];
    uint32_t          player_color;
    int               cash;
    int               rank;
    int               wins;
    int               races;
    int               engine;        /* 0..4                                 */
    int               engine_max;    /* 1..4                                 */
    int               tires;         /* 0..4                                 */
    int               tires_max;     /* 1..4                                 */
    int               armor;         /* 0..4                                 */
    int               armor_max;     /* 1..4                                 */
    int               hull_pct;      /* 0..100                               */
    int               has_mines;
    int               has_spikes;
    int               has_rocket;
    int               has_sabotage;
    int               boss_phase;    /* 0..3                                 */
    int               cheat_active;  /* 1 = GODMODE test save                */

    /* live game options */
    int               music_pct;     /* 0..100                               */
    int               sfx_pct;       /* 0..100                               */
    int               weapons_on;
    int               shadows_on;
    int               textures_on;
    int               chaos_active;

    /* championship standings (sorted highest points first) */
    dr3_ui_standing_t standings[DR3_UI_MAX_STANDINGS];
    int               n_standings;
} dr3_ui_menu_t;

/* Compact row selector for >max_rows standings lists (exported for unit tests):
   fills out_idx[0..out_n-1] with sorted indices, or -1 for an ellipsis separator row.
   Always keeps the top leaders AND guarantees the player_idx is visible. */
int  dr3_ui_select_window(int total, int player_idx, int max_rows, int *out_idx);

/* Sorts racers into live race standings order (1st..Nth, wrecked at bottom unless finished). */
void dr3_ui_sort_racers(dr3_ui_racer_t *list, int n, int battle_mode);

/* Draws crisp 5x7 text (uppercase/digits/symbols) with optional 1px black drop shadow. */
void dr3_ui_draw_text(const dr3_canvas_t *c, int x, int y, const char *s, uint32_t color, int shadow);

/* Renders the full 320x240 Death Rally race dashboard (excluding the minimap interior when
   draw_map == 0, so the caller can blit a cached minimap + incremental markers into DR3_UI_RACE_MAP_*). */
void dr3_ui_draw_race(const dr3_canvas_t *c, const dr3_ui_race_t *st,
                      const dr3_map_car_t *map_cars, int n_map_cars, int draw_map);

/* Renders the full 320x240 Death Rally menu command center. */
void dr3_ui_draw_menu(const dr3_canvas_t *c, const dr3_ui_menu_t *st);

#endif /* DR3_BOTTOM_UI_H */
