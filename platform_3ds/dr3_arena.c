/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/* dr3_arena.c - see dr3_arena.h.  The portable half first (host tested), the engine glue behind DR3_CHAOS. */
#include "dr3_arena.h"

#include <math.h>
#include <stddef.h>

#define DR3_PI 3.14159265358979f

/* ------------------------------------------------------------------------------------------ geometry */

float dr3_arena_wrap180(float a)
{
    while (a > 180.0f)  a -= 360.0f;
    while (a <= -180.0f) a += 360.0f;

    return a;
}

float dr3_arena_bearing(float dx, float dy)
{
    float d;

    if ((dx == 0.0f) && (dy == 0.0f)) return 0.0f;

    /* the engine moves along (-sin D, -cos D), so (dx, dy) = k * (-sin D, -cos D) -> D = atan2(-dx, -dy) */
    d = atan2f(-dx, -dy) * 180.0f / DR3_PI;
    if (d < 0.0f) d += 360.0f;
    if (d >= 360.0f) d -= 360.0f;

    return d;
}

int dr3_arena_steer(float dir, float want, float dead)
{
    const float diff = dr3_arena_wrap180(want - dir);        /* > 0: the goal is to the left (D grows) */

    if (diff > dead)  return 1;
    if (diff < -dead) return -1;

    return 0;
}

int dr3_arena_room(dr3_arena_solid_fn solid, void *ctx, float x, float y, float dir, int max)
{
    const float rad = dir * DR3_PI / 180.0f;
    const float sx = -sinf(rad), sy = -cosf(rad);
    int         d;

    for (d = 6; d <= max; d += 6)
        if (solid(ctx, (int)(x + sx * (float)d), (int)(y + sy * (float)d))) return d - 6;

    return max;
}

/* the room along a heading for a car with a body: three parallel rays across the car's width (+-10 px) */
static int dr3_arena_room3(dr3_arena_solid_fn solid, void *ctx, float x, float y, float dir, int max)
{
    const float rad = dir * DR3_PI / 180.0f;
    const float px  = -cosf(rad) * 10.0f;
    const float py  =  sinf(rad) * 10.0f;
    int a = dr3_arena_room(solid, ctx, x, y, dir, max);
    int b, c;

    if (a < 18) return a;

    b = dr3_arena_room(solid, ctx, x + px, y + py, dir, max);
    c = dr3_arena_room(solid, ctx, x - px, y - py, dir, max);

    if (b < a) a = b;
    if (c < a) a = c;

    return a;
}

float dr3_arena_plan(dr3_arena_solid_fn solid, void *ctx, float x, float y, float dir, float want,
                     int *blocked_ahead, int brake_at)
{
    static const float turn[] = { 22.0f, -22.0f, 45.0f, -45.0f, 70.0f, -70.0f, 95.0f, -95.0f, 125.0f, -125.0f, 180.0f };
    float best_h;
    float best_s;
    int   i, room;

    if (blocked_ahead) *blocked_ahead = (dr3_arena_room3(solid, ctx, x, y, dir, DR3_ARENA_LOOK) < brake_at);

    room = dr3_arena_room3(solid, ctx, x, y, want, DR3_ARENA_LOOK);
    if (room >= DR3_ARENA_PASSABLE) return want;

    best_h = want;
    best_s = (float)room - 180.0f;                       /* the wanted way is a wall: anything with room beats it */

    for (i = 0; i < (int)(sizeof(turn) / sizeof(turn[0])); ++i) {

        const float h = want + turn[i];
        const int   r = dr3_arena_room3(solid, ctx, x, y, h, DR3_ARENA_LOOK);
        float       s = (float)r;
        float       away = turn[i] < 0.0f ? -turn[i] : turn[i];
        float       fromcar = dr3_arena_wrap180(h - dir);

        if (fromcar < 0.0f) fromcar = -fromcar;

        s -= away * 0.35f;                               /* the further from the goal, the dearer */
        s -= fromcar * 0.12f;                            /* ... and a U turn in the middle of a fight is dear too */

        if (s > best_s) { best_s = s; best_h = h; }
    }

    return dr3_arena_wrap180(best_h) < 0.0f ? dr3_arena_wrap180(best_h) + 360.0f : dr3_arena_wrap180(best_h);
}

int dr3_arena_in_sights(float x, float y, float dir, float tx, float ty, int range, int lateral)
{
    const float rad = dir * DR3_PI / 180.0f;
    const float fx = -sinf(rad), fy = -cosf(rad);
    const float dx = tx - x, dy = ty - y;
    const float along = dx * fx + dy * fy;               /* in front of the car */
    const float side  = dx * fy - dy * fx;               /* sideways */

    if ((along < 8.0f) || (along > (float)range)) return 0;

    return ((side < (float)lateral) && (side > -(float)lateral));
}

int dr3_arena_los(dr3_arena_solid_fn solid, void *ctx, float x, float y, float tx, float ty)
{
    const float dx = tx - x, dy = ty - y;
    const float dist = sqrtf(dx * dx + dy * dy);
    int         d;

    if (dist < 12.0f) return 1;

    for (d = 8; (float)d < dist - 8.0f; d += 8) {

        const float t = (float)d / dist;

        if (solid(ctx, (int)(x + dx * t), (int)(y + dy * t))) return 0;
    }

    return 1;
}

int dr3_arena_behind(float x, float y, float dir, float tx, float ty, int range, float cone)
{
    const float dx = tx - x, dy = ty - y;
    float       d2 = dx * dx + dy * dy;
    float       a;

    if ((d2 > (float)range * (float)range) || (d2 < 1.0f)) return 0;

    a = dr3_arena_wrap180(dr3_arena_bearing(dx, dy) - (dir + 180.0f));   /* 0 = exactly behind */
    if (a < 0.0f) a = -a;

    return (a <= cone);
}

/* ------------------------------------------------------------------------------------------ rules */

void dr3_arena_spawn(int place, int w, int h, int *x, int *y, int *img)
{
    const int corner = ((place % DR3_ARENA_CORNERS) + DR3_ARENA_CORNERS) % DR3_ARENA_CORNERS;
    const int slot   = (place / DR3_ARENA_CORNERS) & 3;
    const int sx = (corner == 0 || corner == 2) ? 1 : -1;
    const int sy = (corner == 0 || corner == 1) ? 1 : -1;
    const int px = ((sx > 0) ? DR3_ARENA_BAY_ANCHOR : (w - DR3_ARENA_BAY_ANCHOR)) + sx * DR3_ARENA_BAY_STEP * (slot % 2);
    const int py = ((sy > 0) ? DR3_ARENA_BAY_ANCHOR : (h - DR3_ARENA_BAY_ANCHOR)) + sy * DR3_ARENA_BAY_STEP * (slot / 2);
    int       i;

    if (x) *x = px;
    if (y) *y = py;
    if (img) {

        i = (int)(dr3_arena_bearing((float)(w / 2 - px), (float)(h / 2 - py)) / 3.75f + 0.5f);
        *img = ((i % 96) + 96) % 96;
    }
}

