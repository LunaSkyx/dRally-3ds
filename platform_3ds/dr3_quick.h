/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_quick.h - the direct race start of scripted runs (dr3_scenario.h, "quickrace").
 *
 * Skips the load menu, the shop, the market and the sign up screen: the field is filled the way the sign
 * up screen fills it and PREPARE TO RACE + race_main run at once.  The mode of the race and the number of
 * extra cars are the usual switches (sdmc:/chaos_mode.txt, sdmc:/chaos_extras.txt - az_run.ps1 -Mode/-Extras).
 *
 * This is the 3DS twin of dr3_host_quick_race() in dr3_host.c (the PC harness); keep the two in step.
 */
#ifndef DR3_QUICK_H
#define DR3_QUICK_H

/* 1 once the front end has loaded its assets AND a racer (the player) exists - the race can start then */
int  dr3_quick_ready(void);

/*
 * Sets the next race up and runs it (returns when race_main returns).
 *   slot   which of the three races of the event (0..2)
 *   track  track byte 0..18 for that race, or -1 to keep what the event setup picked
 */
void dr3_quick_race(int slot, int track);

/* the career counts as raced (racer_t.races = 1): the wheel of the sign up screen only spins after a first race (dr3_chaos_raced) */
void dr3_quick_set_raced(void);

/* sets the HP of the player's car in the running race (scenario step "playerhp N": a car that outlives the field, to test the payout) */
void dr3_quick_set_player_hp(int hp);

/* sets the drug (mushroom warp) counter of the player's car in the running race (scenario step "playerdrug N") */
void dr3_quick_set_player_drug(int drug);

/* sets the player's championship rank (1..20) before quickrace so grid placement can be tested at any position (scenario step "playerrank N") */
void dr3_quick_set_player_rank(int rank);

/* sets the HP of Boss car 0 in the running race (scenario step "bosshp N": test Phase 3 enrage & boss defeat) */
void dr3_quick_set_boss_hp(int hp);

/* launches the 30th Anniversary Final Boss Gauntlet at phase 1..3 via ___31008h() (scenario step "quickboss P") */
void dr3_quick_boss(int phase);

#endif /* DR3_QUICK_H */
