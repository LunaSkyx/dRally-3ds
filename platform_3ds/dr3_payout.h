/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_payout.h - what a place in a chaos race pays (the "Race income" of the report after the race).
 *
 * A four car race pays its first three places only ($750/$375/$187 in the easy race) - over a field of
 * twenty that leaves every place below the podium with nothing, and in a chaos race the player's own place
 * usually is one of them.  The chaos difficulty therefore pays *every* place, on a ladder that reads from
 * the back:
 *
 *     place 1                       DR3_CHAOS_RANK_MONEY       (5000)
 *     place 2 / place 3             DR3_CHAOS_RANK_PODIUM_2/3  (4000 / 3000)   <- the jump
 *     place 4 .. cars-1             DR3_CHAOS_RANK_STEP * (cars - place)        (100 per place)
 *     place cars (and below)        0                          <- the last place pays nothing
 *
 * Twenty cars: $5000, $4000, $3000, then $1600 for 4th, $1500 for 5th, ... $200 for 18th, $100 for 19th
 * and nothing for the last one (which is what a car that was wrecked and pushed to the back takes home).
 * A wreck of the player *before* the field is decided (place 0 or a place outside the race) pays nothing
 * either, and a field too small for a podium - two cars - has no jump to make: the last place of it pays
 * nothing and the step is what the place above it is worth.
 *
 * Every number is a switch of its own, so the ladder can be tuned without touching the race code, and the
 * order of the three podium values (1st >= 2nd >= 3rd) is what keeps the ladder from crossing itself.
 *
 * Plain C without any engine header, so the ladder is unit-tested on the host (tests/test_dr3.c) - place
 * by place, field by field.
 */
#ifndef DR3_PAYOUT_H
#define DR3_PAYOUT_H

#define DR3_CHAOS_RANK_MONEY        5000    /* the winner - the same the duel's top pot pays (drally.h) */
#define DR3_CHAOS_RANK_PODIUM_2     4000    /* the jump of the second place                              */
#define DR3_CHAOS_RANK_PODIUM_3     3000    /* ... and of the third                                      */
#define DR3_CHAOS_RANK_STEP         100     /* every place above the last one is worth this much more    */

/* what the place `place` of a race of `cars` cars pays; 0 for a place the race never had */
int dr3_chaos_rank_money(int place, int cars);

/*
 * Elo points for the winner of a 1v1 duel (symmetric for player wins and AI rival wins):
 *   gain = base + step * clamp(winner_rank - loser_rank, 0, max_diff)
 * Beating an equal or lower-ranked opponent pays base (+4); upsetting a higher-ranked opponent
 * pays up to base + step * max_diff (+16).
 */
int dr3_chaos_duel_points(int winner_rank, int loser_rank, int base, int step, int max_diff);

#endif