int dr3_arena_ring_radius(int ticks)
{
    const int grace  = DR3_ARENA_RING_GRACE_S * 70;
    const int shrink = DR3_ARENA_RING_SHRINK_S * 70;

    if (ticks <= grace) return DR3_ARENA_RING_START_R;
    if (ticks >= grace + shrink) return DR3_ARENA_RING_END_R;

    return DR3_ARENA_RING_START_R - (DR3_ARENA_RING_START_R - DR3_ARENA_RING_END_R) * (ticks - grace) / shrink;
}

int dr3_arena_ring_phase(int ticks)
{
    if (ticks <= DR3_ARENA_RING_GRACE_S * 70) return 0;
    if (ticks >= (DR3_ARENA_RING_GRACE_S + DR3_ARENA_RING_SHRINK_S) * 70) return 2;

    return 1;
}

int dr3_arena_ring_outside(float x, float y, int cx, int cy, int r)
{
    const float dx = x - (float)cx, dy = y - (float)cy;

    return (dx * dx + dy * dy) > (float)r * (float)r;
}

int dr3_arena_ring_drain(int hp_max)
{
    const int d = hp_max * DR3_ARENA_RING_DRAIN_PCT / 100 / 70;

    return (d > 0) ? d : 1;
}

int dr3_arena_ring_safe_r(int r)
{
    int margin = (r - 80) / 2;

    if (margin > DR3_ARENA_RING_SAFE) margin = DR3_ARENA_RING_SAFE;
    if (margin < 16) margin = 16;

    return (r > margin) ? (r - margin) : 0;
}

int dr3_arena_banner(int ticks, int own_outside, const char **line1, const char **line2)
{
    const int start = DR3_ARENA_RING_GRACE_S * 70;

    if (own_outside && (ticks > start)) {

        if (((ticks / 18) & 1) == 0) return 0;               /* blinks */

        *line1 = "GET INSIDE";
        *line2 = "THE RING";
        return 2;
    }

    if ((ticks > start) && (ticks < start + 5 * 70)) {

        *line1 = "THE RING";
        *line2 = "CLOSES";
        return 1;
    }

    return 0;
}

int dr3_arena_pick_grudge(int n, int player, int roll)
{
    int has_player, elig, k, i;

    if (n <= 1) return -1;
    has_player = (player >= 0) && (player < n);
    elig = has_player ? (n - 1) : n;
    if (elig <= 0) return -1;

    k = roll % elig;
    if (k < 0) k += elig;

    for (i = 0; i < n; ++i) {
        if (i == player) continue;
        if (k-- == 0) return i;
    }

    return -1;
}

int dr3_arena_extras(int num_cars, int switch_extras)
{
    int room = DR3_ARENA_CARS - num_cars;

    if (switch_extras <= 0) return 0;
    if (room < 0) room = 0;

    return (switch_extras < room) ? switch_extras : room;
}

int dr3_arena_item_for_roll(int roll)
{
    if (roll < 0) roll = 0;
    if (roll > 99) roll = 99;

    if (roll < 30) return DR3_ITEM_AMMO;           /* 30 % - what a fight lives on               */
    if (roll < 50) return DR3_ITEM_MINE_CRATE;     /* 20 % - real Mine Crate (+3 drop mines)     */
    if (roll < 60) return DR3_ITEM_MONEY;          /* 10 % - cash pickup                         */
    if (roll < 64) return DR3_ITEM_MONEY_BIG;      /*  4 % - 10x big cash jackpot                */
    if (roll < 76) return DR3_ITEM_TURBO;          /* 12 % - boost                               */
    if (roll < 84) return DR3_ITEM_REPAIR_SMALL;   /*  8 % - small repair                        */

    return DR3_ITEM_REPAIR_BIG;                    /* 16 % - big repair kit                      */
}

int dr3_arena_prize(int place)
{
    if (place == 1) return DR3_ARENA_PRIZE_1;
    if (place == 2) return DR3_ARENA_PRIZE_2;
    if (place == 3) return DR3_ARENA_PRIZE_3;

    return 0;
}

void dr3_arena_rank(int n, const int *hp, const int *died_at, int *place)
{
    int i, j, better;

    for (i = 0; i < n; ++i) {

        better = 0;

        for (j = 0; j < n; ++j) {

            if (j == i) continue;

            if (hp[j] > 0) {

                /* j survived: it is ahead of a wreck, and of a survivor with less HP (or the same HP and a lower index) */
                if (hp[i] <= 0) ++better;
                else if ((hp[j] > hp[i]) || ((hp[j] == hp[i]) && (j < i))) ++better;
            }
            else if (hp[i] <= 0) {

                /* both wrecks: the later death is the better place (a tie: the lower index) */
                if ((died_at[j] > died_at[i]) || ((died_at[j] == died_at[i]) && (j < i))) ++better;
            }
        }

        place[i] = better + 1;
    }
}

/* ============================================================================================ the engine */
int dr3_arena_player_trashed_out(int player_hp, int ticks, int out_since_tick)
{
    if (player_hp > 0) return 0;
    if (out_since_tick <= 0) return 0;

    return ((ticks - out_since_tick) >= (DR3_ARENA_SPECTATE_S * 70)) ? 1 : 0;
}

#if defined(DR3_CHAOS)

#include "drally.h"
#include "drally_structs_free.h"
#include "dr3_chaos.h"
#include "dr3_boss.h"
#include "dr3_log.h"

#include <stdio.h>
#include <string.h>

extern unsigned char ___1de580h[];      /* s_94: 0x94 bytes a car - HP +0x18, weapon +0x2c, ammo +0x30, turbo +0x34, mines +0x50 */
extern unsigned char ___1e6ed0h[];      /* s_35e: 0x35e bytes a car                                                            */
extern unsigned char ___1de7d0h[];      /* s_54: the names, 0x54 bytes a car                                                   */
extern unsigned char ___1f2488h[];      /* the pickup table: 12 places of 0x120 bytes - x, y, kind (+8)                        */
extern __POINTER__   TRX_MAS;
extern __POINTER__   TRX_IMA;
extern unsigned char ___1a51d0h[];      /* the palette of the loaded track image (0x300 bytes)                              */
extern int           TRX_WIDTH;
extern int           TRX_HEIGHT;
extern int           NUM_OF_CARS;
extern __DWORD__     MY_CAR_IDX;

#define CTRL_ACCELERATE   0x01
#define CTRL_BRAKE        0x02
#define CTRL_STEER_LEFT   0x04
#define CTRL_STEER_RIGHT  0x08
#define CTRL_TURBO_BOOST  0x10
#define CTRL_MACHINE_GUN  0x20
#define CTRL_DROP_MINE    0x40

#define S94(c, off)  (*(int32_t *)(void *)(___1de580h + 0x94 * (c) + (off)))

static struct_35e_t *dr3_arena_cars(void) { return (struct_35e_t *)(void *)___1e6ed0h; }

