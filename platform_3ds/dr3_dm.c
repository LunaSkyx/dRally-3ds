/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_dm.c - the endgame of "Destroy 'em all" (dr3_dm.h has the rule, drally.h the entry in the chaos
 * section of the port).
 *
 * The file is in two halves.  Everything above the DR3_CHAOS guard is the rule itself - the clock, the
 * drain and the banner's timing and layout - and is plain integer arithmetic, so the host tests build and
 * check it without a game around it.  Below the guard sits what needs the engine: the cars (s_94) whose HP
 * the sudden death takes, the wind down counter of the race (race_main.c) and the switch on the SD card.
 */
#include "dr3_dm.h"
#include "dr3_arena.h"      /* DR3_ITEM_* pickup kind constants */

unsigned long long dr3_dm_ticks_ms(int ticks)
{
    if (ticks <= 0) return 0ull;

    return (unsigned long long)ticks * 1000ull / (unsigned long long)DR3_DM_TICKS_PER_S;
}

int dr3_dm_ticks_s(int ticks)
{
    if (ticks <= 0) return 0;

    return ticks / DR3_DM_TICKS_PER_S;
}

/* the deadline in ms - the clock is a whole number of seconds, so this is exact */
static unsigned long long dr3_dm_at_ms(int at_s)
{
    return (at_s > 0) ? ((unsigned long long)at_s * 1000ull) : 0ull;
}

int dr3_dm_sudden_active(unsigned long long ms, int at_s)
{
    if (at_s <= 0) return 0;                        /* no deadline - the mode runs as it always did */

    return (ms > dr3_dm_at_ms(at_s));
}

int dr3_dm_sudden_percent(unsigned long long ms, int at_s)
{
    if (!dr3_dm_sudden_active(ms, at_s)) return 0;

    /*
     * One percent per whole second of sudden death: at the deadline itself nothing is owed yet, and every
     * full second after it takes its percent.  The result can grow past 100 (a race that hangs far beyond
     * the drain); the callers clamp their HP at nothing, so that is harmless.
     */
    return (int)((ms - dr3_dm_at_ms(at_s)) * (unsigned long long)DR3_DM_SUDDEN_PERCENT_S / 1000ull);
}

int dr3_dm_sudden_due(unsigned long long ms, int at_s, int drained)
{
    const int owed = dr3_dm_sudden_percent(ms, at_s);

    if (drained < 0) drained = 0;
    if (owed <= drained) return 0;

    return owed - drained;
}

int dr3_dm_hp_for_percent(int percent)
{
    if (percent <= 0) return 0;

    return percent * DR3_DM_HP_PER_PERCENT;
}

int dr3_dm_banner_visible(unsigned long long ms, int at_s)
{
    unsigned long long since;

    if (!dr3_dm_sudden_active(ms, at_s)) return 0;

    since = ms - dr3_dm_at_ms(at_s);

    if (since < (unsigned long long)DR3_DM_BANNER_HOLD_MS) return 1;

    /* after the hold it blinks: up for 70 % of every period, so it reads even while the race goes on */
    return ((since - DR3_DM_BANNER_HOLD_MS) % DR3_DM_BANNER_BLINK_MS) <
           ((unsigned long long)DR3_DM_BANNER_BLINK_MS * 7ull / 10ull);
}

int dr3_dm_glyph_index(char c)
{
    const int i = (int)(unsigned char)c - 0x20;

    if ((i < 0) || (i >= DR3_DM_GLYPHS)) return -1;

    return i;
}

int dr3_dm_text_width(const char *text, int scale)
{
    int       n = 0;

    if (!text || (scale <= 0)) return 0;

    while (text[n]) ++n;
    if (n <= 0) return 0;

    /* every glyph is DR3_DM_GLYPH_W wide with one blank column behind it - the last one has none */
    return ((DR3_DM_GLYPH_ADVANCE * n) - 1) * scale;
}

int dr3_dm_lap_checks(unsigned long long ms, int interval_s)
{
    if (interval_s <= 0) return 0;                  /* the duty is switched off */

    return (int)(ms / ((unsigned long long)interval_s * 1000ull));
}

int dr3_dm_lap_duty_missed(int laps, int laps_at_check)
{
    if (laps_at_check < 0) laps_at_check = 0;
    if (laps < laps_at_check) laps = laps_at_check;     /* the counter never runs backwards */

    return ((laps - laps_at_check) < DR3_DM_LAP_PER_INTERVAL);
}

