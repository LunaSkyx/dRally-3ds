/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
#include "dr3_anniv_prog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Mode IDs matching drally.h DR3_CHAOS_MODE_* so this file stays host-testable without drally.h */
#define ANNIV_MODE_STANDARD 0
#define ANNIV_MODE_DUEL     1
#define ANNIV_MODE_DM       2
#define ANNIV_MODE_ARENA    4

/* ============================================================ portable progression logic */

int dr3_anniv_tier(int player_car, int player_rank)
{
    if (player_car >= 4 || (player_rank >= 1 && player_rank <= 6))  return 3;
    if (player_car == 3 || (player_rank >= 1 && player_rank <= 10)) return 2;
    if (player_car == 2 || (player_rank >= 1 && player_rank <= 14)) return 1;
    return 0;
}

int dr3_anniv_field_size(int tier, int is_arena)
{
    int sz;

    if (tier <= 0)      sz = 8;
    else if (tier == 1) sz = 12;
    else if (tier == 2) sz = 16;
    else                sz = 20;

    if (is_arena && sz > 16) sz = 16;
    return sz;
}

int dr3_anniv_ai_difficulty_row(int tier)
{
    if (tier <= 0) return 1; /* I_LIVE_TO_RIDE */
    if (tier <= 2) return 2; /* PETROL_IN_MY_VEINS */
    return 3;                /* ANNIVERSARY */
}

int dr3_anniv_boss_chance(int tier)
{
    if (tier <= 0)      return 0;
    if (tier == 1)      return 10;
    if (tier == 2)      return 18;
    return 25;
}

int dr3_anniv_pick_mode(int tier, int roll100)
{
    int r = roll100 % 100;
    if (r < 0) r += 100;

    if (tier <= 0) {
        /* Early Vagabond/Dervish: favor fair Races & 1v1 Challenges while still mixing in LCS & Arena */
        if (r < 35) return ANNIV_MODE_STANDARD;
        if (r < 65) return ANNIV_MODE_DUEL;
        if (r < 85) return ANNIV_MODE_DM;
        return ANNIV_MODE_ARENA;
    }
    if (tier == 1) {
        if (r < 30) return ANNIV_MODE_STANDARD;
        if (r < 55) return ANNIV_MODE_DUEL;
        if (r < 80) return ANNIV_MODE_DM;
        return ANNIV_MODE_ARENA;
    }

    /* Tier 2 & 3: full equal 25% mix across all 4 modes */
    if (r < 25) return ANNIV_MODE_STANDARD;
    if (r < 50) return ANNIV_MODE_DUEL;
    if (r < 75) return ANNIV_MODE_DM;
    return ANNIV_MODE_ARENA;
}

static int dr3_mode_bag[4];
static int dr3_mode_bag_pos  = 4;
static int dr3_mode_bag_last = -1;

void dr3_anniv_mode_bag_reset(int last_mode)
{
    dr3_mode_bag_pos  = 4;
    dr3_mode_bag_last = last_mode;
}

int dr3_anniv_mode_bag_next(int (*rng_fn)(void))
{
    int picked;

    if (dr3_mode_bag_pos >= 4) {
        static const int all_modes[4] = {
            ANNIV_MODE_STANDARD,
            ANNIV_MODE_DUEL,
            ANNIV_MODE_DM,
            ANNIV_MODE_ARENA
        };
        int i;

        for (i = 0; i < 4; ++i) dr3_mode_bag[i] = all_modes[i];

        /* Fisher-Yates shuffle */
        for (i = 3; i > 0; --i) {
            int r = rng_fn ? rng_fn() : i;
            int j, tmp;
            if (r < 0) r = -r;
            j = r % (i + 1);
            tmp = dr3_mode_bag[i];
            dr3_mode_bag[i] = dr3_mode_bag[j];
            dr3_mode_bag[j] = tmp;
        }

        /* Never repeat the last played mode across a bag refill boundary */
        if (dr3_mode_bag[0] == dr3_mode_bag_last) {
            int r = rng_fn ? rng_fn() : 1;
            int sw, tmp;
            if (r < 0) r = -r;
            sw = 1 + (r % 3);
            tmp = dr3_mode_bag[0];
            dr3_mode_bag[0] = dr3_mode_bag[sw];
            dr3_mode_bag[sw] = tmp;
        }

        dr3_mode_bag_pos = 0;
    }

    picked = dr3_mode_bag[dr3_mode_bag_pos++];
    dr3_mode_bag_last = picked;
    return picked;
}

