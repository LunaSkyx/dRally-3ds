/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_dm.h - the endgame of "Destroy 'em all" (DR3_CHAOS_MODE_DM): a deadline, the sudden death and the
 * banner that announces it.
 *
 * Why it is needed: a deathmatch ends when one car is left (race_main.c) and the engine only counts a car
 * as out when it is wrecked *and* standing still (`s_35e[n].__b0 <= 0.5f`, race_main.c:817) - and the only
 * other way out of the mode is the wreck of the player.  In a field of twenty, one stuck or unreachable
 * car therefore keeps the race running forever, and a race the player *survives* can hang.  (Counting the
 * standing cars as out would be the patch for that one hole; a deadlined field is what the mode reads
 * like: the organiser turns the heat up.)
 *
 * The rule (drally.h has the entry in its chaos section):
 *
 *   - DR3_DM_SUDDEN_AT_S seconds into the race the sudden death starts.  It is measured with the race's
 *     own clock (getCounter(1) - 70 ticks a second, started with the race in race___49a34h.c), so it is
 *     game time: a slow frame does not stretch the race and a harness run is frame exact.
 *   - Every car that still has HP loses DR3_DM_SUDDEN_PERCENT_S percent of it every second, in the very
 *     unit the game's own DAMAGE panel counts in (race___400ech.c: damage = 100 - HP/1024).  One percent
 *     is DR3_DM_HP_PER_PERCENT = 1024 HP for every car, whatever its own scale is - the same drain for
 *     everybody, the player included.
 *   - Nobody survives it: after 100 seconds of sudden death every car is a wreck and the race is decided.
 *     The port does not wait for the engine's own count to notice that - as soon as one car is left it
 *     starts the engine's wind down (the very counter the player's own wreck uses, race_main.c:1074), so
 *     the race cannot hang on a wreck that keeps rolling either.
 *   - The banner (DR3_DM_BANNER_LINE1/LINE2, "SUDDEN" and "DEATH" in two lines - see the constants below)
 *     says what is going on.  It is drawn by the port - the same pass
 *     that colours the cars (dr3_blit.c) - with the game's own font (___1d7810h: 6x6 glyphs, characters
 *     from 0x20) scanned out at DR3_DM_BANNER_SCALE, outlined: 3 means 18 px tall letters, which is what
 *     reads as a banner.  It holds DR3_DM_BANNER_HOLD_MS and then blinks every DR3_DM_BANNER_BLINK_MS.
 *
 * Everything below the tuning is plain integer arithmetic and is host tested (tests/test_dr3.c): this
 * file is compiled by the test project too - the engine glue at the bottom only in the chaos build.
 */
#ifndef DR3_DM_H
#define DR3_DM_H

#define DR3_DM_SUDDEN_AT_S       180   /* the sudden death starts three minutes into the race        */
#define DR3_DM_SUDDEN_PERCENT_S  1     /* ... and takes this much of every car's HP every second     */
#define DR3_DM_HP_PER_PERCENT    1024  /* the engine's own HP per percent (race___400ech.c)          */
/*
 * The banner.  Two lines and not one: at DR3_DM_BANNER_SCALE the 12 letters of "SUDDEN DEATH" written in
 * one line are 249 pixels wide, which would reach under the two HUD columns of the race screen (the
 * viewport is 64..256 of the 320) - stacked they are 123 and 102 pixels and stay inside it, and two fat
 * lines read like a banner anyway.
 */
#define DR3_DM_BANNER_LINE1      "SUDDEN"
#define DR3_DM_BANNER_LINE2      "DEATH"
#define DR3_DM_BANNER_SCALE      3     /* the game's 6x6 glyphs, 3x = 18 px tall letters             */
#define DR3_DM_BANNER_HOLD_MS    6000  /* the banner stands this long ...                            */
#define DR3_DM_BANNER_BLINK_MS   900   /* ... and then blinks at this rate                           */
#define DR3_DM_TICKS_PER_S       70    /* the race clock: getCounter(1) counts the game's own frames */

/*
 * The lap duty of a deathmatch - the answer to camping.  The sudden death alone makes "stand in a corner
 * and wait" the winning move: every car loses the same HP per second, so whoever takes no damage simply
 * outlives the field and takes the pot (the pot goes to the survivor, ___33010h.c).  So a car also has to
 * keep *racing*: every DR3_DM_LAP_INTERVAL_S seconds of race time it has to have finished
 * DR3_DM_LAP_PER_INTERVAL lap(s), and if it has not, it loses DR3_DM_LAP_PENALTY_PERCENT percent of its HP
 * at that check (25 percent - four missed checks and the car is a wreck).  Driving a lap takes ~25..40 s on
 * the tracks, so one lap a minute is not a burden for anybody who races; standing still costs 25 percent a
 * minute on top of the sudden death.
 *
 * The laps are counted by the port itself, not read from `s_35e[n].Lap`: the HUD clamps that field to
 * DR3_CHAOS_HUD_LAP_MAX (9) in a deathmatch (race___55ae0h.c), so it stops moving after nine laps.  What
 * is counted instead is the engine's own lap crossing: the checkpoint counter `s_35e[n].__0` goes back to 0
 * exactly when `race___55ae0h.c` counts a lap.
 *
 * The switch is sdmc:/chaos_dm_lap.txt (the seconds between two checks, 0 switches the duty off; the
 * harness reaches it with "dmlap <n>").
 */