int dr3_dm_item_for_roll(int roll)
{
    if (roll < 0) roll = 0;
    if (roll > 99) roll = 99;

    if (roll < 30) return DR3_ITEM_AMMO;           /* 30 % - nonstop gunfights                    */
    if (roll < 50) return DR3_ITEM_MINE_CRATE;     /* 20 % - +3 real land mines on the streets    */
    if (roll < 64) return DR3_ITEM_MONEY;          /* 14 % - street cash                          */
    if (roll < 70) return DR3_ITEM_MONEY_BIG;      /*  6 % - 10x big cash jackpot                 */
    if (roll < 82) return DR3_ITEM_TURBO;          /* 12 % - turbo fuel for ramming & pursuit     */
    if (roll < 90) return DR3_ITEM_REPAIR_SMALL;   /*  8 % - small repair                         */

    return DR3_ITEM_REPAIR_BIG;                    /* 10 % - big repair wrench                    */
}

int dr3_dm_target_score(float dist, int target_hp, int hp_max, float angle_abs_deg)
{
    float max_arc;
    int   score;

    if ((dist < 1.0f) || (dist > (float)DR3_DM_ENGAGE_RANGE)) return 0;
    if (target_hp <= 0) return 0;
    if (angle_abs_deg < 0.0f) angle_abs_deg = -angle_abs_deg;

    /* close-quarters brawl allows wider engagement without making street cars U-turn into walls */
    max_arc = (dist < 95.0f) ? 95.0f : 75.0f;
    if (angle_abs_deg > max_arc) return 0;

    score = (int)(((float)DR3_DM_ENGAGE_RANGE - dist) * 10.0f);
    score += (int)((max_arc - angle_abs_deg) * 6.0f);

    /* goal awareness: finish off wounded targets first so cars actually get wrecked */
    if (hp_max > 0) {
        if ((long long)target_hp * 100ll < (long long)hp_max * 35ll) score += 900;
        else if ((long long)target_hp * 100ll < (long long)hp_max * 60ll) score += 350;
    }

    return (score > 0) ? score : 1;
}

int dr3_dm_pickup_want_range(int kind, int ammo, int hp, int hp_max, int mines, int turbo)
{
    const int hp_ref = (hp_max > 0) ? hp_max : DR3_DM_HP_MAX;

    switch (kind) {
    case DR3_ITEM_AMMO:
        if (ammo < DR3_DM_AMMO_MAX * 35 / 100) return 240;
        if (ammo < DR3_DM_AMMO_MAX * 70 / 100) return 170;
        return 90;
    case DR3_ITEM_REPAIR_BIG:
        if (hp < hp_ref * 50 / 100) return 240;
        if (hp < hp_ref * 80 / 100) return 170;
        return 0;
    case DR3_ITEM_REPAIR_SMALL:
        if (hp < hp_ref * 50 / 100) return 200;
        if (hp < hp_ref * 80 / 100) return 130;
        return 0;
    case DR3_ITEM_MINE_CRATE:
        if (mines <= 0) return 230;
        if (mines < 4)  return 175;
        if (mines < 10) return 120;
        return 0;
    case DR3_ITEM_TURBO:
        if (turbo < DR3_DM_AMMO_MAX * 45 / 100) return 150;
        return 90;
    case DR3_ITEM_MONEY_BIG:
        return 185;
    case DR3_ITEM_MONEY:
        return 135;
    default:
        return 0;
    }
}

#if defined(DR3_CHAOS)

#include "drally.h"         /* dr3_chaos_active / DR3_CHAOS_MODE_DM, the engine globals, getCounter() */
#include "drally_structs_free.h"
#include "dr3_chaos.h"      /* DR3_MAX_CARS - the per car arrays of the lap duty */
#include "dr3_log.h"

#include <math.h>
#include <stdio.h>

#define DR3_DM_PI             3.14159265358979f
#define DR3_DM_CTRL_ACCEL     0x01
#define DR3_DM_CTRL_BRAKE     0x02
#define DR3_DM_CTRL_LEFT      0x04
#define DR3_DM_CTRL_RIGHT     0x08
#define DR3_DM_CTRL_TURBO     0x10
#define DR3_DM_CTRL_GUN       0x20
#define DR3_DM_CTRL_MINE      0x40

#define DM_S94(c, off) (*(int32_t *)(void *)(___1de580h + 0x94 * (c) + (off)))