int dr3_anniv_underdog_bonus(int player_car, int victim_car)
{
    if (player_car < 0) player_car = 0;
    if (victim_car > player_car) {
        return 150 * (victim_car - player_car);
    }
    return 0;
}

int dr3_anniv_select_field(int player_seat, int target_cars,
                           const int *racer_cars, const int *racer_ranks,
                           int num_racers, int *out_seats)
{
    int cand[32];
    int score[32];
    int nc = 0, i, j, out_n = 0;
    int pcar = 0, prank = 20;

    if (!racer_cars || !racer_ranks || !out_seats || num_racers <= 0 || target_cars <= 0)
        return 0;
    if (num_racers > 32) num_racers = 32;
    if (target_cars > num_racers) target_cars = num_racers;

    if (player_seat >= 0 && player_seat < num_racers) {
        pcar  = racer_cars[player_seat];
        prank = racer_ranks[player_seat];
        out_seats[out_n++] = player_seat;
    }

    for (i = 0; i < num_racers; ++i) {
        int dc, dr;
        if (i == player_seat) continue;
        dc = racer_cars[i] - pcar;
        if (dc < 0) dc = -dc;
        dr = racer_ranks[i] - prank;
        if (dr < 0) dr = -dr;
        cand[nc]  = i;
        /* Match speed/car class primarily, breaking ties by closest standings rank */
        score[nc] = dc * 20 + dr;
        ++nc;
    }

    /* Stable insertion sort by score ascending (closest peers first) */
    for (i = 1; i < nc; ++i) {
        int kc = cand[i];
        int ks = score[i];
        for (j = i - 1; j >= 0 && score[j] > ks; --j) {
            cand[j + 1]  = cand[j];
            score[j + 1] = score[j];
        }
        cand[j + 1]  = kc;
        score[j + 1] = ks;
    }

    for (i = 0; i < nc && out_n < target_cars; ++i) {
        out_seats[out_n++] = cand[i];
    }

    return out_n;
}

/* ============================================================ portable radio state machine */

static char dr3_rad_speaker[32];
static char dr3_rad_line[64];
static int  dr3_rad_style;
static int  dr3_rad_start_tick = -10000;
static int  dr3_rad_end_tick   = -10000;
static int  dr3_rad_last_push  = -10000;

static void dr3_rad_copy_upper(char *dst, size_t cap, const char *src)
{
    size_t i = 0;

    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }

    while (src[i] && i + 1 < cap) {
        char c = src[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        dst[i] = c;
        ++i;
    }
    dst[i] = '\0';
}

void dr3_anniv_radio_clear(void)
{
    dr3_rad_speaker[0] = '\0';
    dr3_rad_line[0]    = '\0';
    dr3_rad_style      = DR3_ANNIV_RADIO_PIT;
    dr3_rad_start_tick = -10000;
    dr3_rad_end_tick   = -10000;
    dr3_rad_last_push  = -10000;
}

int dr3_anniv_radio_push(const char *speaker, const char *line,
                         int style, int priority, int now_tick)
{
    int cd;

    if (!speaker || !line || !line[0]) return 0;

    cd = priority ? DR3_ANNIV_RADIO_CD_URGENT : DR3_ANNIV_RADIO_CD_NORMAL;
    if (now_tick < dr3_rad_last_push + cd) return 0;
    if (!priority && now_tick < dr3_rad_end_tick) return 0;

    dr3_rad_copy_upper(dr3_rad_speaker, sizeof(dr3_rad_speaker), speaker);
    dr3_rad_copy_upper(dr3_rad_line, sizeof(dr3_rad_line), line);
    dr3_rad_style      = style;
    dr3_rad_start_tick = now_tick;
    dr3_rad_end_tick   = now_tick + DR3_ANNIV_RADIO_HOLD_TICKS;
    dr3_rad_last_push  = now_tick;
    return 1;
}

