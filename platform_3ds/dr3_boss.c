/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/* dr3_boss.c - 30th Anniversary 3-Phase Final Boss Gauntlet (see dr3_boss.h). */
#include "dr3_boss.h"
#include "dr3_arena.h"

#include <stddef.h>

/* ------------------------------------------------------------------ portable logic --- */

const char *dr3_boss_phase_title(int phase)
{
    switch (phase) {
    case DR3_BOSS_PHASE_1: return "PHASE 1: THE SWARM";
    case DR3_BOSS_PHASE_2: return "PHASE 2: THE ESCORT";
    case DR3_BOSS_PHASE_3: return "FINAL PHASE: THE ADVERSARY";
    default:               return "";
    }
}

const char *dr3_boss_phase_subtitle(int phase)
{
    switch (phase) {
    case DR3_BOSS_PHASE_1: return "1 VS 20 VAGABONDS - BATTLE ARENA";
    case DR3_BOSS_PHASE_2: return "9-LAP ELITE RACE - THE ARENA";
    case DR3_BOSS_PHASE_3: return "1 VS 1 GIANT BOSS - BATTLE ARENA";
    default:               return "";
    }
}

int dr3_boss_phase_bonus(int phase)
{
    if (phase == DR3_BOSS_PHASE_1) return DR3_BOSS_P1_BONUS;
    if (phase == DR3_BOSS_PHASE_2) return DR3_BOSS_P2_BONUS;
    return 0;
}

int dr3_boss_next_phase(int current, int won)
{
    if (!won) return DR3_BOSS_PHASE_NONE;
    if (current == DR3_BOSS_PHASE_1) return DR3_BOSS_PHASE_2;
    if (current == DR3_BOSS_PHASE_2) return DR3_BOSS_PHASE_3;
    return DR3_BOSS_PHASE_NONE;
}

void dr3_boss_swarm_spawn(int idx, int w, int h, int *x, int *y, int *img)
{
    int px, py, i;

    if (idx <= 0) {
        /* Player starts in the open south-center lane facing up (away from immediate corner fire) */
        px = w / 2;
        py = h - 148;
        if (x) *x = px;
        if (y) *y = py;
        if (img) *img = 0;   /* 0 deg = straight up */
        return;
    }

    if (idx <= 16) {
        /* First 16 Vagabonds fill all 4x4 corner spawn bays */
        dr3_arena_spawn(idx - 1, w, h, x, y, img);
        return;
    }

    /* Vagabonds 17..20 enter from the four outer wall midpoints, all facing the center */
    switch ((idx - 17) & 3) {
    case 0:  px = w / 2;  py = 82;      break;  /* North wall */
    case 1:  px = w / 2;  py = h - 82;  break;  /* South wall */
    case 2:  px = 96;     py = h / 2;   break;  /* West wall  */
    default: px = w - 96; py = h / 2;   break;  /* East wall  */
    }

    if (x) *x = px;
    if (y) *y = py;
    if (img) {
        i = (int)(dr3_arena_bearing((float)(w / 2 - px), (float)(h / 2 - py)) / 3.75f + 0.5f);
        *img = ((i % 96) + 96) % 96;
    }
}

void dr3_boss_phase3_spawn(int is_boss, int w, int h, int *x, int *y, int *img)
{
    if (is_boss) {
        /* Giant Adversary faces south across the central arena hub */
        if (x)   *x   = w / 2;
        if (y)   *y   = 132;
        if (img) *img = 48;  /* 180 deg = facing down */
    }
    else {
        /* Player faces north into the showdown */
        if (x)   *x   = w / 2;
        if (y)   *y   = h - 132;
        if (img) *img = 0;   /* 0 deg = facing up */
    }
}

int dr3_boss_p3_enraged(int hp_now, int hp_max)
{
    if ((hp_now <= 0) || (hp_max <= 0)) return 0;
    return (hp_now * 2 <= hp_max);
}

int dr3_boss_p3_ring_radius(int enrage_ticks)
{
    const int shrink = DR3_BOSS_P3_RING_SHRINK_S * 70;

    if (enrage_ticks <= 0) return DR3_BOSS_P3_RING_START_R;
    if (enrage_ticks >= shrink) return DR3_BOSS_P3_RING_END_R;

    return DR3_BOSS_P3_RING_START_R
         - (DR3_BOSS_P3_RING_START_R - DR3_BOSS_P3_RING_END_R) * enrage_ticks / shrink;
}