/*
 * The cars of the running race (s_94, 0x94 bytes each - the HP of a car sits at +0x18, dr3_chaos.h), the
 * size of the field, the player's own car and the counter the race over sequence of the frame pass reads
 * (race_main.c: once it is above 0x12c the race winds down).
 */
extern unsigned char ___1de580h[];
extern unsigned char ___1e6ed0h[];      /* the cars (struct_35e_t, 0x35e bytes each) - the checkpoint */
extern unsigned char ___1f2488h[];      /* the 16 pickup slots (0x120 bytes each: x, y, kind)        */
extern __POINTER__   TRX_MAS;
extern int           TRX_WIDTH;
extern int           TRX_HEIGHT;
extern int           NUM_OF_CARS;
extern __DWORD__     MY_CAR_IDX;
extern unsigned char ___2438c8h[];

static int dr3_dm_solid(void *ctx, int x, int y)
{
    (void)ctx;

    if ((x < 0) || (y < 0) || (x >= TRX_WIDTH) || (y >= TRX_HEIGHT)) return 1;

    return ((((const unsigned char *)TRX_MAS)[x + TRX_WIDTH * y] & 15) < 4);
}

/* the engine's own HP of one car, and its write */
static int dr3_dm_car_hp(int car)
{
    return (int)(*(const uint32_t *)(const void *)(___1de580h + 0x94 * car + 0x18));
}

static void dr3_dm_car_hp_set(int car, int hp)
{
    *(uint32_t *)(void *)(___1de580h + 0x94 * car + 0x18) = (uint32_t)hp;
}

#if defined(DR3_HOST)
#define DR3_DM_PATH "chaos_dm.txt"
#define DR3_DM_LAP_PATH "chaos_dm_lap.txt"
#else
#define DR3_DM_PATH "sdmc:/chaos_dm.txt"
#define DR3_DM_LAP_PATH "sdmc:/chaos_dm_lap.txt"
#endif

/* the seconds until the sudden death, 0 = the mode runs as it always did (the switch of dr3_dm.h) */
static int dr3_dm_seconds = DR3_DM_SUDDEN_AT_S;

/* the seconds between two lap checks, 0 = no lap duty (the other switch of dr3_dm.h) */
static int dr3_dm_lap_seconds = DR3_DM_LAP_INTERVAL_S;

static int                dr3_dm_drained;      /* percent the sudden death has taken so far          */
static int                dr3_dm_next_log;     /* the next percent worth a log line (every ten)      */
static int                dr3_dm_started;      /* the "it starts" line has been written             */
static int                dr3_arena_out_at;    /* the tick the player was wrecked at (arena), 0 = still in */
static int                dr3_dm_ended;        /* the wind down has been started                     */
static unsigned long long dr3_dm_ms_now;       /* the race age of the last tick, for the banner      */

/*
 * The lap duty (drally.h / dr3_dm.h: the answer to camping).  `laps` is the port's own count - the engine's
 * `s_35e[n].Lap` is clamped for the HUD in a deathmatch - and `ckpt` is the checkpoint counter read last
 * frame, which is what tells that a lap was just counted (it goes back to 0 then, race___55ae0h.c).
 */
static int dr3_dm_lap_count[DR3_MAX_CARS];
static int dr3_dm_laps_at_check[DR3_MAX_CARS];
static int dr3_dm_ckpt[DR3_MAX_CARS];
static int dr3_dm_checks;                      /* how many lap checks have run                     */

void dr3_dm_reset(void)
{
    int n;

    dr3_dm_drained   = 0;
    dr3_dm_next_log  = 0;
    dr3_dm_started   = 0;
    dr3_dm_ended     = 0;
    dr3_arena_out_at = 0;
    dr3_dm_ms_now    = 0ull;
    dr3_dm_checks    = 0;

    for (n = 0; n < DR3_MAX_CARS; ++n) {

        dr3_dm_lap_count[n]           = 0;
        dr3_dm_laps_at_check[n]  = 0;
        dr3_dm_ckpt[n]           = 0;
    }
}

unsigned long long dr3_dm_banner_ms(void)
{
    return dr3_dm_ms_now;
}

int dr3_dm_sudden_s(void)
{
    return dr3_dm_seconds;
}

void dr3_dm_sudden_set(int seconds)
{
    if (seconds < 0) seconds = 0;

    dr3_dm_seconds = seconds;
}