typedef struct {
    int   target;           /* the car it hunts, -1 = nobody yet                    */
    int   retarget;         /* frames until it thinks about it again                 */
    int   goal_item;        /* the pickup place it drives to, -1 = it hunts         */
    int   item_last;        /* the item it chased the frame before                   */
    int   item_frames;      /* for how long it has been chasing that one             */
    int   ban_until[DR3_ARENA_PICKUPS_MAX];   /* items it gave up on, until this tick */
    float heading;          /* the heading the planner chose                         */
    int   plan_age;         /* frames since the planner ran                          */
    int   still;            /* frames since the position was last checked            */
    float px, py;           /* where it was at that check                            */
    int   stuck_n;          /* how often in a row it had to back out                 */
    int   reverse;          /* frames of backing out left                            */
    int   reverse_turn;     /* the way it steers while it backs out                  */
    int   mine_wait;        /* frames until the next mine                            */
    int   blocked;          /* the way straight ahead is short                       */
    int   died_at;          /* the tick (getCounter(1)) it was wrecked at, 0 = alive  */
} dr3_arena_ai_t;

static dr3_arena_ai_t dr3_ai[DR3_MAX_CARS];

/* One hostile grudge rival per regular FFA Arena ("<NAME> IS MAD AT YOU!"): relentlessly hunts the player */
static int            dr3_grudge_car = -1;
static char           dr3_grudge_name[24];
int                   rand_watcom106(void);

static int dr3_arena_solid(void *ctx, int x, int y)
{
    (void)ctx;

    if ((x < 0) || (y < 0) || (x >= TRX_WIDTH) || (y >= TRX_HEIGHT)) return 1;

    return ((((const unsigned char *)TRX_MAS)[x + TRX_WIDTH * y] & 15) < 4);
}

static const char *dr3_arena_name(int car)
{
    return (const char *)(___1de7d0h + 0x54 * car);
}

static int dr3_arena_alive(int car)
{
    return (S94(car, 0x18) > 0);
}

/* ------------------------------------------------------------------------------------------ the closing ring */

#define DR3_RING_SAVE_MAX 40000

static int32_t       dr3_ring_idx[DR3_RING_SAVE_MAX];     /* the pixels of the track picture the ring has painted over ...   */
static unsigned char dr3_ring_val[DR3_RING_SAVE_MAX];     /* ... and what was in them                                         */
static int           dr3_ring_n;
static int           dr3_ring_colour = -1;                /* the palette entry of the line                                    */
static int           dr3_ring_painted_at = -1000;         /* the tick of the last paint                                       */
static int           dr3_ring_last_r = -1;                /* the radius currently painted on TRX_IMA                          */
static int           dr3_ring_logged_phase = -1;

/* cached active pickup coordinates for the current ring paint (avoids reading ___1f2488h per pixel) */
static int           dr3_ring_pk_x[DR3_ARENA_PICKUPS_MAX];
static int           dr3_ring_pk_y[DR3_ARENA_PICKUPS_MAX];
static int           dr3_ring_pk_n;

/* the 16 pickup places hold a copy of the pixels under them (race___526ach.c) - the ring leaves a gap there so
   their restore never puts a stale piece of the ring back */
static int dr3_ring_near_pickup(int x, int y)
{
    int n;

    for (n = 0; n < dr3_ring_pk_n; ++n) {
        if ((x >= dr3_ring_pk_x[n] - 11) && (x <= dr3_ring_pk_x[n] + 11) &&
            (y >= dr3_ring_pk_y[n] - 11) && (y <= dr3_ring_pk_y[n] + 11)) return 1;
    }

    return 0;
}

static void dr3_ring_unpaint(void)
{
    unsigned char *ima = (unsigned char *)TRX_IMA;
    int            i;

    if (!ima) { dr3_ring_n = 0; return; }

    for (i = 0; i < dr3_ring_n; ++i) ima[dr3_ring_idx[i]] = dr3_ring_val[i];

    dr3_ring_n = 0;
}

static void dr3_ring_span(int y, int x0, int x1)
{
    unsigned char *ima = (unsigned char *)TRX_IMA;
    int            x;

    if (x0 < 0) x0 = 0;
    if (x1 >= TRX_WIDTH) x1 = TRX_WIDTH - 1;

    for (x = x0; x <= x1; ++x) {

        const int i = y * TRX_WIDTH + x;

        if (dr3_ring_n >= DR3_RING_SAVE_MAX) return;
        if (dr3_ring_near_pickup(x, y)) continue;

        dr3_ring_idx[dr3_ring_n] = i;
        dr3_ring_val[dr3_ring_n] = ima[i];
        ++dr3_ring_n;

        ima[i] = (unsigned char)dr3_ring_colour;
    }
}

static void dr3_ring_paint(int r)
{
    const int cx = TRX_WIDTH / 2, cy = TRX_HEIGHT / 2;
    const int ro = r + DR3_ARENA_RING_THICK / 2, ri = r - DR3_ARENA_RING_THICK / 2;
    int       y;

    dr3_ring_unpaint();

    if (!TRX_IMA || (TRX_WIDTH <= 0) || (TRX_HEIGHT <= 0) || (dr3_ring_colour < 0)) return;

    {
        int n;
        dr3_ring_pk_n = 0;
        for (n = 0; n < DR3_ARENA_PICKUPS_MAX; ++n) {
            const int px = *(const int32_t *)(const void *)(___1f2488h + 0x120 * n);
            const int py = *(const int32_t *)(const void *)(___1f2488h + 0x120 * n + 4);

            if (px > 0) {
                dr3_ring_pk_x[dr3_ring_pk_n] = px;
                dr3_ring_pk_y[dr3_ring_pk_n] = py;
                ++dr3_ring_pk_n;
            }
        }
    }

    for (y = 0; y < TRX_HEIGHT; ++y) {

        const int dy = y - cy;
        int       xo, xi;

        if ((dy > ro) || (dy < -ro)) continue;

        xo = (int)sqrtf((float)(ro * ro - dy * dy));

        if ((dy >= ri) || (dy <= -ri)) { dr3_ring_span(y, cx - xo, cx + xo); continue; }

        xi = (int)sqrtf((float)(ri * ri - dy * dy));

        dr3_ring_span(y, cx - xo, cx - xi);
        dr3_ring_span(y, cx + xi, cx + xo);
    }
}

/* the line's colour: the purest bright red in the track palette outside the car ramps (55..255) */
static void dr3_ring_pick_colour(void)
{
    int   n, best = -1;
    int   best_s = -1000;

    for (n = 55; n < 256; ++n) {

        const int r = ___1a51d0h[3 * n], g = ___1a51d0h[3 * n + 1], b = ___1a51d0h[3 * n + 2];
        const int m = (g > b) ? g : b;
        const int s = r - m;

        if (s > best_s) { best_s = s; best = n; }
    }

    dr3_ring_colour = best;

    dr3_log("[dr3] chaos: arena: the ring is drawn in palette entry %d (%d,%d,%d)", best,
            (best >= 0) ? ___1a51d0h[3 * best] : 0, (best >= 0) ? ___1a51d0h[3 * best + 1] : 0, (best >= 0) ? ___1a51d0h[3 * best + 2] : 0);
}