void dr3_boss_encode_save(unsigned char unk0[12], int phase)
{
    if (!unk0) return;

    if ((phase >= DR3_BOSS_PHASE_1) && (phase <= DR3_BOSS_PHASE_3)) {
        unk0[0] = (unsigned char)DR3_BOSS_SAVE_TAG0;
        unk0[1] = (unsigned char)DR3_BOSS_SAVE_TAG1;
        unk0[2] = (unsigned char)phase;
    }
    else {
        unk0[0] = 0;
        unk0[1] = 0;
        unk0[2] = 0;
    }
}

int dr3_boss_decode_save(const unsigned char unk0[12])
{
    if (!unk0) return DR3_BOSS_PHASE_NONE;
    if ((unk0[0] == (unsigned char)DR3_BOSS_SAVE_TAG0) &&
        (unk0[1] == (unsigned char)DR3_BOSS_SAVE_TAG1) &&
        (unk0[2] >= (unsigned char)DR3_BOSS_PHASE_1) &&
        (unk0[2] <= (unsigned char)DR3_BOSS_PHASE_3)) {
        return (int)unk0[2];
    }
    return DR3_BOSS_PHASE_NONE;
}

/* ================================================================== engine glue === */
#if defined(DR3_CHAOS)

#include "drally.h"
#include "drally_structs_fixed.h"
#include "drally_structs_free.h"
#include "sfx.h"
#include "dr3_chaos.h"
#include "dr3_log.h"

extern __BYTE__ ___1a01e0h[];
extern __BYTE__ ___1a1ef8h[];
extern __BYTE__ ___1de580h[];
extern __BYTE__ ___1e6ed0h[];
extern __BYTE__ ___1df520h[];
extern __DWORD__ MY_CAR_IDX;
extern int      NUM_OF_CARS;

void dRally_Sound_pushEffect(__BYTE__ channel, __BYTE__ n, __DWORD__ unk, __DWORD__ a0, __DWORD__ a1, __DWORD__ a2);
__DWORD__ __GET_FRAME_COUNTER_NOW(void);

static int dr3_boss_cur_phase;      /* DR3_BOSS_PHASE_NONE..3 (persists across Pit Stop) */
static int dr3_boss_racing;         /* 1 while a Boss Gauntlet race/arena is executing   */
static int dr3_boss_p3_enrage_on;   /* 1 once Giant Adversary drops <= 50% HP            */
static int dr3_boss_p3_enrage_tick; /* getCounter(1) tick when enrage triggered          */
static int dr3_boss_p3_base_spd_set;
static float dr3_boss_p3_base_spd;
static int dr3_boss_p3_dead;        /* 1 once Giant Adversary is destroyed (finale!)     */
static int dr3_boss_p3_dead_tick;   /* getCounter(1) tick when Giant Adversary died      */
static int dr3_boss_p3_next_boom;   /* next dt to trigger a chain-reaction explosion     */
static int dr3_boss_p3_boom_idx;    /* explosion pattern counter                         */
static int dr3_boss_p3_hail_done;   /* 1 once SFX_HAIL_TO_THE_KING has been played       */
static int dr3_boss_cheat_on;       /* 1 when loaded from a dedicated cheat test save    */

int dr3_boss_p1_enemy_hp(void)
{
    return dr3_extreme_active() ? DR3_BOSS_P1_ENEMY_HP_EXT : DR3_BOSS_P1_ENEMY_HP;
}

int dr3_boss_p3_hp_max(void)
{
    return dr3_extreme_active() ? DR3_BOSS_P3_HP_MAX_EXT : DR3_BOSS_P3_HP_MAX;
}

int  dr3_boss_cheat_active(void) { return dr3_boss_cheat_on; }

int  dr3_boss_phase(void) { return dr3_boss_cur_phase; }
void dr3_boss_set_phase(int phase)
{
    if ((phase < DR3_BOSS_PHASE_NONE) || (phase > DR3_BOSS_PHASE_3)) phase = DR3_BOSS_PHASE_NONE;
    dr3_boss_cur_phase = phase;
}

