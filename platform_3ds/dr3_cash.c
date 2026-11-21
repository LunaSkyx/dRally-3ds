/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#include "dr3_cash.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int dr3_cash_lr1_dev(unsigned char lr1)
{
    int b, d;

    /* TRX_LR1 == 0 or >= 16 sits outside the painted road corridor (sidewalks/plazas behind barriers) */
    if ((lr1 == 0) || (lr1 >= 16)) return -1;

    b = 16 - (int)lr1;
    d = b - 8;

    return (d < 0) ? -d : d;
}

int dr3_cash_spot_ok(const unsigned char *mask, int w, int h, int x, int y)
{
    int dx, dy;

    if (!mask || (w < 48) || (h < 48)) return 0;
    if ((x < 24) || (y < 24) || (x >= w - 24) || (y >= h - 24)) return 0;

    for (dy = -12; dy <= 12; dy += 3) {
        for (dx = -12; dx <= 12; dx += 3) {
            if ((mask[(y + dy) * w + (x + dx)] & 0x0f) < 4) return 0;
        }
    }

    return 1;
}

static int dr3_cash_qcell_passable(const unsigned char *mask, int w, int h, int qx, int qy)
{
    const int x0 = qx << 2;
    const int y0 = qy << 2;

    if ((x0 < 2) || (y0 < 2) || (x0 + 3 >= w - 2) || (y0 + 3 >= h - 2)) return 0;

    if ((mask[(y0 + 0) * w + (x0 + 0)] & 0x0f) < 4) return 0;
    if ((mask[(y0 + 0) * w + (x0 + 3)] & 0x0f) < 4) return 0;
    if ((mask[(y0 + 3) * w + (x0 + 0)] & 0x0f) < 4) return 0;
    if ((mask[(y0 + 3) * w + (x0 + 3)] & 0x0f) < 4) return 0;
    if ((mask[(y0 + 2) * w + (x0 + 2)] & 0x0f) < 4) return 0;

    return 1;
}

