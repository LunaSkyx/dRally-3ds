/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_boss.h - 30th Anniversary 3-Phase Final Boss Gauntlet.
 *
 * When the player reaches #1 in the 30th Anniversary championship and enters the Final Challenge:
 *   Phase 1 - THE SWARM:      Updated Battle Arena (TRA), 1 vs 20 Vagabonds all hunting the player.
 *   Phase 2 - THE ESCORT:     Classic Arena Oval (TR0), 5-lap high-speed race vs 11 maxed elite rivals.
 *   Phase 3 - THE ADVERSARY:  Updated Battle Arena (TRA), 1 vs 1 against a 1.5x Giant Boss Deliverator
 *                             with 3x HP, dedicated Boss HP bar, and an Enraged sub-phase below 50% HP.
 *
 * Between phases, the player receives a mandatory free full repair + bonus cash and enters a Shop/Underground
 * pit stop (where mines/upgrades can be bought and the game can be saved).  Losing any phase resets the
 * gauntlet back to Phase 1.
 */
#ifndef DR3_BOSS_H
#define DR3_BOSS_H

#define DR3_BOSS_PHASE_NONE      0
#define DR3_BOSS_PHASE_1         1
#define DR3_BOSS_PHASE_2         2
#define DR3_BOSS_PHASE_3         3

/* Phase 1: 1 player + 20 Vagabonds = 21 cars total */
#define DR3_BOSS_P1_ENEMIES         20
#define DR3_BOSS_P1_CARS            21
#define DR3_BOSS_P1_ENEMY_HP        44000   /* ~43% of full Arena HP so 1v20 is intense & doable */
#define DR3_BOSS_P1_ENEMY_HP_EXT    62000   /* EXTREME: ~60% HP Vagabonds */
#define DR3_BOSS_P1_BONUS           5000

/* Phase 2: 9-lap high-speed race on TR0 vs 11 elite rivals = 12 cars */
#define DR3_BOSS_P2_CARS            12
#define DR3_BOSS_P2_LAPS            9
#define DR3_BOSS_P2_BONUS           8000

/* Phase 3: 1 vs 1 Giant Boss Deliverator (1.5x visual & hitbox scale, 3x HP / 4x EXTREME) */
#define DR3_BOSS_P3_CARS            2
#define DR3_BOSS_P3_HP_MAX          307200  /* 3x standard Arena HP (102400) */
#define DR3_BOSS_P3_HP_MAX_EXT      409600  /* EXTREME: 4x standard Arena HP */
#define DR3_BOSS_P3_SPEED_MULT      1.14f
#define DR3_BOSS_P3_ENRAGE_MULT     1.22f
#define DR3_BOSS_P3_ENRAGE_MULT_EXT 1.28f
#define DR3_BOSS_P3_STEER_MULT      1.10f
#define DR3_BOSS_P3_START_MINES  8
#define DR3_BOSS_P3_RING_START_R 620
#define DR3_BOSS_P3_RING_END_R   185     /* keeps the 4 inner ammo/repair/mine pads inside the ring */
#define DR3_BOSS_P3_RING_SHRINK_S 65

#define DR3_BOSS_SAVE_TAG0       0x44    /* 'D' */
#define DR3_BOSS_SAVE_TAG1       0x42    /* 'B' */