void dr3_boss_reset(void)
{
    dr3_boss_cur_phase       = DR3_BOSS_PHASE_NONE;
    dr3_boss_racing          = 0;
    dr3_boss_p3_enrage_on    = 0;
    dr3_boss_p3_enrage_tick  = 0;
    dr3_boss_p3_base_spd_set = 0;
    dr3_boss_p3_dead         = 0;
    dr3_boss_p3_dead_tick    = 0;
    dr3_boss_p3_next_boom    = 0;
    dr3_boss_p3_boom_idx     = 0;
    dr3_boss_p3_hail_done    = 0;
    dr3_boss_cheat_on        = 0;
}

int  dr3_boss_in_race(void) { return dr3_boss_racing && (dr3_boss_cur_phase != DR3_BOSS_PHASE_NONE); }
void dr3_boss_set_in_race(int on) { dr3_boss_racing = on ? 1 : 0; }

int  dr3_boss_is_arena_phase(void)
{
    return dr3_boss_in_race() &&
           ((dr3_boss_cur_phase == DR3_BOSS_PHASE_1) || (dr3_boss_cur_phase == DR3_BOSS_PHASE_3));
}

int  dr3_boss_is_swarm(void)       { return dr3_boss_in_race() && (dr3_boss_cur_phase == DR3_BOSS_PHASE_1); }
int  dr3_boss_is_escort_race(void) { return dr3_boss_in_race() && (dr3_boss_cur_phase == DR3_BOSS_PHASE_2); }
int  dr3_boss_is_final_duel(void)  { return dr3_boss_in_race() && (dr3_boss_cur_phase == DR3_BOSS_PHASE_3); }
int  dr3_boss_is_giant_car(int car) { return dr3_boss_is_final_duel() && (car == 0); }

void dr3_boss_race_reset(void)
{
    dr3_boss_p3_enrage_on    = 0;
    dr3_boss_p3_enrage_tick  = 0;
    dr3_boss_p3_base_spd_set = 0;
    dr3_boss_p3_base_spd     = 0.0f;
    dr3_boss_p3_dead         = 0;
    dr3_boss_p3_dead_tick    = 0;
    dr3_boss_p3_next_boom    = 0;
    dr3_boss_p3_boom_idx     = 0;
    dr3_boss_p3_hail_done    = 0;
}

int dr3_boss_enraged(void)  { return dr3_boss_is_final_duel() && dr3_boss_p3_enrage_on && !dr3_boss_p3_dead; }
int dr3_boss_defeated(void) { return dr3_boss_is_final_duel() && dr3_boss_p3_dead; }

int dr3_boss_death_flash(void)
{
    int dt;

    if (!dr3_boss_defeated()) return 0;
    dt = (int)getCounter(1) - dr3_boss_p3_dead_tick;
    if (dt < 0) return 0;

    /* Initial kill shockwave flash (ticks 0..20) */
    if (dt < 20) return 200 - (dt * 200) / 20;

    /* Catastrophic core detonation flash (ticks 100..132) */
    if ((dt >= 100) && (dt < 132)) return 235 - ((dt - 100) * 235) / 32;

    return 0;
}

static void dr3_boss_spawn_explosion(int x, int y)
{
    const int idx = (int)getCounter(6);

    if ((idx < 0) || (idx >= DR3_MAX_MINES - 2)) return;
    D(___1df520h + 0x10 * idx + 0x0) = x;
    D(___1df520h + 0x10 * idx + 0x4) = y;
    D(___1df520h + 0x10 * idx + 0x8) = 0;
    D(___1df520h + 0x10 * idx + 0xc) = __GET_FRAME_COUNTER_NOW();
    incCounter(6);
}

int dr3_boss_enrage_ticks(void)
{
    if (!dr3_boss_enraged()) return 0;
    {
        const int dt = (int)getCounter(1) - dr3_boss_p3_enrage_tick;
        return (dt > 0) ? dt : 0;
    }
}

int dr3_boss_p3_ring_active(void) { return dr3_boss_enraged(); }
int dr3_boss_p3_ring_r(void)      { return dr3_boss_p3_ring_radius(dr3_boss_enrage_ticks()); }