void dr3_dm_sudden_reload(void)
{
    FILE * f = fopen(DR3_DM_PATH, "r");

    if (!f) return;

    {
        int v = -1;

        if (fscanf(f, "%d", &v) == 1) {

            dr3_dm_sudden_set(v);

            dr3_log("[dr3] chaos: %s: the sudden death of a deathmatch starts after %d second(s), %d percent "
                    "per second (%d HP per car and second), 0 switches it off (drally.h: DR3_CHAOS_MODE_DM)",
                    DR3_DM_PATH, dr3_dm_seconds, DR3_DM_SUDDEN_PERCENT_S, DR3_DM_HP_PER_PERCENT);
        }

        fclose(f);
    }
}

int dr3_dm_lap_s(void)
{
    return dr3_dm_lap_seconds;
}

void dr3_dm_lap_set(int seconds)
{
    if (seconds < 0) seconds = 0;

    dr3_dm_lap_seconds = seconds;
}

void dr3_dm_lap_reload(void)
{
    FILE * f = fopen(DR3_DM_LAP_PATH, "r");

    if (!f) return;

    {
        int v = -1;

        if (fscanf(f, "%d", &v) == 1) {

            dr3_dm_lap_set(v);

            dr3_log("[dr3] chaos: %s: the lap duty of a deathmatch asks for %d lap(s) every %d second(s), a car "
                    "that misses one loses %d percent of its HP (%d HP), 0 switches it off (drally.h: "
                    "DR3_CHAOS_MODE_DM)",
                    DR3_DM_LAP_PATH, DR3_DM_LAP_PER_INTERVAL, dr3_dm_lap_seconds,
                    DR3_DM_LAP_PENALTY_PERCENT, dr3_dm_hp_for_percent(DR3_DM_LAP_PENALTY_PERCENT));
        }

        fclose(f);
    }
}

int dr3_dm_laps(int car)
{
    if ((car < 0) || (car >= DR3_MAX_CARS)) return 0;

    return dr3_dm_lap_count[car];
}

/*
 * One frame of a deathmatch.  The engine's own "who is out" counting (race_main.c:811) wants a wreck to be
 * standing still before it counts, and it is the only way out of the mode besides the player's own wreck -
 * so the sudden death here is what makes the race finish: it takes the HP of every car that still has one
 * and, once only one is left, it starts the engine's wind down.
 */