void dr3_arena_ring_tick(void)
{
    const int ticks = (int)getCounter(1);
    const int cx    = TRX_WIDTH / 2, cy = TRX_HEIGHT / 2;
    struct_35e_t *s = dr3_arena_cars();
    int           r, phase, n;

    /* Boss Phase 1 (1 vs 20 Swarm): pure survival brawl, no closing ring */
    if (dr3_boss_is_swarm()) return;

    if (dr3_boss_is_final_duel()) {
        /* Boss Phase 3: closing ring starts only when the Giant Adversary enrages (< 50% HP) */
        if (!dr3_boss_p3_ring_active()) return;
        r     = dr3_boss_p3_ring_r();
        phase = (r <= DR3_BOSS_P3_RING_END_R) ? 2 : 1;
    }
    else {
        r     = dr3_arena_ring_radius(ticks);
        phase = dr3_arena_ring_phase(ticks);
    }

    if (phase != dr3_ring_logged_phase) {

        dr3_ring_logged_phase = phase;
        dr3_log("[dr3] chaos: arena: the ring is %s at %ds (radius %d)", (phase == 0) ? "waiting" : (phase == 1) ? "closing" : "closed",
                ticks / 70, r);
    }

    if (phase == 0) return;

    /* the ring is painted at its first size not at all (it is outside the picture), then ~7 times a second while shrinking */
    if ((r != dr3_ring_last_r) && (ticks - dr3_ring_painted_at >= 10)) {

        dr3_ring_painted_at = ticks;
        dr3_ring_last_r     = r;
        dr3_ring_paint(r);
    }

    if (dr3_chaos_race_over()) return;

    for (n = 0; n < NUM_OF_CARS; ++n) {

        int hp = S94(n, 0x18);
        int drain;

        if ((hp <= 0) || !dr3_arena_ring_outside(s[n].XLocation, s[n].YLocation, cx, cy, r)) continue;

        drain = dr3_arena_ring_drain(DR3_ARENA_HP_MAX);
        if (dr3_extreme_active()) drain *= 2;
        S94(n, 0x18) = hp - drain;
        if (S94(n, 0x18) < 0) S94(n, 0x18) = 0;

        if (S94(n, 0x18) == 0) dr3_chaos_wreck(n, hp, "the closing ring");
    }

    if (((ticks % 700) == 0) && dr3_log_enabled()) {

        int out = 0;

        for (n = 0; n < NUM_OF_CARS; ++n)
            if (dr3_arena_alive(n) && dr3_arena_ring_outside(s[n].XLocation, s[n].YLocation, cx, cy, r)) ++out;

        dr3_log("[dr3] chaos: arena: ring radius %d at %ds, %d car(s) outside it", r, ticks / 70, out);
    }
}

int dr3_arena_banner_now(const char **line1, const char **line2)
{
    const int          ticks = (int)getCounter(1);
    const int          me    = (int)MY_CAR_IDX;
    int                outside = 0;

    if (!dr3_chaos_arena() || (me < 0) || (me >= NUM_OF_CARS)) return 0;
    if (dr3_boss_is_swarm()) return 0;

    if (dr3_boss_is_final_duel()) {
        if (!dr3_boss_p3_ring_active()) return 0;
        if (dr3_arena_alive(me)) {
            const struct_35e_t *s = dr3_arena_cars();
            outside = dr3_arena_ring_outside(s[me].XLocation, s[me].YLocation,
                                             TRX_WIDTH / 2, TRX_HEIGHT / 2, dr3_boss_p3_ring_r());
        }
        if (outside) {
            if (((ticks / 18) & 1) == 0) return 0;
            *line1 = "GET INSIDE";
            *line2 = "THE RING";
            return 2;
        }
        return 0;
    }

    if (dr3_arena_alive(me)) {

        const struct_35e_t *s = dr3_arena_cars();

        outside = dr3_arena_ring_outside(s[me].XLocation, s[me].YLocation, TRX_WIDTH / 2, TRX_HEIGHT / 2, dr3_arena_ring_radius(ticks));
    }

    /* Opening grudge callout during the ring grace period (ticks 30..490 = ~0.4s..7.0s, before ring closes at 12s) */
    if ((dr3_grudge_car >= 0) && (dr3_grudge_name[0] != '\0') && (ticks >= 30) && (ticks < 490)) {
        *line1 = dr3_grudge_name;
        *line2 = "IS MAD AT YOU!";
        return 3;
    }

    return dr3_arena_banner(ticks, outside, line1, line2);
}