void dr3_boss_tick(void)
{
    int hp_now;

    if (!dr3_boss_is_final_duel() || (NUM_OF_CARS < 1)) return;

    if (!dr3_boss_p3_base_spd_set) {
        dr3_boss_p3_base_spd     = *(const float *)(const void *)(___1de580h + 4);
        dr3_boss_p3_base_spd_set = 1;
    }

    hp_now = (int)(*(const int32_t *)(const void *)(___1de580h + 0x18));

    /* Chain-reaction Giant Boss detonation finale when Adversary is destroyed while player lives */
    if ((hp_now <= 0) && (MY_CAR_IDX < (unsigned)NUM_OF_CARS)) {
        const int player_hp = (int)(*(const int32_t *)(const void *)(___1de580h + 0x94 * MY_CAR_IDX + 0x18));

        if (!dr3_boss_p3_dead && (player_hp > 0)) {
            dr3_boss_p3_dead      = 1;
            dr3_boss_p3_dead_tick = (int)getCounter(1);
            dr3_boss_p3_next_boom = 0;
            dr3_boss_p3_boom_idx  = 0;
            dr3_boss_p3_hail_done = 0;
            dr3_log("[dr3] boss: PHASE 3 GIANT ADVERSARY DESTROYED! Chain-reaction finale started!");
        }
    }

    if (dr3_boss_p3_dead) {
        const struct_35e_t *s35 = (const struct_35e_t *)___1e6ed0h;
        const int           bx  = (int)s35[0].XLocation;
        const int           by  = (int)s35[0].YLocation;
        const int           dt  = (int)getCounter(1) - dr3_boss_p3_dead_tick;
        static const signed char ox[15] = { 0, -22, 20, -14, 24, 0, -26, 18, -18, 22, -10, 12, -24, 24, 0 };
        static const signed char oy[15] = { 0, -18, 16, 22, -16, -24, 6, -22, 14, 20, -20, 18, -8, 10, 0 };

        if ((dt >= dr3_boss_p3_next_boom) && (dr3_boss_p3_boom_idx < 15)) {
            dr3_boss_spawn_explosion(bx + ox[dr3_boss_p3_boom_idx], by + oy[dr3_boss_p3_boom_idx]);
            if (dr3_boss_p3_boom_idx == 14) {
                /* Catastrophic final triple-core blast! */
                dr3_boss_spawn_explosion(bx - 16, by - 14);
                dr3_boss_spawn_explosion(bx + 16, by + 14);
                dRally_Sound_pushEffect(0xb, SFX_MINE_EXPLOSION, 0, 0x10000, 0x1e000, 0x8000);
            }
            else {
                dRally_Sound_pushEffect((dr3_boss_p3_boom_idx & 1) ? 0xb : 0xc,
                                        (dr3_boss_p3_boom_idx & 1) ? SFX_EXPLOSION : SFX_MINE_EXPLOSION,
                                        0, 0xe000, 0x20000 + (dr3_boss_p3_boom_idx % 4) * 0x1000, 0x8000);
            }
            ++dr3_boss_p3_boom_idx;
            dr3_boss_p3_next_boom = (dr3_boss_p3_boom_idx == 14) ? 100 : (dt + 7);
        }

        if (!dr3_boss_p3_hail_done && (dt >= 102)) {
            dr3_boss_p3_hail_done = 1;
            dRally_Sound_pushEffect(2, SFX_HAIL_TO_THE_KING, 0, 0x10000, 0x28000, 0x8000);
        }
        return;
    }

    if (!dr3_boss_p3_enrage_on && dr3_boss_p3_enraged(hp_now, dr3_boss_p3_hp_max())) {
        const float enrage_m = dr3_extreme_active() ? DR3_BOSS_P3_ENRAGE_MULT_EXT : DR3_BOSS_P3_ENRAGE_MULT;
        const float boost    = enrage_m / DR3_BOSS_P3_SPEED_MULT;

        dr3_boss_p3_enrage_on   = 1;
        dr3_boss_p3_enrage_tick = (int)getCounter(1);

        *(float *)(void *)(___1de580h + 4) = dr3_boss_p3_base_spd * boost;
        *(float *)(void *)(___1de580h + 8) = dr3_boss_p3_base_spd * boost;

        /* Top up the Boss's ammo and grant 4 extra mines on enrage (6 on EXTREME) */
        *(int32_t *)(void *)(___1de580h + 0x30) = DR3_ARENA_AMMO_MAX;
        *(int32_t *)(void *)(___1de580h + 0x28) += dr3_extreme_active() ? 6 : 4;

        dr3_log("[dr3] boss: PHASE 3 ADVERSARY ENRAGED at tick %d (hp %d/%d) - speed boosted & closing ring started!",
                dr3_boss_p3_enrage_tick, hp_now, dr3_boss_p3_hp_max());
    }
}