void dr3_dm_tick(void)
{
    int                alive = 0, n, due;
    const int          ticks = (int)getCounter(1);
    const unsigned long long ms = dr3_dm_ticks_ms(ticks);

    const int          arena = dr3_chaos_arena();

    if (!dr3_chaos_active()) return;
    if (!dr3_chaos_battle()) return;

    /*
     * The lap duty counts the engine's own lap crossings (dr3_dm.h): `s_35e[n].__0`, the checkpoint counter
     * at +0 of the car, goes back to 0 exactly when race___55ae0h.c counts a lap.  The engine's `Lap` field
     * cannot be used for it - it is clamped for the HUD in a deathmatch (nine laps and it stops).
     */
    for (n = 0; (n < NUM_OF_CARS) && !arena; ++n) {      /* the arena has no laps: no duty either */

        const int ckpt = (int)(*(const uint32_t *)(const void *)(___1e6ed0h + 0x35e * n + 0));

        if ((ckpt == 0) && (dr3_dm_ckpt[n] > 0)) ++dr3_dm_lap_count[n];

        dr3_dm_ckpt[n] = ckpt;
    }

    if ((dr3_dm_lap_seconds > 0) && !arena) {

        const int checks = dr3_dm_lap_checks(ms, dr3_dm_lap_seconds);

        while (dr3_dm_checks < checks) {

            const int penalty = dr3_dm_hp_for_percent(DR3_DM_LAP_PENALTY_PERCENT);

            ++dr3_dm_checks;

            for (n = 0; n < NUM_OF_CARS; ++n) {

                const int now = dr3_dm_car_hp(n);

                if (!dr3_dm_lap_duty_missed(dr3_dm_lap_count[n], dr3_dm_laps_at_check[n])) {

                    dr3_dm_laps_at_check[n] = dr3_dm_lap_count[n];
                    continue;
                }

                dr3_dm_laps_at_check[n] = dr3_dm_lap_count[n];

                if (now <= 0) continue;                     /* a wreck owes nothing */

                dr3_dm_car_hp_set(n, (now > penalty) ? (now - penalty) : 0);

                dr3_log("[dr3] chaos: deathmatch: lap check %d: car %d finished no lap since the last one - "
                        "%d percent of its HP (%d HP), it is now at %d HP",
                        dr3_dm_checks, n, DR3_DM_LAP_PENALTY_PERCENT, penalty, dr3_dm_car_hp(n));
            }
        }
    }

    /*
     * Both battle modes (DM and ARENA) end as soon as only one car has HP left - without waiting for a wreck
     * to coast to a stop (s_35e[n].__b0 <= 0.5f) or for sudden death to start.  Not in the first 4 seconds
     * while the field sits on the grid/corners during the lights.
     */
    if (arena) {
        dr3_arena_note_deaths();
    }

    if (ms > 4000ull) {

        int over;

        alive = 0;
        for (n = 0; n < NUM_OF_CARS; ++n) if (dr3_dm_car_hp(n) > 0) ++alive;

        if (arena) {
            /*
             * Player wrecked in Arena: wait DR3_ARENA_SPECTATE_S seconds (3 s) while trashed so a
             * lucky bump into a repair item can still revive them; once trashed for the full window,
             * trigger RACE OVER immediately instead of making them spectate the rest of the match.
             */
            const int my_hp = dr3_dm_car_hp((int)MY_CAR_IDX);

            if (my_hp <= 0) {
                if (dr3_arena_out_at == 0) dr3_arena_out_at = ticks;
            } else {
                dr3_arena_out_at = 0;
            }

            if (dr3_arena_player_trashed_out(my_hp, ticks, dr3_arena_out_at)) {
                if (!dr3_dm_ended) {
                    dr3_dm_ended = 1;
                    dr3_log("[dr3] chaos: arena: player trashed for %ds (%d car(s) left) - triggering Race Over",
                            DR3_ARENA_SPECTATE_S, alive);
                }
                if ((int)D(___2438c8h) <= 0x12c) D(___2438c8h) = 0x12d;
                over = 0;
            } else {
                over = (alive <= 1);
            }
        } else {
            over = (alive <= 1);
        }

        if (over) {

            if (!dr3_dm_ended) {

                dr3_dm_ended = 1;

                dr3_log("[dr3] chaos: %s: %d car(s) left at %ds - the race is over, the wind down starts",
                        arena ? "arena" : "deathmatch", alive, dr3_dm_ticks_s(ticks));
            }

            D(___2438c8h)++;
        }
    }

    if (dr3_dm_seconds <= 0) return;

    dr3_dm_ms_now = ms;

    if (!dr3_dm_started && dr3_dm_sudden_active(ms, dr3_dm_seconds)) {

        dr3_dm_started = 1;

        for (n = 0; n < NUM_OF_CARS; ++n) if (dr3_dm_car_hp(n) > 0) ++alive;

        dr3_log("[dr3] chaos: deathmatch: sudden death starts %ds into the race - %d of %d car(s) still hold HP, "
                "the player's car at %d%%, every car loses %d percent a second (-%d HP)",
                dr3_dm_seconds, alive, NUM_OF_CARS, dr3_dm_car_hp((int)MY_CAR_IDX) * 100 / 102400,
                DR3_DM_SUDDEN_PERCENT_S, DR3_DM_HP_PER_PERCENT);
    }

    due = dr3_dm_sudden_due(ms, dr3_dm_seconds, dr3_dm_drained);

    if (due > 0) {

        const int hp = dr3_dm_hp_for_percent(due);

        dr3_dm_drained += due;

        for (n = 0; n < NUM_OF_CARS; ++n) {

            const int now = dr3_dm_car_hp(n);

            if (now <= 0) continue;                       /* a wreck stays a wreck */

            dr3_dm_car_hp_set(n, (now > hp) ? (now - hp) : 0);
        }

        if ((dr3_dm_drained >= dr3_dm_next_log) && dr3_log_enabled()) {

            alive = 0;
            for (n = 0; n < NUM_OF_CARS; ++n) if (dr3_dm_car_hp(n) > 0) ++alive;

            dr3_log("[dr3] chaos: deathmatch: sudden death after %ds: %d percent taken, %d of %d car(s) left, "
                    "the player's car at %d HP (s_94[%d].__18)",
                    dr3_dm_ticks_s(ticks), dr3_dm_drained, alive, NUM_OF_CARS,
                    dr3_dm_car_hp((int)MY_CAR_IDX), (int)MY_CAR_IDX);

            dr3_dm_next_log = dr3_dm_drained + 10;
        }
    }

    /*
     * The race is decided as soon as one car is left - the survivor, whoever that is.  That is the very
     * rule the engine has (race_main.c:827), only without its "and the wreck stands still": the port
     * counts the HP it just took, so a wreck that keeps rolling cannot keep the race alive.
     */
    if (!dr3_dm_sudden_active(ms, dr3_dm_seconds)) return;

    alive = 0;
    for (n = 0; n < NUM_OF_CARS; ++n) if (dr3_dm_car_hp(n) > 0) ++alive;

    if (alive > 1) return;

    if (!dr3_dm_ended) {

        dr3_dm_ended = 1;

        dr3_log("[dr3] chaos: deathmatch: sudden death decided the race - %d car(s) left at %ds, the wind down "
                "of the race starts (race_main.c)", alive, dr3_dm_ticks_s(ticks));
    }

    D(___2438c8h)++;
}