void dr3_arena_setup(void)
{
    struct_35e_t *s = dr3_arena_cars();
    int           n;

    dr3_ring_n = 0;                    /* the track picture is decoded afresh for every race: nothing to restore */
    dr3_ring_painted_at = -1000;
    dr3_ring_last_r = -1;
    dr3_ring_logged_phase = -1;
    dr3_grudge_car = -1;
    dr3_grudge_name[0] = '\0';
    dr3_ring_pick_colour();

    for (n = 0; n < DR3_MAX_CARS; ++n) {

        dr3_ai[n].target = -1;
        dr3_ai[n].retarget = n * 5;            /* staggered: not every car thinks in the same frame */
        dr3_ai[n].goal_item = -1;
        dr3_ai[n].item_last = -1;
        dr3_ai[n].item_frames = 0;
        { int k; for (k = 0; k < DR3_ARENA_PICKUPS_MAX; ++k) dr3_ai[n].ban_until[k] = 0; }
        dr3_ai[n].heading = 0.0f;
        dr3_ai[n].plan_age = DR3_ARENA_PLAN_EVERY;
        dr3_ai[n].still = 0;
        dr3_ai[n].px = dr3_ai[n].py = -1000.0f;
        dr3_ai[n].stuck_n = 0;
        dr3_ai[n].reverse = 0;
        dr3_ai[n].reverse_turn = 1;
        dr3_ai[n].mine_wait = 120 + n * 7;
        dr3_ai[n].blocked = 0;
        dr3_ai[n].died_at = 0;
    }

    for (n = 0; n < NUM_OF_CARS; ++n) {

        const int pos = (dr3_chaos_car_pos[n] >= 0 && dr3_chaos_car_pos[n] < NUM_OF_CARS) ? dr3_chaos_car_pos[n] : n;
        int       x, y, img;

        if (dr3_boss_is_swarm()) {
            /* Phase 1: player (MY_CAR_IDX) starts center-south; 20 Vagabonds surround from bays + walls */
            int swarm_idx = 0;
            if (n != (int)MY_CAR_IDX) swarm_idx = (n < (int)MY_CAR_IDX) ? (n + 1) : n;
            dr3_boss_swarm_spawn(swarm_idx, TRX_WIDTH, TRX_HEIGHT, &x, &y, &img);
        }
        else if (dr3_boss_is_final_duel()) {
            dr3_boss_phase3_spawn(n == 0, TRX_WIDTH, TRX_HEIGHT, &x, &y, &img);
        }
        else {
            dr3_arena_spawn(pos, TRX_WIDTH, TRX_HEIGHT, &x, &y, &img);
        }

        s[n].XLocation = (float)x;
        s[n].YLocation = (float)y;
        s[n].__14e     = (float)x;
        s[n].__152     = (float)y;
        s[n].ImgIndex  = (__DWORD__)img;
        s[n].Direction = 3.75f * (float)img;
        s[n].__156     = s[n].Direction;
        s[n].ImgOffset = (__DWORD__)(0x640 * img + 0x25800 * n);
        s[n].__b0      = 0.0f;

        /* every car brings the gun and real drop mines (s_94 +0x28): an arena is a fight! */
        S94(n, 0x2c) = 1;
        S94(n, 0x30) = DR3_ARENA_AMMO_MAX;

        if (dr3_boss_is_swarm()) {
            if (n == (int)MY_CAR_IDX) {
                if (S94(n, 0x28) < 5) S94(n, 0x28) = 5;
            }
            else {
                S94(n, 0x18) = dr3_boss_p1_enemy_hp();
                S94(n, 0x28) = dr3_extreme_active() ? 2 : 1;
            }
        }
        else if (dr3_boss_is_final_duel()) {
            if (n == 0) {
                /* Giant Boss Deliverator: 3x HP (4x EXTREME), full ammo/turbo, 8+ starting mines */
                S94(n, 0x18) = dr3_boss_p3_hp_max();
                S94(n, 0x34) = DR3_ARENA_AMMO_MAX;
                if (S94(n, 0x28) < DR3_BOSS_P3_START_MINES) S94(n, 0x28) = DR3_BOSS_P3_START_MINES;
            }
            else {
                if (S94(n, 0x28) < 5) S94(n, 0x28) = 5;
            }
        }
        else {
            if (S94(n, 0x28) < 3) S94(n, 0x28) = 3;
            if (n == dr3_chaos_boss_car()) {
                if (S94(n, 0x28) < 5) S94(n, 0x28) = 5;
            }
        }

        dr3_log("[dr3] chaos: arena: car %2d (%s) starts at %d,%d facing %.0f deg (img %d, hp %d, mines %d)",
                n, dr3_arena_name(n), x, y, (double)s[n].Direction, img, (int)S94(n, 0x18), (int)S94(n, 0x28));
    }

    if (!dr3_boss_is_swarm() && !dr3_boss_is_final_duel()) {
        dr3_grudge_car = dr3_arena_pick_grudge(NUM_OF_CARS, (int)MY_CAR_IDX, rand_watcom106());
        if ((dr3_grudge_car >= 0) && (dr3_grudge_car < NUM_OF_CARS)) {
            const char *nm = dr3_arena_name(dr3_grudge_car);
            snprintf(dr3_grudge_name, sizeof(dr3_grudge_name), "%s", (nm && nm[0]) ? nm : "RIVAL");
            dr3_log("[dr3] chaos: arena: grudge rival is car %d (%s) -> '%s IS MAD AT YOU!'",
                    dr3_grudge_car, dr3_grudge_name, dr3_grudge_name);
        }
    }

    if (dr3_chaos_boss_car() >= 0) {
        dr3_log("[dr3] chaos: arena: ADVERSARY spawned in car %d (corner 0, slot 0, 5 mines)",
                dr3_chaos_boss_car());
    }

    dr3_log("[dr3] chaos: arena: %d cars on %s (%dx%d), the gun, %d mines and full ammunition for everybody",
            NUM_OF_CARS, DR3_ARENA_TRACK, TRX_WIDTH, TRX_HEIGHT, 3);
}

/* the nearest enemy in sight (clear line of sight wins, healthy leaders are not ignored); hysteresis keeps a hunt going */
static int dr3_arena_pick_target(int car)
{
    const struct_35e_t *s = dr3_arena_cars();
    const int cur = dr3_ai[car].target;
    int       j, best = -1;
    float     best_w = 1.0e30f, cur_w = 1.0e30f;

    /* Boss Phase 1 (1 vs 20 Swarm): every Vagabond hunts ONLY the player */
    if (dr3_boss_is_swarm() && (car != (int)MY_CAR_IDX)) {
        return dr3_arena_alive((int)MY_CAR_IDX) ? (int)MY_CAR_IDX : -1;
    }

    /* Regular FFA Arena grudge rival: stays locked onto the player until one of them is wrecked */
    if ((car == dr3_grudge_car) && dr3_arena_alive((int)MY_CAR_IDX)) {
        return (int)MY_CAR_IDX;
    }

    for (j = 0; j < NUM_OF_CARS; ++j) {

        float dx, dy, w, hp_frac;

        if ((j == car) || !dr3_arena_alive(j)) continue;

        dx = s[j].XLocation - s[car].XLocation;
        dy = s[j].YLocation - s[car].YLocation;
        w  = sqrtf(dx * dx + dy * dy);

        /* targets in clear line of sight are much better than someone behind a bunker */
        if (dr3_arena_los(dr3_arena_solid, NULL, s[car].XLocation, s[car].YLocation, s[j].XLocation, s[j].YLocation))
            w *= 0.72f;

        /* strong killer instinct on wounded prey (< 35% HP) and healthy leaders (> 75% HP) so nobody cruises untouched */
        hp_frac = (float)S94(j, 0x18) / (float)DR3_ARENA_HP_MAX;
        if (hp_frac < 0.35f) w *= 0.72f;
        else if (hp_frac > 0.75f) w *= 0.84f;

        if (w < best_w) { best_w = w; best = j; }
        if (j == cur) cur_w = w;
    }

    if ((cur >= 0) && (cur_w < best_w * 1.20f)) return cur;      /* keep the hunt unless somebody is clearly better prey */

    return best;
}