#define DR3_DM_LAP_PER_INTERVAL    1     /* laps a car has to finish every ...                         */
#define DR3_DM_LAP_INTERVAL_S      60    /* ... this many seconds of race time                         */
#define DR3_DM_LAP_PENALTY_PERCENT 25    /* ... or it loses this much of its HP at the check           */

/* Street Last Car Standing (DR3_CHAOS_MODE_DM) combat AI & loot tuning */
#define DR3_DM_ENGAGE_RANGE        230   /* px: street combat target engagement radius                 */
#define DR3_DM_GUN_RANGE           130   /* px: full machine gun hitscan reach (26 * 5 px)             */
#define DR3_DM_MINE_COOLDOWN       95    /* frames (~1.35s) between AI mine drops on the streets       */
#define DR3_DM_START_MINES         3     /* minimum mines every car brings to a street deathmatch      */
#define DR3_DM_HP_MAX              102400
#define DR3_DM_AMMO_MAX            102400

/* the glyphs of the game's font (___1d7810h: 0x900 bytes, 0x24 each, characters from 0x20) */
#define DR3_DM_GLYPHS            64
#define DR3_DM_GLYPH_BYTES       0x24
#define DR3_DM_GLYPH_W           6
#define DR3_DM_GLYPH_H           6
/* one blank column between two glyphs, so a text of n letters is (7n - 1) glyph pixels wide */
#define DR3_DM_GLYPH_ADVANCE     (DR3_DM_GLYPH_W + 1)

/* the race clock (getCounter(1) ticks) in the unit the tuning above is written in */
unsigned long long dr3_dm_ticks_ms(int ticks);
int                dr3_dm_ticks_s(int ticks);

/*
 * The sudden death, all of it a function of the race age in ms and of the deadline `at_s` (the switch
 * below, normally DR3_DM_SUDDEN_AT_S):
 *
 *   percent()  how much of its HP a car has lost by then (0 while the deadline has not come)
 *   active()   whether the deadline has come at all - the banner hangs on this, not on percent()
 *   due()      what is still owed when `drained` percent have been taken already (never negative, so a
 *              race that starts after the deadline catches up instead of standing still)
 *   hp()       the HP that many percent are worth
 */
int dr3_dm_sudden_percent(unsigned long long ms, int at_s);
int dr3_dm_sudden_active(unsigned long long ms, int at_s);
int dr3_dm_sudden_due(unsigned long long ms, int at_s, int drained);
int dr3_dm_hp_for_percent(int percent);

/* whether the banner is up: it holds from the start of the sudden death, then it blinks */
int dr3_dm_banner_visible(unsigned long long ms, int at_s);

/*
 * The lap duty.  checks() is how many interval checks a race of `ms` has behind it (0 while the switch is
 * off or the first interval is not over), missed() says whether a car that had `laps_at_check` laps at the
 * last check and `laps` now owes a penalty.  Both are plain arithmetic and host tested.
 */
int dr3_dm_lap_checks(unsigned long long ms, int interval_s);
int dr3_dm_lap_duty_missed(int laps, int laps_at_check);

/* the glyph of a character of the game's font, -1 when the font has no glyph for it (a space, say) */
int dr3_dm_glyph_index(char c);
/* how wide a text is at that scale, in the game's own pixels */
int dr3_dm_text_width(const char *text, int scale);

/*
 * Street Last Car Standing (DR3_CHAOS_MODE_DM) loot & combat decisions (portable, host tested):
 *   item_for_roll()     0..99 -> pickup kind (ammo, mine crate, money, big money, turbo, repairs)
 *   target_score()      higher = better prey to hunt/line up/ram (0 = ignore)
 *   pickup_want_range() max detour distance in px for an AI driver to veer onto a street drop
 */
int dr3_dm_item_for_roll(int roll);
int dr3_dm_target_score(float dist, int target_hp, int hp_max, float angle_abs_deg);
int dr3_dm_pickup_want_range(int kind, int ammo, int hp, int hp_max, int mines, int turbo);

#if defined(DR3_CHAOS)
/*
 * The engine side.  reset() is called when a race starts (race___49a34h.c), setup() arms the grid for a
 * street deathmatch (race___42824h.c), ai_augment() layers goal-aware combat onto race___4b62ch(), and
 * tick() runs once per frame of a deathmatch/arena.
 */
void dr3_dm_reset(void);
void dr3_dm_setup(void);
void dr3_dm_ai_augment(int car);
void dr3_dm_tick(void);
unsigned long long dr3_dm_banner_ms(void);

/*
 * The switch of the sudden death: sdmc:/chaos_dm.txt holds one number - the seconds until it starts, 0
 * switches it off entirely (then a deathmatch runs exactly as it did before).  Read with the other
 * switches at every race setup; the PC harness goes through dr3_dm_sudden_set(), so a run can be cut down
 * to seconds for a test.
 */
int dr3_dm_sudden_s(void);
void dr3_dm_sudden_set(int seconds);
void dr3_dm_sudden_reload(void);

/*
 * The switch of the lap duty (dr3_dm.h): sdmc:/chaos_dm_lap.txt holds the seconds between two checks, 0
 * switches the duty off.  Same shape as the sudden death switch - read at every race setup, and the harness
 * goes through dr3_dm_lap_set() ("dmlap <n>").
 */
int  dr3_dm_lap_s(void);
void dr3_dm_lap_set(int seconds);
void dr3_dm_lap_reload(void);

/* how many laps the port counted for a car of the running race (the HUD's own counter is clamped) */
int  dr3_dm_laps(int car);
#endif

#endif /* DR3_DM_H */