/*
 * Arm every car in Street Last Car Standing (DR3_CHAOS_MODE_DM): guaranteed machine guns, full ammo,
 * and at least DR3_DM_START_MINES real droppable mines (preserving any higher shop-bought count).
 */
void dr3_dm_setup(void)
{
    int n;

    if (!dr3_chaos_active() || (dr3_chaos_mode() != DR3_CHAOS_MODE_DM)) return;

    for (n = 0; n < NUM_OF_CARS; ++n) {
        DM_S94(n, 0x2c) = 1;
        DM_S94(n, 0x30) = DR3_DM_AMMO_MAX;
        if (DM_S94(n, 0x28) < DR3_DM_START_MINES) DM_S94(n, 0x28) = DR3_DM_START_MINES;
    }

    dr3_log("[dr3] chaos: deathmatch: armed all %d cars (guns ON, full ammo, >= %d mines each)",
            NUM_OF_CARS, DR3_DM_START_MINES);
}

/*
 * Street Last Car Standing (DR3_CHAOS_MODE_DM) Combat Brain layered on top of race___4b62ch():
 * keeps smooth street-track cornering while adding active target hunting, lead-aimed combat steering,
 * aggressive turbo ramming/pursuit, full-range LOS gunfire, tactical rear/pack mine dropping, and
 * street pickup seeking (ammo, mine crates, money, repair, turbo) with wall & lap-duty safety guards.
 */