/* the item it wants: what it lacks and lies within reach - or any item that is almost on the way */
static int dr3_arena_pick_item(int car, float tdist)
{
    const struct_35e_t *s = dr3_arena_cars();
    const int is_grudge = (car == dr3_grudge_car) && dr3_arena_alive((int)MY_CAR_IDX);
    const int ammo_low  = (S94(car, 0x30) < DR3_ARENA_AMMO_MAX * (is_grudge ? 20 : 55) / 100);
    const int now       = (int)getCounter(1);
    const int hp_max_ref = dr3_boss_is_giant_car(car) ? dr3_boss_p3_hp_max()
                         : (dr3_boss_is_swarm() && (car != (int)MY_CAR_IDX)) ? dr3_boss_p1_enemy_hp()
                         : DR3_ARENA_HP_MAX;
    const int hp_low    = (S94(car, 0x18) < hp_max_ref * (is_grudge ? 25 : 50) / 100);
    const int mines_low = (S94(car, 0x28) <= (is_grudge ? 0 : 1));
    const int turbo_low = (S94(car, 0x34) < DR3_ARENA_AMMO_MAX / 4);
    const int ring_on   = dr3_boss_is_swarm() ? 0
                        : dr3_boss_is_final_duel() ? dr3_boss_p3_ring_active()
                        : (dr3_arena_ring_phase(now) >= 1);
    const int ring_r    = dr3_boss_is_final_duel() ? dr3_boss_p3_ring_r() : dr3_arena_ring_radius(now);
    int       n, best = -1;
    float     best_d = 1.0e30f;

    /* grudge rival stays glued to the player unless critically dry on ammo or near death */
    if (is_grudge && !ammo_low && !hp_low && !mines_low) return -1;

    for (n = 0; n < DR3_ARENA_PICKUPS_MAX; ++n) {

        const int32_t kind = *(const int32_t *)(const void *)(___1f2488h + 0x120 * n + 8);
        float         dx, dy, d, want;

        if (kind <= 0) continue;
        if (dr3_ai[car].ban_until[n] > now) continue;               /* it could not get that one */

        dx = (float)*(const int32_t *)(const void *)(___1f2488h + 0x120 * n) - s[car].XLocation;
        dy = (float)*(const int32_t *)(const void *)(___1f2488h + 0x120 * n + 4) - s[car].YLocation;
        d  = sqrtf(dx * dx + dy * dy);

        /* an item the closing ring has already passed is not worth the HP */
        if (ring_on &&
            dr3_arena_ring_outside(s[car].XLocation + dx, s[car].YLocation + dy, TRX_WIDTH / 2, TRX_HEIGHT / 2,
                                   ring_r - 12)) continue;

        /* how much the car wants this one: a need makes it come from far, a convenience only from near */
        want = 170.0f;
        if (ammo_low && (kind == DR3_ITEM_AMMO)) want = 720.0f;
        if (hp_low && ((kind == DR3_ITEM_REPAIR_BIG) || (kind == DR3_ITEM_REPAIR_SMALL))) want = 720.0f;
        if (mines_low && (kind == DR3_ITEM_MINE_CRATE)) want = 420.0f;
        if (turbo_low && (kind == DR3_ITEM_TURBO)) want = 310.0f;
        if (kind == DR3_ITEM_MONEY_BIG) want = 220.0f;
        if (kind == DR3_ITEM_MONEY) want = 140.0f;

        if ((want <= 220.0f) && (d > tdist * 0.55f)) continue;      /* not worth a detour from the hunt */
        if (d > want) continue;

        if (d < best_d) { best_d = d; best = n; }
    }

    return best;
}