int dr3_boss_banner_now(const char **line1, const char **line2)
{
    const int ticks = (int)getCounter(1);

    if (!dr3_boss_in_race()) return 0;

    /* Giant Boss defeated victory celebration banner */
    if (dr3_boss_defeated()) {
        const int dt = (int)getCounter(1) - dr3_boss_p3_dead_tick;
        if ((dt >= 6) && (dt < 96)) {
            *line1 = "ADVERSARY";
            *line2 = "DESTROYED!";
            return 1;
        }
        if (dt >= 96) {
            *line1 = "HAIL TO";
            *line2 = "THE KING!";
            return 1;
        }
        return 0;
    }

    /* Enrage alert takes priority for 4 seconds */
    if (dr3_boss_enraged()) {
        const int dt = dr3_boss_enrage_ticks();

        if (dt < 4 * 70) {
            if (((dt / 14) & 1) == 0) {
                *line1 = "ADVERSARY";
                *line2 = "ENRAGED!";
                return 2;
            }
            return 0;
        }
    }

    /* Phase opening banner during the first 4 seconds */
    if ((ticks >= 10) && (ticks < 290)) {
        switch (dr3_boss_cur_phase) {
        case DR3_BOSS_PHASE_1:
            *line1 = "PHASE 1";
            *line2 = "THE SWARM";
            return 1;
        case DR3_BOSS_PHASE_2:
            *line1 = "PHASE 2";
            *line2 = "THE ESCORT";
            return 1;
        case DR3_BOSS_PHASE_3:
            *line1 = "FINAL PHASE";
            *line2 = "ADVERSARY";
            return 2;
        default:
            break;
        }
    }

    return 0;
}

int dr3_boss_hud_bar(int *hp_now, int *hp_max, int *enraged)
{
    int hp;

    if (!dr3_boss_is_final_duel() || dr3_chaos_race_over()) return 0;

    hp = (int)(*(const int32_t *)(const void *)(___1de580h + 0x18));
    if (hp < 0) hp = 0;

    if (hp_now)  *hp_now  = hp;
    if (hp_max)  *hp_max  = dr3_boss_p3_hp_max();
    if (enraged) *enraged = dr3_boss_p3_enrage_on;
    return 1;
}

void dr3_boss_save_sync(void)
{
    racer_t  *s_6c = (racer_t *)___1a01e0h;
    const int me   = (int)D(___1a1ef8h);

    if ((me < 0) || (me >= 20)) return;
    dr3_boss_encode_save((unsigned char *)s_6c[me].unk0, dr3_chaos_active() ? dr3_boss_cur_phase : DR3_BOSS_PHASE_NONE);
    s_6c[me].unk0[3] = dr3_boss_cheat_on ? 'C' : 0;
}

void dr3_boss_load_sync(void)
{
    const racer_t *s_6c = (const racer_t *)___1a01e0h;
    const int      me   = (int)D(___1a1ef8h);

    dr3_boss_racing = 0;
    dr3_boss_race_reset();
    if (!dr3_chaos_active() || (me < 0) || (me >= 20)) {
        dr3_boss_cur_phase = DR3_BOSS_PHASE_NONE;
        dr3_boss_cheat_on  = 0;
        return;
    }

    dr3_boss_cur_phase = dr3_boss_decode_save((const unsigned char *)s_6c[me].unk0);
    dr3_boss_cheat_on  = (s_6c[me].unk0[3] == 'C') ? 1 : 0;
    if (dr3_boss_cur_phase != DR3_BOSS_PHASE_NONE) {
        dr3_log("[dr3] boss: restored gauntlet phase %d (%s) from save",
                dr3_boss_cur_phase, dr3_boss_phase_title(dr3_boss_cur_phase));
    }
    if (dr3_boss_cheat_on) {
        dr3_log("[dr3] boss: CHEAT TEST SAVE active (God Mode + Infinite Ammo/Turbo/Mines + 6x Gun)");
    }
}

#endif /* DR3_CHAOS */