/* Quarter-res BFS from starting cars: proves a candidate spot is reachable without crossing a wall. */
static unsigned char *dr3_cash_build_reach(const unsigned char *mask, int w, int h,
                                           const int *seed_x, const int *seed_y, int n_seed)
{
    const int      wq    = w >> 2;
    const int      hq    = h >> 2;
    const int      cells = wq * hq;
    unsigned char *vis;
    int           *queue;
    int            head = 0, tail = 0, i;

    if ((wq <= 0) || (hq <= 0) || !seed_x || !seed_y || (n_seed <= 0)) return NULL;

    vis   = (unsigned char *)calloc((size_t)cells, 1);
    queue = (int *)malloc((size_t)cells * sizeof(int));
    if (!vis || !queue) {
        free(vis);
        free(queue);
        return NULL;
    }

    for (i = 0; i < n_seed; ++i) {
        const int sx = seed_x[i] >> 2;
        const int sy = seed_y[i] >> 2;
        int       dy, dx;

        for (dy = -2; dy <= 2; ++dy) {
            for (dx = -2; dx <= 2; ++dx) {
                const int qx = sx + dx, qy = sy + dy;
                if ((qx < 0) || (qy < 0) || (qx >= wq) || (qy >= hq)) continue;
                if (vis[qy * wq + qx]) continue;
                if (!dr3_cash_qcell_passable(mask, w, h, qx, qy)) continue;
                vis[qy * wq + qx] = 1;
                queue[tail++] = qy * wq + qx;
            }
        }
    }

    while (head < tail) {
        static const int d4[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
        const int cur = queue[head++];
        const int cx  = cur % wq;
        const int cy  = cur / wq;

        for (i = 0; i < 4; ++i) {
            const int nx = cx + d4[i][0];
            const int ny = cy + d4[i][1];
            int       idx;

            if ((nx < 0) || (ny < 0) || (nx >= wq) || (ny >= hq)) continue;
            idx = ny * wq + nx;
            if (vis[idx]) continue;
            if (!dr3_cash_qcell_passable(mask, w, h, nx, ny)) continue;

            vis[idx] = 1;
            queue[tail++] = idx;
        }
    }

    free(queue);
    if (tail == 0) { free(vis); return NULL; }
    return vis;
}

static int dr3_cash_far_enough(int x, int y, const int *px, const int *py, int n, int min_d)
{
    const int min2 = min_d * min_d;
    int       i;

    for (i = 0; i < n; ++i) {
        const int dx = x - px[i];
        const int dy = y - py[i];

        if ((dx * dx + dy * dy) < min2) return 0;
    }

    return 1;
}

int dr3_cash_place(const unsigned char *mask, const unsigned char *lr1, int w, int h,
                   const int *avoid_x, const int *avoid_y, int n_avoid,
                   const int *pad_x, const int *pad_y, int n_pad,
                   int *out_x, int *out_y, int max_spots)
{
    enum { MAX_CAND = 512 };
    const int      wq    = w >> 2;
    const int      hq    = h >> 2;
    unsigned char *reach = NULL;
    int            cand_x[MAX_CAND], cand_y[MAX_CAND], cand_tier[MAX_CAND];
    int            n_cand = 0;
    int            tier, count = 0;
    int            y, x, i, j;

    if (!mask || !lr1 || !out_x || !out_y || (max_spots <= 0) || (wq <= 0) || (hq <= 0)) return 0;

    reach = dr3_cash_build_reach(mask, w, h, avoid_x, avoid_y, n_avoid);

    /*
     * Gather reachable off-line candidates across the ENTIRE track (never top-biased):
     *   tier 0: road edges/shoulders cleanly off AI's 7..9 center line (dev in 3..5)
     *   tier 1: wider road edge fallback (dev in 2..6)
     */
    for (tier = 0; tier < 2; ++tier) {
        const int min_dev = (tier == 0) ? 3 : 2;
        const int max_dev = (tier == 0) ? 5 : 6;
        const int step    = (w > 700 || h > 700) ? 20 : 16;

        for (y = 28; (y < h - 28) && (n_cand < MAX_CAND); y += step) {
            const int qy = y >> 2;
            if ((qy < 0) || (qy >= hq)) continue;

            for (x = 28; (x < w - 28) && (n_cand < MAX_CAND); x += step) {
                const int qx = x >> 2;
                int       dev;

                if ((qx < 0) || (qx >= wq)) continue;
                if (reach && !reach[qy * wq + qx]) continue;

                dev = dr3_cash_lr1_dev(lr1[qy * wq + qx]);
                if ((dev < min_dev) || (dev > max_dev)) continue;

                if (!dr3_cash_spot_ok(mask, w, h, x, y)) continue;
                if (pad_x && pad_y && (n_pad > 0) &&
                    !dr3_cash_far_enough(x, y, pad_x, pad_y, n_pad, DR3_CASH_PAD_CLEAR)) continue;
                if (avoid_x && avoid_y && (n_avoid > 0) &&
                    !dr3_cash_far_enough(x, y, avoid_x, avoid_y, n_avoid, DR3_CASH_CAR_CLEAR)) continue;

                cand_x[n_cand]    = x;
                cand_y[n_cand]    = y;
                cand_tier[n_cand] = tier;
                ++n_cand;
            }
        }
    }

    free(reach);
    if (n_cand <= 0) return 0;

    /*
     * Farthest-point selection: starts with a Tier-0 candidate and repeatedly picks whichever
     * candidate is farthest from all already-placed spots, spreading the 20 $ pickups uniformly
     * around the ENTIRE circuit (both top and bottom halves, corners and straights).
     */
    {
        const int min2 = DR3_CASH_MIN_SEP * DR3_CASH_MIN_SEP;
        int       min_d2[MAX_CAND];

        /* seed with middle candidate (preferring tier 0) */
        int first = n_cand / 2;
        for (i = 0; i < n_cand; ++i) {
            if (cand_tier[i] == 0) { first = i; break; }
        }

        out_x[0] = cand_x[first];
        out_y[0] = cand_y[first];
        count    = 1;

        for (i = 0; i < n_cand; ++i) {
            const int dx = cand_x[i] - out_x[0];
            const int dy = cand_y[i] - out_y[0];
            min_d2[i] = dx * dx + dy * dy;
        }

        while (count < max_spots) {
            int best    = -1;
            int best_d2 = min2 - 1;

            for (tier = 0; (tier < 2) && (best < 0); ++tier) {
                for (i = 0; i < n_cand; ++i) {
                    if (cand_tier[i] > tier) continue;
                    if (min_d2[i] > best_d2) {
                        best_d2 = min_d2[i];
                        best    = i;
                    }
                }
            }

            if (best < 0) break;

            out_x[count] = cand_x[best];
            out_y[count] = cand_y[best];
            ++count;

            for (j = 0; j < n_cand; ++j) {
                const int dx = cand_x[j] - cand_x[best];
                const int dy = cand_y[j] - cand_y[best];
                const int d2 = dx * dx + dy * dy;
                if (d2 < min_d2[j]) min_d2[j] = d2;
            }
        }
    }

    return count;
}

int dr3_cash_pickup_value(int race_tier, int player_rank, int leads_points)
{
    if (leads_points) return 400; /* 0x190 */

    if (race_tier == 0) return 50;

    if (player_rank < 1)  player_rank = 1;
    if (player_rank > 20) player_rank = 20;

    if (race_tier == 1) {
        if (player_rank <= 5)  return 260;
        if (player_rank <= 10) return 200;
        if (player_rank <= 15) return 120;
        return 60;
    }

    if (race_tier == 2) {
        if (player_rank <= 5)  return 500;
        if (player_rank <= 10) return 300;
        if (player_rank <= 15) return 150;
        return 80;
    }

    return 0;
}

int dr3_cash_payout(int pickups, int unit_value)
{
    if ((pickups <= 0) || (unit_value <= 0)) return 0;

    return pickups * unit_value;
}

int dr3_cash_player_keeps(int chaos_active, int lapped, int finished_last)
{
    if (chaos_active) return 1;

    return (!lapped && !finished_last) ? 1 : 0;
}

#if defined(DR3_CHAOS)

#include "drally.h"
#include "drally_structs_free.h"
#include "drmath.h"
#include "sfx.h"
#include "dr3_chaos.h"
#include "dr3_boss.h"
#include "dr3_log.h"

extern __POINTER__ TRX_IMA;
extern __POINTER__ TRX_MAS;
extern __POINTER__ TRX_LR1;
extern __POINTER__ RACE_OBSTACLE_BPK;
extern int         TRX_WIDTH;
extern int         TRX_HEIGHT;
extern int         TRX_VIEWPORT_TL_X;
extern int         TRX_VIEWPORT_TL_Y;
extern int         CURRENT_VIEWPORT_X;
extern int         NUM_OF_CARS;
extern __BYTE__    MY_CAR_IDX[];
extern __BYTE__    ___1e6ed0h[];
extern __BYTE__    ___1de580h[];
extern __BYTE__    ___1de7d0h[];
extern __BYTE__    ___1f2488h[];
extern __BYTE__    ___243334h[];

int  rand_watcom106(void);
void dRally_Sound_pushEffect(__BYTE__ channel, __BYTE__ n, __DWORD__ unk, __DWORD__ a0, __DWORD__ a1, __DWORD__ a2);
char *itoa_watcom106(int value, char *buffer, int radix);
void ___424c8h_v2(const char *, int x, int y);

typedef struct {
    int           x;
    int           y;
    int           active;     /* 1 = $ is sitting on TRX_IMA, 0 = waiting to respawn */
    int           cooldown;   /* ticks remaining until respawn                       */
    int           popup_life; /* >0 = floating $ popup remaining life                */
    unsigned char bg[256];    /* saved 16x16 TRX_IMA pixels underneath the $ sprite  */
} dr3_cash_slot_t;

static dr3_cash_slot_t dr3_cash_slots[DR3_CASH_MAX];
static int             dr3_cash_n;

static void dr3_cash_blit(dr3_cash_slot_t *sl)
{
    unsigned char       *ima = (unsigned char *)TRX_IMA;
    const unsigned char *spr = (const unsigned char *)RACE_OBSTACLE_BPK + 0x200; /* kind 3 ($) */
    int                  j, i;

    if (!ima || !RACE_OBSTACLE_BPK) return;

    for (j = 0; j < 16; ++j) {
        unsigned char *row = ima + (sl->y + j - 8) * TRX_WIDTH + (sl->x - 8);
        memcpy(sl->bg + j * 16, row, 16);
        for (i = 0; i < 16; ++i) {
            const unsigned char px = spr[j * 16 + i];
            if (px != 0) row[i] = px;
        }
    }

    sl->active = 1;
}

static void dr3_cash_restore(dr3_cash_slot_t *sl)
{
    unsigned char *ima = (unsigned char *)TRX_IMA;
    int            j;

    if (!ima) return;

    for (j = 0; j < 16; ++j) {
        memcpy(ima + (sl->y + j - 8) * TRX_WIDTH + (sl->x - 8), sl->bg + j * 16, 16);
    }

    sl->active = 0;
}

void dr3_cash_reset(void)
{
    dr3_cash_n = 0;
    memset(dr3_cash_slots, 0, sizeof(dr3_cash_slots));
}

void dr3_cash_setup(void)
{
    const struct_35e_t *s_35e = (const struct_35e_t *)(const void *)___1e6ed0h;
    const struct_54_t  *s_54  = (const struct_54_t *)(const void *)___1de7d0h;
    int                 avoid_x[DR3_MAX_CARS], avoid_y[DR3_MAX_CARS], n_avoid = 0;
    int                 pad_x[16], pad_y[16], n_pad = 0;
    int                 out_x[DR3_CASH_MAX], out_y[DR3_CASH_MAX];
    int                 n;

    dr3_cash_reset();

    /* Only in 30th Anniversary normal street races (never classic difficulties, never DM/Arena, never Boss) */
    if (!dr3_chaos_active() || dr3_chaos_battle()) return;
    if (dr3_boss_in_race()) return;
    if ((int)s_54[0].__50 <= 0) return;
    if (!TRX_IMA || !TRX_MAS || !TRX_LR1 || !RACE_OBSTACLE_BPK) return;

    for (n = 0; (n < NUM_OF_CARS) && (n < DR3_MAX_CARS); ++n) {
        avoid_x[n_avoid] = (int)s_35e[n].XLocation;
        avoid_y[n_avoid] = (int)s_35e[n].YLocation;
        ++n_avoid;
    }

    for (n = 0; n < 16; ++n) {
        const int px = (int)D(___1f2488h + 0x120 * n);
        const int py = (int)D(___1f2488h + 0x120 * n + 4);
        if (px > 0) {
            pad_x[n_pad] = px;
            pad_y[n_pad] = py;
            ++n_pad;
        }
    }

    dr3_cash_n = dr3_cash_place((const unsigned char *)TRX_MAS, (const unsigned char *)TRX_LR1,
                                TRX_WIDTH, TRX_HEIGHT,
                                avoid_x, avoid_y, n_avoid,
                                pad_x, pad_y, n_pad,
                                out_x, out_y, DR3_CASH_MAX);

    for (n = 0; n < dr3_cash_n; ++n) {
        dr3_cash_slots[n].x          = out_x[n];
        dr3_cash_slots[n].y          = out_y[n];
        dr3_cash_slots[n].active     = 0;
        dr3_cash_slots[n].cooldown   = 0;
        dr3_cash_slots[n].popup_life = 0;
        dr3_cash_blit(&dr3_cash_slots[n]);
    }

    dr3_log("[dr3] cash: placed %d off-line street $ pickups ($%d each, respawn %d ticks)",
            dr3_cash_n, (int)s_54[0].__50, DR3_CASH_RESPAWN_TICKS);
}

void dr3_cash_tick(void)
{
    const struct_35e_t *s_35e = (const struct_35e_t *)(const void *)___1e6ed0h;
    const int           dt    = (int)D(___243334h);
    int                 n, c;

    if (dr3_cash_n <= 0) return;

    for (n = 0; n < dr3_cash_n; ++n) {
        dr3_cash_slot_t *sl = &dr3_cash_slots[n];

        if (sl->active) continue;

        if (sl->cooldown > dt) {
            sl->cooldown -= dt;
            continue;
        }
        sl->cooldown = 0;

        /* wait until no car sits directly over the spot before re-drawing $ */
        {
            int clear = 1;
            for (c = 0; c < NUM_OF_CARS; ++c) {
                const int dx = (int)s_35e[c].XLocation - sl->x;
                const int dy = (int)s_35e[c].YLocation - sl->y;
                if ((dx > -24) && (dx < 24) && (dy > -24) && (dy < 24)) { clear = 0; break; }
            }
            if (clear) dr3_cash_blit(sl);
        }
    }
}

void dr3_cash_collect(void)
{
    const struct_35e_t *s_35e = (const struct_35e_t *)(const void *)___1e6ed0h;
    const int           me    = (int)D(MY_CAR_IDX);
    int                 n;

    if (dr3_cash_n <= 0) return;
    if ((me < 0) || (me >= NUM_OF_CARS)) return;
    if ((int)D(___1de580h + 0x94 * me + 0x18) <= 0) return;
    if (s_35e[me].Finished != 0) return;

    for (n = 0; n < dr3_cash_n; ++n) {
        dr3_cash_slot_t *sl = &dr3_cash_slots[n];
        int              dx, dy;

        if (!sl->active) continue;

        dx = (int)s_35e[me].XLocation - sl->x;
        dy = (int)s_35e[me].YLocation - sl->y;

        if ((dRMath_abs_i(dx) < 18) && (dRMath_abs_i(dy) < 18)) {
            dr3_cash_restore(sl);
            dRally_Sound_pushEffect(4, SFX_MINE, 0, 0x9000, 0x21000, 0x8000);
            D(___1de580h + 0x94 * me + 0x50)++;
            sl->popup_life = 0x8c;
            sl->cooldown   = DR3_CASH_RESPAWN_TICKS + (rand_watcom106() % 210);
            dr3_log("[dr3] cash: player collected off-line street $ #%d at %d,%d (total cash pickups: %d)",
                    n, sl->x, sl->y, (int)D(___1de580h + 0x94 * me + 0x50));
        }
    }
}

void dr3_cash_draw_popups(void)
{
    const struct_54_t *s_54 = (const struct_54_t *)(const void *)___1de7d0h;
    const int          val  = (int)s_54[0].__50;
    const int          dt   = (int)D(___243334h);
    char               buf[12];
    int                n;

    if ((dr3_cash_n <= 0) || (val <= 0)) return;

    for (n = 0; n < dr3_cash_n; ++n) {
        dr3_cash_slot_t *sl = &dr3_cash_slots[n];
        int              x, y;

        if (sl->popup_life <= 0) continue;

        x = CURRENT_VIEWPORT_X + sl->x - TRX_VIEWPORT_TL_X;
        y = sl->y + (sl->popup_life >> 3) - TRX_VIEWPORT_TL_Y - 4;

        if ((x >= -18 + CURRENT_VIEWPORT_X) && (x < DR3_RACE_W) && (y >= 6) && (y < DR3_RACE_H)) {
            ___424c8h_v2(itoa_watcom106(val, strcpy(buf, "$") + 1, 10) - 1, x + 0x60, y - 6);
        }

        sl->popup_life -= dt;
        if (sl->popup_life < 0) sl->popup_life = 0;
    }
}

#endif /* DR3_CHAOS */