int dr3_anniv_radio_query(int now_tick, const char **speaker,
                          const char **line, int *style)
{
    if (!dr3_rad_line[0]) return 0;
    if (now_tick < dr3_rad_start_tick || now_tick >= dr3_rad_end_tick) return 0;

    if (speaker) *speaker = dr3_rad_speaker;
    if (line)    *line    = dr3_rad_line;
    if (style)   *style   = dr3_rad_style;
    return 1;
}

/* ============================================================ engine runtime glue */
#if defined(DR3_CHAOS)

#include "drally.h"
#include "drally_structs_free.h"
#include "dr3_chaos.h"
#include "dr3_boss.h"
#include "dr3_arena.h"
#include "dr3_log.h"

extern unsigned char ___1de580h[];   /* s_94: HP at +0x18 */
extern unsigned char ___1e6ed0h[];   /* s_35e: Lap, Position */
extern unsigned char ___1de7d0h[];   /* s_54: car name (+0x00) */
extern int           NUM_OF_CARS;
extern int           NUM_OF_LAPS;
extern __DWORD__     MY_CAR_IDX;

static int  dr3_active_field_n;

/* Per-race dialogue flags */
static int  dr3_r_tier;
static int  dr3_r_field_n;
static int  dr3_r_mode;
static int  dr3_r_has_boss;
static char dr3_r_rival[24];
static int  dr3_r_seed;
static int  dr3_r_intro_done;
static int  dr3_r_lead_done;
static int  dr3_r_lastlap_done;
static int  dr3_r_lowhp_done;
static int  dr3_r_surv3_done;
static int  dr3_r_surv2_done;
static int  dr3_r_boss_half_done;
static int  dr3_r_boss_enrage_done;
static int  dr3_r_was_p1;
static int  dr3_r_kills;

void dr3_anniv_set_active_field(int count)
{
    dr3_active_field_n = count;
}

int dr3_anniv_active_field(void)
{
    return dr3_active_field_n;
}

void dr3_anniv_radio_race_init(int tier, int field_n, int mode, int has_boss,
                               const char *rival_name)
{
    dr3_anniv_radio_clear();
    dr3_r_tier             = tier;
    dr3_r_field_n          = field_n;
    dr3_r_mode             = mode;
    dr3_r_has_boss         = has_boss;
    dr3_r_seed             = (dr3_r_seed + 1) & 0xff;
    dr3_r_intro_done       = 0;
    dr3_r_lead_done        = 0;
    dr3_r_lastlap_done     = 0;
    dr3_r_lowhp_done       = 0;
    dr3_r_surv3_done       = 0;
    dr3_r_surv2_done       = 0;
    dr3_r_boss_half_done   = 0;
    dr3_r_boss_enrage_done = 0;
    dr3_r_was_p1           = 0;
    dr3_r_kills            = 0;

    if (rival_name && rival_name[0]) {
        dr3_rad_copy_upper(dr3_r_rival, sizeof(dr3_r_rival), rival_name);
    } else {
        strcpy(dr3_r_rival, "RIVAL");
    }
}

static int dr3_rad_car_hp(int car)
{
    return (int)(*(const uint32_t *)(const void *)(___1de580h + 0x94 * car + 0x18));
}

static const char *dr3_rad_car_name(int car)
{
    if (car < 0 || car >= NUM_OF_CARS) return "RIVAL";
    return (const char *)(___1de7d0h + 0x54 * car);
}