void dr3_arena_ai(int car)
{
    struct_35e_t *s = dr3_arena_cars();
    dr3_arena_ai_t *a = &dr3_ai[car];
    unsigned       c = 0;
    float          gx, gy, want, tdist = 1.0e9f, speed;
    int            j, steer, ticks = (int)getCounter(1);

    if (!dr3_arena_alive(car)) { s[car].Ctrls[getCounter(4)] = 0; return; }

    speed = s[car].__b0 < 0.0f ? -s[car].__b0 : s[car].__b0;

    /* --- whom to hunt, and whether something on the floor is worth the detour */
    if (--a->retarget <= 0 || (a->target >= 0 && !dr3_arena_alive(a->target))) {

        a->retarget = DR3_ARENA_RETARGET;
        a->target   = dr3_arena_pick_target(car);
    }

    if (a->target >= 0) {

        const float dx = s[a->target].XLocation - s[car].XLocation, dy = s[a->target].YLocation - s[car].YLocation;

        tdist = sqrtf(dx * dx + dy * dy);
    }

    if (a->retarget == DR3_ARENA_RETARGET) a->goal_item = dr3_arena_pick_item(car, tdist);

    /* the goal: an item while it has one in mind (and it is still there), else the prey a little ahead of where he is */
    if ((a->goal_item >= 0) && (*(const int32_t *)(const void *)(___1f2488h + 0x120 * a->goal_item + 8) <= 0)) a->goal_item = -1;

    /*
     * A car that has been after the same item for DR3_ARENA_ITEM_GIVE_UP frames does not get it - it circles it
     * (the turn radius at speed is bigger than the pickup) or something is in the way.  It leaves that item alone
     * for a while and hunts instead.
     */
    if (a->goal_item != a->item_last) { a->item_last = a->goal_item; a->item_frames = 0; }

    if ((a->goal_item >= 0) && (++a->item_frames > DR3_ARENA_ITEM_GIVE_UP)) {

        dr3_log("[dr3] chaos: arena: car %d (%s) gives up on item place %d after %d frames", car, dr3_arena_name(car), a->goal_item, a->item_frames);
        a->ban_until[a->goal_item] = ticks + DR3_ARENA_ITEM_BAN;
        a->goal_item  = -1;
        a->item_last  = -1;
        a->item_frames = 0;
    }

    if (a->goal_item >= 0) {

        gx = (float)*(const int32_t *)(const void *)(___1f2488h + 0x120 * a->goal_item);
        gy = (float)*(const int32_t *)(const void *)(___1f2488h + 0x120 * a->goal_item + 4);
    }
    else if (a->target >= 0) {

        const struct_35e_t *t = &s[a->target];
        const float lead = (tdist > 90.0f) ? 10.0f : (tdist * 0.08f);
        const float trad = (t->Direction + 180.0f) * 3.14159265f / 180.0f;

        gx = t->XLocation + lead * t->__b0 * sinf(trad);
        gy = t->YLocation + lead * t->__b0 * cosf(trad) * (5.0f / 6.0f);
    }
    else {

        gx = (float)(TRX_WIDTH / 2);      /* nobody left to hunt: wait in the middle */
        gy = (float)(TRX_HEIGHT / 2);
    }

    /*
     * The closing ring: a car near its edge or outside it goes for the middle - the hunt and the items wait.  (It
     * still shoots at whatever is in front of the gun below.)
     */
    {
        const int ring_on = dr3_boss_is_swarm() ? 0
                          : dr3_boss_is_final_duel() ? dr3_boss_p3_ring_active()
                          : (dr3_arena_ring_phase(ticks) >= 1);
        const int ring_r  = dr3_boss_is_final_duel() ? dr3_boss_p3_ring_r() : dr3_arena_ring_radius(ticks);

        if (ring_on &&
            dr3_arena_ring_outside(s[car].XLocation, s[car].YLocation, TRX_WIDTH / 2, TRX_HEIGHT / 2,
                                   dr3_arena_ring_safe_r(ring_r))) {

            gx = (float)(TRX_WIDTH / 2);
            gy = (float)(TRX_HEIGHT / 2);
        }
    }

    /* engine movement and gunfire scale y by 5/6 (race___4c434h.c, race___4ff50h.c): compensate dy by 6/5 */
    want = dr3_arena_bearing(gx - s[car].XLocation, (gy - s[car].YLocation) * 1.2f);

    /* --- backing out of a corner */
    if (a->reverse > 0) {

        --a->reverse;
        c = CTRL_BRAKE | ((a->reverse_turn > 0) ? CTRL_STEER_LEFT : CTRL_STEER_RIGHT);
        s[car].Ctrls[getCounter(4)] = c;
        return;
    }

    /* --- the way: the wanted heading when there is room for it, else the best one round it */
    if (++a->plan_age >= DR3_ARENA_PLAN_EVERY) {

        a->plan_age = 0;
        a->heading  = dr3_arena_plan(dr3_arena_solid, NULL, s[car].XLocation, s[car].YLocation, s[car].Direction,
                                     want, &a->blocked, 62);
    }

    steer = dr3_arena_steer(s[car].Direction, a->heading, 2.0f);
    if (steer > 0) c |= CTRL_STEER_LEFT;
    if (steer < 0) c |= CTRL_STEER_RIGHT;

    {
        float off = dr3_arena_wrap180(a->heading - s[car].Direction);
        float idist = 1.0e9f;
        int   tracking_shot = 0;

        if (off < 0.0f) off = -off;

        if (a->goal_item >= 0) {

            const float ix = gx - s[car].XLocation, iy = gy - s[car].YLocation;

            idist = sqrtf(ix * ix + iy * iy);
        }
        else if ((a->target >= 0) && (S94(car, 0x30) > 0) && (tdist < 145.0f) && (off < 22.0f) &&
                 dr3_arena_los(dr3_arena_solid, NULL, s[car].XLocation, s[car].YLocation,
                               s[a->target].XLocation, s[a->target].YLocation)) {

            tracking_shot = 1;
        }

        if (a->blocked) {

            /* wall close ahead: brake hard instead of slamming into it for 10% HP damage */
            if (speed > 1.7f) c |= CTRL_BRAKE;
            else if ((off < 40.0f) && (speed < 0.9f)) c |= CTRL_ACCELERATE;
        }
        else if ((idist < 90.0f) && (off > 25.0f) && (speed > 1.2f)) {

            /* close to an item and not pointing at it: slow down and turn in, instead of orbiting it at speed */
            if (speed > 2.0f) c |= CTRL_BRAKE;
        }
        else if (tracking_shot && (speed > 2.6f)) {

            /* lining up a close-range gun burst: ease off instead of overshooting into the far wall */
            if (speed > 3.2f) c |= CTRL_BRAKE;
        }
        else if ((off < 75.0f) || (speed < 1.3f)) c |= CTRL_ACCELERATE;
        else if (speed > 3.2f) c |= CTRL_BRAKE;

        /* aggressive turbo: pursuit on clear runs AND close-range ramming (spikes, low-HP prey, grudge rival, or out of ammo) */
        if ((c & CTRL_ACCELERATE) && !a->blocked && (a->goal_item < 0) && (a->target >= 0) &&
            (S94(car, 0x34) > DR3_ARENA_AMMO_MAX / ((car == dr3_grudge_car) ? 10 : 6))) {

            const int is_grudge = (car == dr3_grudge_car) && (a->target == (int)MY_CAR_IDX);
            const int ram_ok = (tdist < 115.0f) && (off < (is_grudge ? 18.0f : 16.0f)) &&
                               (is_grudge || (S94(car, 0x30) <= 0) || (S94(car, 0x24) != 0) ||
                                (S94(a->target, 0x18) < DR3_ARENA_HP_MAX * 35 / 100) ||
                                (S94(car, 0x18) > DR3_ARENA_HP_MAX * 45 / 100));
            const int chase_ok = (tdist > (is_grudge ? 80.0f : 130.0f)) && (off < (is_grudge ? 16.0f : 12.0f));

            if (ram_ok || chase_ok) c |= CTRL_TURBO_BOOST;
        }
    }

    /*
     * Stuck: it has not moved DR3_ARENA_STUCK_PX px in DR3_ARENA_STUCK_FRAMES frames while the clock runs.  The
     * position is the measure, not the speed - a car pressed against a wall keeps "speed" from its own gas
     * pedal while it goes nowhere.  Backing out gets longer every time in a row, and the way it turns while
     * backing out changes, so a car wedged in a corner works its way out instead of repeating the same move.
     */
    if (ticks > 0xc8) {

        if (++a->still >= DR3_ARENA_STUCK_FRAMES) {

            const float mx = s[car].XLocation - a->px, my = s[car].YLocation - a->py;

            if ((mx * mx + my * my) < (float)(DR3_ARENA_STUCK_PX * DR3_ARENA_STUCK_PX)) {

                ++a->stuck_n;
                a->reverse      = DR3_ARENA_REVERSE + 12 * ((a->stuck_n > 4) ? 4 : a->stuck_n);
                a->reverse_turn = ((a->stuck_n & 1) ? 1 : -1) * ((steer >= 0) ? 1 : -1);
                dr3_log("[dr3] chaos: arena: car %d (%s) is stuck at %d,%d - backs out for %d frames (#%d)", car, dr3_arena_name(car),
                        (int)s[car].XLocation, (int)s[car].YLocation, a->reverse, a->stuck_n);

                /*
                 * The engine lets a car press itself a few pixels into a wall (its collision pushes it out by the
                 * contact point, not by the depth) and then nothing moves it any more: it is not a matter of
                 * backing out.  From the second try on it is set down a car's length into the free room.
                 */
                if (a->stuck_n >= 2) {

                    const float h = dr3_arena_plan(dr3_arena_solid, NULL, s[car].XLocation, s[car].YLocation, s[car].Direction,
                                                   dr3_arena_bearing((float)(TRX_WIDTH / 2) - s[car].XLocation,
                                                                     (float)(TRX_HEIGHT / 2) - s[car].YLocation), NULL, 0);
                    const float hr = h * 3.14159265f / 180.0f;
                    const float nx = s[car].XLocation - sinf(hr) * 16.0f, ny = s[car].YLocation - cosf(hr) * 16.0f;

                    if (!dr3_arena_solid(NULL, (int)nx, (int)ny)) {

                        s[car].XLocation = s[car].__14e = nx;
                        s[car].YLocation = s[car].__152 = ny;
                        s[car].__b0 = 0.0f;
                        dr3_log("[dr3] chaos: arena: car %d is set down at %d,%d (heading %.0f)", car, (int)nx, (int)ny, (double)h);
                    }
                }
            }
            else a->stuck_n = 0;

            a->still = 0;
            a->px = s[car].XLocation;
            a->py = s[car].YLocation;
        }
    }

    /* --- shooting: only when a car is actually on the gun's 5/6-scaled line of fire AND no wall sits between */
    if ((ticks > 0x1ae) && (S94(car, 0x30) > 0)) {

        const float rad = s[car].Direction * DR3_PI / 180.0f;
        const float fx  = -sinf(rad), fy = -cosf(rad);
        const float cx  = s[car].XLocation, cy = s[car].YLocation;
        const float lat = dr3_boss_is_giant_car(car) ? 20.0f : ((car == dr3_grudge_car) ? 18.0f : 16.0f);
        const int   swarm_enemy = dr3_boss_is_swarm() && (car != (int)MY_CAR_IDX);

        for (j = 0; j < NUM_OF_CARS; ++j) {

            float dx, dy, along, side;

            if ((j == car) || !dr3_arena_alive(j)) continue;
            if (swarm_enemy && (j != (int)MY_CAR_IDX)) continue;
            dx    = s[j].XLocation - cx;
            dy    = (s[j].YLocation - cy) * 1.2f;
            along = dx * fx + dy * fy;
            if ((along < 8.0f) || (along > (float)DR3_ARENA_GUN_RANGE)) continue;
            side  = dx * fy - dy * fx;
            if ((side >= lat) || (side <= -lat)) continue;
            if (dr3_arena_los(dr3_arena_solid, NULL, cx, cy, s[j].XLocation, s[j].YLocation)) {
                c |= CTRL_MACHINE_GUN;
                break;
            }
        }
    }

    /* --- mines: for whoever chases it OR when surrounded in a close melee (uses real mine count at s_94 +0x28!) */
    if ((a->mine_wait > 0)) --a->mine_wait;

    if ((a->mine_wait <= 0) && (S94(car, 0x28) > 0) && (ticks > 0x1ae)) {

        const int swarm_enemy = dr3_boss_is_swarm() && (car != (int)MY_CAR_IDX);
        const int mine_cd     = (dr3_boss_is_giant_car(car) && dr3_boss_enraged()) ? 65
                              : (car == dr3_grudge_car) ? 75 : DR3_ARENA_MINE_COOLDOWN;
        int       drop        = 0;
        int       near_pack   = 0;

        for (j = 0; j < NUM_OF_CARS; ++j) {

            float mdx, mdy;

            if ((j == car) || !dr3_arena_alive(j)) continue;
            if (swarm_enemy && (j != (int)MY_CAR_IDX)) continue;
            if (dr3_arena_behind(s[car].XLocation, s[car].YLocation, s[car].Direction,
                                 s[j].XLocation, s[j].YLocation, 145, 62.0f)) {
                drop = 1;
                break;
            }
            mdx = s[j].XLocation - s[car].XLocation;
            mdy = s[j].YLocation - s[car].YLocation;
            if ((mdx * mdx + mdy * mdy) < (95.0f * 95.0f)) ++near_pack;
        }

        if (!drop && (near_pack >= 2) && (speed > 0.7f)) drop = 1;

        if (drop) {
            c &= ~CTRL_BRAKE;    /* race___514d8h suppresses mine drop while BRAKE is also set */
            c |= CTRL_DROP_MINE;
            a->mine_wait = mine_cd;
        }
    }

    s[car].Ctrls[getCounter(4)] = c;
}