void dr3_dm_ai_augment(int car)
{
    struct_35e_t *s     = (struct_35e_t *)(void *)___1e6ed0h;
    const int     slot  = (int)getCounter(4);
    const int     ticks = (int)getCounter(1);
    const int     hp    = DM_S94(car, 0x18);
    const int     ammo  = DM_S94(car, 0x30);
    const int     turbo = DM_S94(car, 0x34);
    const int     mines = DM_S94(car, 0x28);
    const int     spikes = DM_S94(car, 0x24);
    const float   cx    = s[car].XLocation;
    const float   cy    = s[car].YLocation;
    const float   dir   = s[car].Direction;
    const float   speed = (s[car].__b0 < 0.0f) ? -s[car].__b0 : s[car].__b0;
    const float   rad   = dir * DR3_DM_PI / 180.0f;
    const float   fx    = -sinf(rad);
    const float   fy    = -cosf(rad);
    unsigned char c;
    int           j;

    if ((car < 0) || (car >= NUM_OF_CARS)) return;
    if ((hp <= 0) || (s[car].Finished != 0)) return;
    if (ticks <= 0xbe) return;

    c = s[car].Ctrls[slot];

    /* clamp vanilla 350-frame mine timer down to aggressive DM cooldown */
    if ((int)s[car].MineTimer > DR3_DM_MINE_COOLDOWN) s[car].MineTimer = DR3_DM_MINE_COOLDOWN;

    /* --- 1. Full-range (130 px) LOS machine gun firing against any alive rival in sights --- */
    if ((ticks > 0x1ae) && (ammo > 0)) {

        for (j = 0; j < NUM_OF_CARS; ++j) {
            float dx, dy, along, side;

            if ((j == car) || (DM_S94(j, 0x18) <= 0)) continue;
            dx = s[j].XLocation - cx;
            dy = (s[j].YLocation - cy) * 1.2f;
            along = dx * fx + dy * fy;
            if ((along < 8.0f) || (along > (float)DR3_DM_GUN_RANGE)) continue;
            side = dx * fy - dy * fx;
            if ((side >= 16.0f) || (side <= -16.0f)) continue;
            if (dr3_arena_los(dr3_dm_solid, NULL, cx, cy, s[j].XLocation, s[j].YLocation)) {
                c |= DR3_DM_CTRL_GUN;
                break;
            }
        }
    }

    /* --- 2. Aggressive tactical mine drops: chasers behind OR surrounded in a tight melee --- */
    if ((ticks > 0x1ae) && (mines > 0) && (s[car].MineTimer == 0)) {

        int drop      = 0;
        int near_pack = 0;

        for (j = 0; j < NUM_OF_CARS; ++j) {
            float dx, dy, d2;

            if ((j == car) || (DM_S94(j, 0x18) <= 0)) continue;
            dx = s[j].XLocation - cx;
            dy = s[j].YLocation - cy;
            d2 = dx * dx + dy * dy;
            if (d2 > (145.0f * 145.0f)) continue;

            if (dr3_arena_behind(cx, cy, dir, s[j].XLocation, s[j].YLocation, 145, 60.0f)) {
                drop = 1;
                break;
            }
            if (d2 < (95.0f * 95.0f)) ++near_pack;
        }

        if (!drop && (near_pack >= 2) && (speed > 0.8f)) drop = 1;

        if (drop) {
            c &= (unsigned char)~DR3_DM_CTRL_BRAKE;
            c |= DR3_DM_CTRL_MINE;
            s[car].MineTimer = DR3_DM_MINE_COOLDOWN;
        }
    }

    /* --- 3. Goal-aware combat steering & pickup seeking (skip while recovering or urgent lap duty) --- */
    {
        int lap_urgent = 0;

        if (dr3_dm_lap_seconds > 0) {
            const unsigned long long ms_now     = dr3_dm_ticks_ms(ticks);
            const unsigned long long interval   = (unsigned long long)dr3_dm_lap_seconds * 1000ull;
            const unsigned long long until_next = interval - (ms_now % interval);

            if ((until_next < 15000ull) &&
                dr3_dm_lap_duty_missed(dr3_dm_lap_count[car], dr3_dm_laps_at_check[car])) {
                lap_urgent = 1;
            }
        }

        if (!lap_urgent && ((int)s[car].__17e < 100) && ((int)s[car].__192 <= 2) && ((int)s[car].__196 <= 2) &&
            (dr3_arena_room(dr3_dm_solid, NULL, cx, cy, dir, 48) >= 34)) {

            int   best_tgt   = -1;
            int   best_score = 0;
            float best_tdist = 1.0e9f;
            int   best_pk    = -1;
            int   pk_urgent  = 0;
            float best_pdist = 1.0e9f;

            /* scan nearby visible enemies */
            for (j = 0; j < NUM_OF_CARS; ++j) {
                float dx, dy, d2, dist, brg, ang;
                int   sc;

                if ((j == car) || (DM_S94(j, 0x18) <= 0)) continue;
                dx = s[j].XLocation - cx;
                dy = s[j].YLocation - cy;
                d2 = dx * dx + dy * dy;
                if ((d2 < 4.0f) || (d2 > (float)(DR3_DM_ENGAGE_RANGE * DR3_DM_ENGAGE_RANGE))) continue;

                dist = sqrtf(d2);
                brg  = dr3_arena_bearing(dx, dy * 1.2f);
                ang  = dr3_arena_wrap180(brg - dir);
                if (ang < 0.0f) ang = -ang;

                sc = dr3_dm_target_score(dist, DM_S94(j, 0x18), DR3_DM_HP_MAX, ang);
                if (sc <= best_score) continue;

                if (!dr3_arena_los(dr3_dm_solid, NULL, cx, cy, s[j].XLocation, s[j].YLocation)) continue;

                best_score = sc;
                best_tgt   = j;
                best_tdist = dist;
            }

            /* scan nearby street pickups (ammo, mine crates, money, repair, turbo) */
            for (j = 0; j < DR3_ARENA_PICKUPS_MAX; ++j) {
                const int32_t kind = *(const int32_t *)(const void *)(___1f2488h + 0x120 * j + 8);
                int           want_r, is_urg;
                float         px, py, dx, dy, d2, dist, brg, ang;

                if (kind <= 0) continue;
                want_r = dr3_dm_pickup_want_range(kind, ammo, hp, DR3_DM_HP_MAX, mines, turbo);
                if (want_r <= 0) continue;

                px = (float)*(const int32_t *)(const void *)(___1f2488h + 0x120 * j);
                py = (float)*(const int32_t *)(const void *)(___1f2488h + 0x120 * j + 4);
                dx = px - cx;
                dy = py - cy;
                d2 = dx * dx + dy * dy;
                if (d2 > (float)(want_r * want_r)) continue;

                brg = dr3_arena_bearing(dx, dy * 1.2f);
                ang = dr3_arena_wrap180(brg - dir);
                if (ang < 0.0f) ang = -ang;
                if (ang > 58.0f) continue;

                dist = sqrtf(d2);
                is_urg = (((kind == DR3_ITEM_AMMO) && (ammo < DR3_DM_AMMO_MAX * 25 / 100)) ||
                          (((kind == DR3_ITEM_REPAIR_BIG) || (kind == DR3_ITEM_REPAIR_SMALL)) && (hp < DR3_DM_HP_MAX * 35 / 100)) ||
                          ((kind == DR3_ITEM_MINE_CRATE) && (mines <= 0)));

                if ((best_pk >= 0) && (pk_urgent > is_urg)) continue;
                if ((pk_urgent == is_urg) && (dist >= best_pdist)) continue;
                if (!dr3_arena_los(dr3_dm_solid, NULL, cx, cy, px, py)) continue;

                best_pk    = j;
                pk_urgent  = is_urg;
                best_pdist = dist;
            }

            /* decide steering goal: urgent survival drop > active prey > convenient street drop */
            if ((best_pk >= 0) && (pk_urgent || (best_tgt < 0) || (best_pdist < best_tdist * 0.65f))) {

                const float px   = (float)*(const int32_t *)(const void *)(___1f2488h + 0x120 * best_pk);
                const float py   = (float)*(const int32_t *)(const void *)(___1f2488h + 0x120 * best_pk + 4);
                const float want = dr3_arena_bearing(px - cx, (py - cy) * 1.2f);

                if (dr3_arena_room(dr3_dm_solid, NULL, cx, cy, want, 64) >= 44) {
                    const int st = dr3_arena_steer(dir, want, 3.0f);

                    c &= (unsigned char)~(DR3_DM_CTRL_LEFT | DR3_DM_CTRL_RIGHT);
                    if (st > 0) c |= DR3_DM_CTRL_LEFT;
                    else if (st < 0) c |= DR3_DM_CTRL_RIGHT;
                    c |= DR3_DM_CTRL_ACCEL;
                }
            }
            else if (best_tgt >= 0) {

                const struct_35e_t *t = &s[best_tgt];
                const float lead = (best_tdist > 90.0f) ? 8.0f : (best_tdist * 0.07f);
                const float trad = (t->Direction + 180.0f) * DR3_DM_PI / 180.0f;
                const float gx   = t->XLocation + lead * t->__b0 * sinf(trad);
                const float gy   = t->YLocation + lead * t->__b0 * cosf(trad) * (5.0f / 6.0f);
                const float want = dr3_arena_bearing(gx - cx, (gy - cy) * 1.2f);
                const int   room = dr3_arena_room(dr3_dm_solid, NULL, cx, cy, want, 72);

                if (room >= 44) {
                    const int st  = dr3_arena_steer(dir, want, 2.5f);
                    float     off = dr3_arena_wrap180(want - dir);

                    if (off < 0.0f) off = -off;
                    c &= (unsigned char)~(DR3_DM_CTRL_LEFT | DR3_DM_CTRL_RIGHT);
                    if (st > 0) c |= DR3_DM_CTRL_LEFT;
                    else if (st < 0) c |= DR3_DM_CTRL_RIGHT;
                    c |= DR3_DM_CTRL_ACCEL;

                    /* aggressive turbo ram / pursuit when lined up on open street */
                    if ((room >= 66) && (off < 16.0f) && (turbo > DR3_DM_AMMO_MAX / 5)) {
                        const int ram_it = (best_tdist < 105.0f) &&
                                           ((ammo <= 0) || (spikes != 0) ||
                                            (DM_S94(best_tgt, 0x18) < DR3_DM_HP_MAX * 35 / 100) ||
                                            (hp > DR3_DM_HP_MAX * 45 / 100));
                        const int chase_it = (best_tdist >= 80.0f) && (off < 12.0f);

                        if (ram_it || chase_it) c |= DR3_DM_CTRL_TURBO;
                    }
                }
            }
        }
    }

    s[car].Ctrls[slot] = c;
}

#endif /* DR3_CHAOS */