void dr3_anniv_radio_on_kill(const char *victim_name, int victim_model,
                             int player_model, int total_bounty, int underdog_bonus)
{
    const int ticks = (int)getCounter(1);
    char      spk[32];
    char      msg[64];

    if (!dr3_chaos_active() || dr3_chaos_race_over()) return;

    ++dr3_r_kills;
    (void)player_model;

    if (victim_model == DR3_ADVERSARY_CAR) {
        dr3_anniv_radio_push("ADVERSARY:", "IMPOSSIBLE... THIS ISN'T OVER!",
                             DR3_ANNIV_RADIO_ADVERSARY, 1, ticks);
        return;
    }

    if (underdog_bonus > 0) {
        if ((dr3_r_kills & 1) && victim_name && victim_name[0]) {
            snprintf(spk, sizeof(spk), "%.22s:", victim_name);
            dr3_anniv_radio_push(spk, "WRECKED BY THAT JUNKHEAP?!",
                                 DR3_ANNIV_RADIO_RIVAL, 1, ticks);
        } else {
            snprintf(msg, sizeof(msg), "GIANT SLAYER! +$%d BONUS!", total_bounty);
            dr3_anniv_radio_push("PIT RADIO:", msg, DR3_ANNIV_RADIO_GOLD, 1, ticks);
        }
        return;
    }

    if (dr3_r_kills >= 3) {
        snprintf(msg, sizeof(msg), "UNSTOPPABLE! %d WRECKS!", dr3_r_kills);
        dr3_anniv_radio_push("PIT RADIO:", msg, DR3_ANNIV_RADIO_GOLD, 1, ticks);
        return;
    }

    if (dr3_r_kills == 2) {
        dr3_anniv_radio_push("PIT RADIO:", "DOUBLE WRECK! ON A RAMPAGE!",
                             DR3_ANNIV_RADIO_GOLD, 1, ticks);
        return;
    }

    {
        static const char *const lines[4] = {
            "MY CAR!! YOU'LL PAY FOR THAT!",
            "NO!! NOT MY ENGINE!",
            "YOU MANIAC!! SEE YOU NEXT RACE!",
            "MY PAINT JOB!! ARGH!!"
        };
        snprintf(spk, sizeof(spk), "%.22s:", (victim_name && victim_name[0]) ? victim_name : "RIVAL");
        dr3_anniv_radio_push(spk, lines[(dr3_r_seed + dr3_r_kills) & 3],
                             DR3_ANNIV_RADIO_RIVAL, 1, ticks);
    }
}