void dr3_arena_note_deaths(void)
{
    int n, left = 0;
    const int ticks = (int)getCounter(1);

    dr3_boss_tick();
    dr3_arena_ring_tick();

    for (n = 0; n < NUM_OF_CARS; ++n) if (dr3_arena_alive(n)) ++left;

    /*
     * The standings the HUD shows (the "1st 2nd 3rd" board and the player's own place) are the engine's Position
     * of every car, which in a race is the lap counter's business - here it is who outlives whom, kept live, so
     * the board reads the order of the fight.  Twice a second is plenty.
     */
    if ((ticks % 35) == 0) {

        int hp[DR3_MAX_CARS], died[DR3_MAX_CARS], place[DR3_MAX_CARS];

        if (NUM_OF_CARS <= DR3_MAX_CARS) {

            for (n = 0; n < NUM_OF_CARS; ++n) { hp[n] = S94(n, 0x18); died[n] = dr3_ai[n].died_at; }

            dr3_arena_rank(NUM_OF_CARS, hp, died, place);

            for (n = 0; n < NUM_OF_CARS; ++n) dr3_arena_cars()[n].Position = (unsigned char)place[n];
        }
    }

    if (((ticks % 350) == 0) && dr3_log_enabled()) {

        const struct_35e_t *s = dr3_arena_cars();
        char                line[700];
        int                 pos = 0;

        for (n = 0; (n < NUM_OF_CARS) && (pos < 560); ++n)
            pos += snprintf(line + pos, sizeof(line) - (size_t)pos, " %d:%d%%/a%d/s%.1f@%d,%d", n,
                            (int)(S94(n, 0x18) * 100 / DR3_ARENA_HP_MAX), (int)(S94(n, 0x30) * 100 / DR3_ARENA_AMMO_MAX), (double)s[n].__b0,
                            (int)s[n].XLocation, (int)s[n].YLocation);

        dr3_log("[dr3] chaos: arena: %ds, %d left -%s", ticks / 70, left, line);
    }

    for (n = 0; n < NUM_OF_CARS; ++n) {

        if (dr3_arena_alive(n)) {
            /* If a wreck gets bumped into a repair pickup and revives, clear its old death stamp */
            if (dr3_ai[n].died_at != 0) {
                dr3_ai[n].died_at = 0;
                dr3_log("[dr3] chaos: arena: car %d (%s) revived via repair at %ds!",
                        n, dr3_arena_name(n), ticks / 70);
            }
            continue;
        }

        if (dr3_ai[n].died_at != 0) continue;

        dr3_ai[n].died_at = (int)getCounter(1) + 1;

        {
            const struct_35e_t *s = dr3_arena_cars();

            dr3_log("[dr3] chaos: arena: car %d (%s) is out at %ds - %d car(s) left (at %d,%d, speed %.1f, heading %.0f, room ahead %d px)",
                    n, dr3_arena_name(n), (int)getCounter(1) / 70, left, (int)s[n].XLocation, (int)s[n].YLocation,
                    (double)s[n].__b0, (double)s[n].Direction,
                    dr3_arena_room(dr3_arena_solid, NULL, s[n].XLocation, s[n].YLocation, s[n].Direction, DR3_ARENA_LOOK));
        }
    }
}

void dr3_arena_final_places(void)
{
    struct_35e_t *s = dr3_arena_cars();
    int           hp[DR3_MAX_CARS], died[DR3_MAX_CARS], place[DR3_MAX_CARS];
    int           n;

    if (NUM_OF_CARS > DR3_MAX_CARS) return;

    for (n = 0; n < NUM_OF_CARS; ++n) {

        hp[n]   = S94(n, 0x18);
        died[n] = dr3_ai[n].died_at;
    }

    dr3_arena_rank(NUM_OF_CARS, hp, died, place);

    for (n = 0; n < NUM_OF_CARS; ++n) {

        s[n].Position = (unsigned char)place[n];

        dr3_log("[dr3] chaos: arena: place %2d - car %2d (%s), %s (HP %d)", place[n], n, dr3_arena_name(n),
                (hp[n] > 0) ? "survived" : "out", hp[n]);
    }
}

int dr3_arena_roll_item(int roll)
{
    return dr3_arena_item_for_roll(roll);
}

#endif /* DR3_CHAOS */
