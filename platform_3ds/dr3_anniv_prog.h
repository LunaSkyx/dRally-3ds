/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#ifndef DR3_ANNIV_PROG_H
#define DR3_ANNIV_PROG_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 30th Anniversary "Rising Chaos" progression & live in-race radio dialogue.
 *
 * Keeps early Vagabond/Dervish races tight, fair, and speed-matched (8 cars -> 12 -> 16 -> 20
 * across Standard, Destroy 'Em All, and Arena), scales AI pace and Adversary invasion chance
 * as the player climbs the championship, awards Underdog Bounties for wrecking higher-class
 * cars, and drives contextual in-race radio & rival trash-talk callouts.
 */

#define DR3_ANNIV_RADIO_PIT        0   /* cyan:   Pit Radio / Race Control   */
#define DR3_ANNIV_RADIO_RIVAL      1   /* amber:  Rival trash-talk           */
#define DR3_ANNIV_RADIO_GOLD       2   /* gold:   Underdog / rampage / hype  */
#define DR3_ANNIV_RADIO_ADVERSARY  3   /* purple: Black Anniversary boss     */

#define DR3_ANNIV_RADIO_HOLD_TICKS 245 /* ~3.5 s display time at 70 Hz       */
#define DR3_ANNIV_RADIO_CD_NORMAL  490 /* ~7.0 s cooldown for normal chatter */
#define DR3_ANNIV_RADIO_CD_URGENT  140 /* ~2.0 s cooldown for urgent events  */

/* ---- Portable pure progression logic (host unit tested) ---- */

/* Progression tier 0..3 from player's car model (0..6) and championship rank (1..20). */
int dr3_anniv_tier(int player_car, int player_rank);

/* Target grid size for the tier: Normal 8 -> 12 -> 16 -> 20 (16 Arena); Extreme = 20 (16 Arena) always. */
int dr3_anniv_field_size(int tier, int is_arena);
int dr3_anniv_field_size_ex(int tier, int is_arena, int extreme);

/* AI speed/difficulty table row (race___3f970h.c): Normal 1 -> 2 -> 3; Extreme = 3 (Anniversary) always. */
int dr3_anniv_ai_difficulty_row(int tier);
int dr3_anniv_ai_difficulty_row_ex(int tier, int extreme);

/* Default random Adversary invasion percent chance: Normal 0/10/18/25%; Extreme 45/45/60/60%. */
int dr3_anniv_boss_chance(int tier);
int dr3_anniv_boss_chance_ex(int tier, int extreme);

/* 1v1 Challenge duel lap count: Normal 3/3/4/5; Extreme 4/5/5/6. */
int dr3_anniv_duel_laps(int tier);
int dr3_anniv_duel_laps_ex(int tier, int extreme);

/* Tier-weighted wheel mode pick (returns DR3_CHAOS_MODE_* 0..3/4). roll100 in 0..99. */
int dr3_anniv_pick_mode(int tier, int roll100);

/*
 * 4-mode shuffle-bag selector: guarantees all 4 modes (Race, Challenge 1v1, Destroy 'Em All, Arena)
 * appear once every 4 spins and never repeats the same mode back-to-back across bag boundaries.
 * Pass last_mode = -1 for no initial exclusion (or DR3_CHAOS_MODE_STANDARD after Race 1).
 */
void dr3_anniv_mode_bag_reset(int last_mode);
int  dr3_anniv_mode_bag_next(int (*rng_fn)(void));

/* Extra cash bonus for wrecking a strictly higher-class car (Normal $150/gap, Extreme $200/gap). */
int dr3_anniv_underdog_bonus(int player_car, int victim_car);
int dr3_anniv_underdog_bonus_ex(int player_car, int victim_car, int extreme);

/*
 * Select target_cars seats from the 20-driver roster: out_seats[0] = player_seat, followed by
 * the closest speed/class-matched opponents sorted by |car_diff|*20 + |rank_diff|.
 * Returns number of seats written (1..target_cars).
 */
int dr3_anniv_select_field(int player_seat, int target_cars,
                           const int *racer_cars, const int *racer_ranks,
                           int num_racers, int *out_seats);

/* ---- Portable radio state machine (host unit tested) ---- */

void dr3_anniv_radio_clear(void);
int  dr3_anniv_radio_push(const char *speaker, const char *line,
                          int style, int priority, int now_tick);
int  dr3_anniv_radio_query(int now_tick, const char **speaker,
                           const char **line, int *style);

#if defined(DR3_CHAOS)
/* ---- Engine runtime hooks (30th Anniversary only) ---- */

void dr3_anniv_set_active_field(int count);
int  dr3_anniv_active_field(void);

void dr3_anniv_radio_race_init(int tier, int field_n, int mode, int has_boss,
                               const char *rival_name);
void dr3_anniv_radio_tick(void);
void dr3_anniv_radio_on_kill(const char *victim_name, int victim_model,
                             int player_model, int total_bounty, int underdog_bonus);
int  dr3_anniv_radio_now(const char **speaker, const char **line, int *style);
#else
static __inline void dr3_anniv_set_active_field(int count) { (void)count; }
static __inline int  dr3_anniv_active_field(void) { return 0; }
static __inline void dr3_anniv_radio_race_init(int tier, int field_n, int mode, int has_boss,
                                               const char *rival_name)
{ (void)tier; (void)field_n; (void)mode; (void)has_boss; (void)rival_name; }
static __inline void dr3_anniv_radio_tick(void) {}
static __inline void dr3_anniv_radio_on_kill(const char *victim_name, int victim_model,
                                             int player_model, int total_bounty, int underdog_bonus)
{ (void)victim_name; (void)victim_model; (void)player_model; (void)total_bounty; (void)underdog_bonus; }
static __inline int  dr3_anniv_radio_now(const char **speaker, const char **line, int *style)
{ (void)speaker; (void)line; (void)style; return 0; }
#endif

#ifdef __cplusplus
}
#endif

#endif /* DR3_ANNIV_PROG_H */