void dr3_anniv_radio_tick(void)
{
    const struct_35e_t *s     = (const struct_35e_t *)(const void *)___1e6ed0h;
    const int           ticks = (int)getCounter(1);
    const int           me    = (int)MY_CAR_IDX;
    char                spk[32];
    char                msg[64];

    if (!dr3_chaos_active() || dr3_chaos_race_over()) return;
    if (me < 0 || me >= NUM_OF_CARS) return;

    /* 1. Race Intro / Tier Flavor (right at green light; delayed slightly in Arena so Grudge banner shines first) */
    if (!dr3_r_intro_done && !dr3_boss_in_race()) {
        const int intro_at = (dr3_r_mode == DR3_CHAOS_MODE_ARENA) ? 430 : 195;

        if (ticks >= intro_at) {
            dr3_r_intro_done = 1;

            if (dr3_r_has_boss) {
                static const char *const boss_lines[2] = {
                    "I HEARD YOU WERE GETTING FAST...",
                    "LET'S SEE IF YOU CAN KEEP UP!"
                };
                dr3_anniv_radio_push("ADVERSARY:", boss_lines[dr3_r_seed & 1],
                                     DR3_ANNIV_RADIO_ADVERSARY, 1, ticks);
            }
            else if (dr3_r_mode == DR3_CHAOS_MODE_DUEL) {
                static const char *const duel_lines[2] = {
                    "JUST YOU AND ME. DON'T BLINK!",
                    "LET'S SETTLE THIS ONE ON ONE!"
                };
                snprintf(spk, sizeof(spk), "%.22s:", dr3_r_rival);
                dr3_anniv_radio_push(spk, duel_lines[dr3_r_seed & 1],
                                     DR3_ANNIV_RADIO_RIVAL, 0, ticks);
            }
            else if (dr3_r_mode == DR3_CHAOS_MODE_DM) {
                if (dr3_r_seed & 1) {
                    snprintf(msg, sizeof(msg), "%d CARS! LAST ONE STANDING WINS!", dr3_r_field_n);
                    dr3_anniv_radio_push("PIT RADIO:", msg, DR3_ANNIV_RADIO_PIT, 0, ticks);
                } else {
                    dr3_anniv_radio_push("PIT RADIO:", "GRAB MINES AND AMMO. WRECK 'EM!",
                                         DR3_ANNIV_RADIO_PIT, 0, ticks);
                }
            }
            else if (dr3_r_mode == DR3_CHAOS_MODE_ARENA) {
                snprintf(msg, sizeof(msg), "%d-CAR ARENA! TOP 3 GET PAID!", dr3_r_field_n);
                dr3_anniv_radio_push("PIT RADIO:", msg, DR3_ANNIV_RADIO_PIT, 0, ticks);
            }
            else {
                /* Standard Race: escalates with tier! */
                if (dr3_r_tier == 0) {
                    if ((dr3_r_seed % 3) == 0) {
                        snprintf(msg, sizeof(msg), "%d CARS ON THE GRID. STAY SHARP!", dr3_r_field_n);
                        dr3_anniv_radio_push("PIT RADIO:", msg, DR3_ANNIV_RADIO_PIT, 0, ticks);
                    } else if ((dr3_r_seed % 3) == 1) {
                        snprintf(spk, sizeof(spk), "%.22s:", dr3_r_rival);
                        dr3_anniv_radio_push(spk, "WELCOME TO THE 30TH, ROOKIE!",
                                             DR3_ANNIV_RADIO_RIVAL, 0, ticks);
                    } else {
                        snprintf(spk, sizeof(spk), "%.22s:", dr3_r_rival);
                        dr3_anniv_radio_push(spk, "EAT MY DUST, FRESH MEAT!",
                                             DR3_ANNIV_RADIO_RIVAL, 0, ticks);
                    }
                } else if (dr3_r_tier == 1) {
                    if (dr3_r_seed & 1) {
                        snprintf(msg, sizeof(msg), "%d CARS! THE BIG GUNS ARE WATCHING.", dr3_r_field_n);
                        dr3_anniv_radio_push("PIT RADIO:", msg, DR3_ANNIV_RADIO_PIT, 0, ticks);
                    } else {
                        snprintf(spk, sizeof(spk), "%.22s:", dr3_r_rival);
                        dr3_anniv_radio_push(spk, "MADE IT PAST THE SCRAP HEAP?",
                                             DR3_ANNIV_RADIO_RIVAL, 0, ticks);
                    }
                } else if (dr3_r_tier == 2) {
                    if (dr3_r_seed & 1) {
                        snprintf(msg, sizeof(msg), "%d CARS! WELCOME TO THE BIG LEAGUES.", dr3_r_field_n);
                        dr3_anniv_radio_push("PIT RADIO:", msg, DR3_ANNIV_RADIO_PIT, 0, ticks);
                    } else {
                        snprintf(spk, sizeof(spk), "%.22s:", dr3_r_rival);
                        dr3_anniv_radio_push(spk, "THINK YOU BELONG UP HERE?!",
                                             DR3_ANNIV_RADIO_RIVAL, 0, ticks);
                    }
                } else {
                    if (dr3_r_seed & 1) {
                        dr3_anniv_radio_push("PIT RADIO:", "ALL 20 CARS! TOTAL 30TH MAYHEM!",
                                             DR3_ANNIV_RADIO_GOLD, 0, ticks);
                    } else {
                        snprintf(spk, sizeof(spk), "%.22s:", dr3_r_rival);
                        dr3_anniv_radio_push(spk, "EVERYBODY'S HERE. NO MERCY!",
                                             DR3_ANNIV_RADIO_RIVAL, 0, ticks);
                    }
                }
            }
        }
    }

    /* 2. Boss Gauntlet mid-fight taunts */
    if (dr3_boss_is_swarm() && !dr3_r_boss_half_done && ticks > 400) {
        int alive_enemies = 0, i;
        for (i = 0; i < NUM_OF_CARS; ++i)
            if (i != me && dr3_rad_car_hp(i) > 0) ++alive_enemies;
        if (alive_enemies <= 10 && alive_enemies > 0) {
            dr3_r_boss_half_done = 1;
            dr3_anniv_radio_push("ADVERSARY:", "STILL STANDING? SEND THE REST!",
                                 DR3_ANNIV_RADIO_ADVERSARY, 1, ticks);
        }
    }
    if (dr3_boss_is_final_duel() && !dr3_r_boss_enrage_done && dr3_boss_enraged()) {
        dr3_r_boss_enrage_done = 1;
        dr3_anniv_radio_push("ADVERSARY:", "ENOUGH GAMES! BURN!!",
                             DR3_ANNIV_RADIO_ADVERSARY, 1, ticks);
    }

    /* 3. Taking P1 mid-race (Standard & Duel) */
    if (!dr3_chaos_battle() && !dr3_r_lead_done && ticks > 420 && dr3_rad_car_hp(me) > 0) {
        int pos = (int)s[me].Position;
        if (pos == 1 && !dr3_r_was_p1) {
            dr3_r_lead_done = 1;
            if (dr3_r_seed & 1) {
                dr3_anniv_radio_push("PIT RADIO:", "YOU'RE IN P1! HOLD THE LINE!",
                                     DR3_ANNIV_RADIO_GOLD, 0, ticks);
            } else {
                int p2 = -1, i;
                for (i = 0; i < NUM_OF_CARS; ++i)
                    if (i != me && (int)s[i].Position == 2) { p2 = i; break; }
                snprintf(spk, sizeof(spk), "%.22s:", (p2 >= 0) ? dr3_rad_car_name(p2) : dr3_r_rival);
                dr3_anniv_radio_push(spk, "WAIT TIL I CATCH YOU!!",
                                     DR3_ANNIV_RADIO_RIVAL, 0, ticks);
            }
        }
        dr3_r_was_p1 = (pos == 1);
    }

    /* 4. Final Lap callout (Standard & Duel) */
    if (!dr3_chaos_battle() && !dr3_r_lastlap_done && NUM_OF_LAPS > 1 &&
        (int)s[me].Lap >= NUM_OF_LAPS && s[me].Finished == 0 && dr3_rad_car_hp(me) > 0) {
        int pos = (int)s[me].Position;
        dr3_r_lastlap_done = 1;
        if (pos == 1) {
            dr3_anniv_radio_push("PIT RADIO:", "FINAL LAP! BRING IT HOME!",
                                 DR3_ANNIV_RADIO_GOLD, 1, ticks);
        } else if (pos <= 3) {
            dr3_anniv_radio_push("PIT RADIO:", "FINAL LAP! HIT THE TURBO!",
                                 DR3_ANNIV_RADIO_GOLD, 1, ticks);
        } else {
            dr3_anniv_radio_push("PIT RADIO:", "FINAL LAP! PUSH TO THE FINISH!",
                                 DR3_ANNIV_RADIO_PIT, 1, ticks);
        }
    }

    /* 5. Battle mode (LCS / Arena) survivor tension */
    if (dr3_chaos_battle() && !dr3_boss_in_race() && ticks > 420 && dr3_rad_car_hp(me) > 0) {
        int alive = 0, i;
        for (i = 0; i < NUM_OF_CARS; ++i)
            if (dr3_rad_car_hp(i) > 0) ++alive;

        if (alive == 2 && !dr3_r_surv2_done) {
            dr3_r_surv2_done = 1;
            dr3_r_surv3_done = 1;
            dr3_anniv_radio_push("PIT RADIO:", "FINAL SHOWDOWN! 1V1 TO THE DEATH!",
                                 DR3_ANNIV_RADIO_GOLD, 1, ticks);
        } else if (alive == 3 && !dr3_r_surv3_done && dr3_r_field_n >= 6) {
            dr3_r_surv3_done = 1;
            dr3_anniv_radio_push("PIT RADIO:", "ONLY 3 CARS LEFT! STAY ALIVE!",
                                 DR3_ANNIV_RADIO_GOLD, 0, ticks);
        }
    }

    /* 6. Low HP Warning (once per race below 28% HP) */
    if (!dr3_r_lowhp_done && ticks > 350) {
        int hp = dr3_rad_car_hp(me);
        if (hp > 0 && hp < 28672) { /* < 28% of 102400 */
            dr3_r_lowhp_done = 1;
            dr3_anniv_radio_push("PIT RADIO:",
                                 dr3_chaos_battle() ? "HULL CRITICAL! FIND A WRENCH!"
                                                    : "CRITICAL DAMAGE! WATCH YOUR BUMPER!",
                                 DR3_ANNIV_RADIO_RIVAL, 0, ticks);
        }
    }
}

int dr3_anniv_radio_now(const char **speaker, const char **line, int *style)
{
    if (!dr3_chaos_active() || dr3_chaos_race_over()) return 0;
    return dr3_anniv_radio_query((int)getCounter(1), speaker, line, style);
}

#endif /* DR3_CHAOS */
