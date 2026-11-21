/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_cash.h - Off-line street cash ($) pickups for normal street races.
 *
 * In standard street races, track files only define 16 pickup pads (max 4 active at once) and most sit
 * on the AI racing line, making it tough for a starter Vagabond to earn upgrade cash.  This module places
 * up to DR3_CASH_MAX small cash ($) drops on drivable shoulders, wide corner outsides, and road edges
 * AWAY from the AI's center line (TRX_LR1), reserved for the player to hunt down and respawning once per
 * lap.
 */
#ifndef DR3_CASH_H
#define DR3_CASH_H

#define DR3_CASH_MAX           10    /* extra off-line street $ pickups placed around a circuit */
#define DR3_CASH_MIN_SEP       118   /* minimum px between two off-line cash spots              */
#define DR3_CASH_PAD_CLEAR     36    /* minimum px from any standard track powerup pad          */
#define DR3_CASH_CAR_CLEAR     60    /* minimum px from any car on the starting grid            */
#define DR3_CASH_RESPAWN_TICKS 1050  /* ~15 seconds (70 ticks/s) before a collected $ respawns  */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Distance from the AI's center racing line (TRX_LR1: 1..15 is on the road corridor, 8 = center,
 * AI hugs 7..9). Returns -1 for off-track pixels (lr1 == 0 or >= 16) so decorative areas behind walls
 * are rejected.
 */
int dr3_cash_lr1_dev(unsigned char lr1);

/* Whether (x, y) has a clean 21x21 drivable clearance ((mask & 0xf) >= 4) away from track walls. */
int dr3_cash_spot_ok(const unsigned char *mask, int w, int h, int x, int y);

/*
 * Places up to `max_spots` off-line cash positions on a street track (w x h, quarter-res lr1).
 * Prefers Tier 1 spots well off the AI line (dev >= 4), then Tier 2 edges (dev >= 2), while keeping
 * clear of walls, existing track pads (`pad_x/y`), and starting cars (`avoid_x/y`).
 * Returns the number of placed spots (0..max_spots).
 */
int dr3_cash_place(const unsigned char *mask, const unsigned char *lr1, int w, int h,
                   const int *avoid_x, const int *avoid_y, int n_avoid,
                   const int *pad_x, const int *pad_y, int n_pad,
                   int *out_x, int *out_y, int max_spots);

/*
 * Per-pickup dollar value for street cash ($) pickups (s_54[0].__50), matching Death Rally's
 * tier & championship-rank table:
 *   - leads_points != 0           -> $400 (0x190)
 *   - race_tier == 0 (Easy)       -> $50
 *   - race_tier == 1 (Medium)     -> rank 1..5: $260, 6..10: $200, 11..15: $120, 16..20: $60
 *   - race_tier == 2 (Hard)       -> rank 1..5: $500, 6..10: $300, 11..15: $150, 16..20: $80
 *   - race_tier == 3 (Final/Glory)-> $0 (unless leads_points != 0)
 */
int dr3_cash_pickup_value(int race_tier, int player_rank, int leads_points);

/* Total dollars earned from `pickups` collected $ units at `unit_value` each (clamped >= 0). */
int dr3_cash_payout(int pickups, int unit_value);

/*
 * Whether the player keeps their collected street cash ($) pickups after a race:
 *   - 30th Anniversary (chaos_active != 0): ALWAYS kept (even if lapped, wrecked, or last place).
 *   - Classic difficulties (chaos_active == 0): original Death Rally rule (lost if lapped or last).
 */
int dr3_cash_player_keeps(int chaos_active, int lapped, int finished_last);

#if defined(DR3_CHAOS)
void dr3_cash_reset(void);
void dr3_cash_setup(void);
void dr3_cash_tick(void);
void dr3_cash_collect(void);
void dr3_cash_draw_popups(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DR3_CASH_H */