#ifdef __cplusplus
extern "C" {
#endif

/* ---- portable pure logic (host unit tested in tests/test_dr3.c) ---- */

const char *dr3_boss_phase_title(int phase);
const char *dr3_boss_phase_subtitle(int phase);
int         dr3_boss_phase_bonus(int phase);
int         dr3_boss_next_phase(int current, int won);

/* Phase 1 spawn: idx==0 is the player (center-south), idx 1..20 are the 20 Vagabonds around the arena */
void        dr3_boss_swarm_spawn(int idx, int w, int h, int *x, int *y, int *img);

/* Phase 3 spawn: dramatic north-vs-south face-off across the central hub */
void        dr3_boss_phase3_spawn(int is_boss, int w, int h, int *x, int *y, int *img);

/* Phase 3 enrage & closing ring math */
int         dr3_boss_p3_enraged(int hp_now, int hp_max);
int         dr3_boss_p3_ring_radius(int enrage_ticks);

/* Savegame encoding inside racer_t.unk0[12] (backwards-compatible with old saves) */
void        dr3_boss_encode_save(unsigned char unk0[12], int phase);
int         dr3_boss_decode_save(const unsigned char unk0[12]);

#if defined(DR3_CHAOS)
/* ---- engine state & runtime helpers ---- */

int   dr3_boss_phase(void);
void  dr3_boss_set_phase(int phase);
void  dr3_boss_reset(void);

int   dr3_boss_in_race(void);
void  dr3_boss_set_in_race(int on);

int   dr3_boss_is_arena_phase(void);     /* Phase 1 or Phase 3 currently racing on TRA */
int   dr3_boss_is_swarm(void);           /* Phase 1 currently racing */
int   dr3_boss_is_escort_race(void);     /* Phase 2 currently racing on TR0 */
int   dr3_boss_is_final_duel(void);      /* Phase 3 currently racing on TRA */
int   dr3_boss_is_giant_car(int car);    /* 1.5x scaled Boss Deliverator (car 0 in Phase 3) */

void  dr3_boss_race_reset(void);         /* called at race start to clear enrage state */
void  dr3_boss_tick(void);               /* called once per frame during Boss Phase 3 */
int   dr3_boss_enraged(void);
int   dr3_boss_enrage_ticks(void);
int   dr3_boss_defeated(void);           /* 1 during Phase 3 Giant Boss chain-reaction death */
int   dr3_boss_death_flash(void);        /* 0..255 shockwave flash alpha during boss death   */
int   dr3_boss_p3_ring_active(void);
int   dr3_boss_p3_ring_r(void);

/* Banner for Boss Phase intro (first 4s) and Phase 3 Enrage (4s): returns 0=none, 1=yellow, 2=red */
int   dr3_boss_banner_now(const char **line1, const char **line2);

/* Boss HP bar info for Phase 3 (returns 1 when bar should be drawn) */
int   dr3_boss_hud_bar(int *hp_now, int *hp_max, int *enraged);

/* Persist/restore boss gauntlet phase into/from s_6c[player].unk0 */
void  dr3_boss_save_sync(void);
void  dr3_boss_load_sync(void);

/* Active Boss HP scaling (Normal vs EXTREME) */
int   dr3_boss_p1_enemy_hp(void);
int   dr3_boss_p3_hp_max(void);

/* 1 when loaded from a dedicated cheat test save (unk0[3] == 'C'): God Mode + Infinite Ammo/Turbo/Mines + 5x Gun */
int   dr3_boss_cheat_active(void);
#else
static __inline int dr3_boss_p1_enemy_hp(void) { return DR3_BOSS_P1_ENEMY_HP; }
static __inline int dr3_boss_p3_hp_max(void) { return DR3_BOSS_P3_HP_MAX; }
static __inline int dr3_boss_phase(void) { return 0; }
static __inline int dr3_boss_in_race(void) { return 0; }
static __inline int dr3_boss_is_arena_phase(void) { return 0; }
static __inline int dr3_boss_is_swarm(void) { return 0; }
static __inline int dr3_boss_is_escort_race(void) { return 0; }
static __inline int dr3_boss_is_final_duel(void) { return 0; }
static __inline int dr3_boss_is_giant_car(int car) { (void)car; return 0; }
static __inline int dr3_boss_enraged(void) { return 0; }
static __inline int dr3_boss_defeated(void) { return 0; }
static __inline int dr3_boss_death_flash(void) { return 0; }
static __inline int dr3_boss_cheat_active(void) { return 0; }
static __inline void dr3_boss_reset(void) {}
static __inline void dr3_boss_save_sync(void) {}
static __inline void dr3_boss_load_sync(void) {}
#endif

#ifdef __cplusplus
}
#endif

#endif /* DR3_BOSS_H */
