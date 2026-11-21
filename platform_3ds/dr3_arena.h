/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_arena.h - the chaos mode ARENA (DR3_CHAOS_MODE_ARENA, drally.h): an epic free for all.
 *
 * Not a race at all.  Every car starts at the same moment from its own place in one of the four corners of
 * a walled arena (TRA.BPA, built by scripts/arena/make_arena.py - doc/arena.md has the format and the
 * layout), the ground is littered with ammunition and other items, and the AI of the cars does not follow a
 * road: it HUNTS.  It picks an opponent, drives at him, shoots when he is in front of the gun, drops mines
 * on whoever chases it, goes for ammunition when it runs dry and for a repair when it is hurt.  The last
 * car standing wins; the top three survivors are paid (DR3_ARENA_PRIZE_*) and everybody scores the points
 * of the place he finished in - the order is who outlived whom.
 *
 * Everything the rules need to *decide* is plain arithmetic in this header's functions (host tested in
 * tests/test_dr3.c): where a car stands, which way it has to turn, which way is free, who is placed where,
 * what a place pays, which item the dice give.  The engine side (the AI itself, the pickups, the ranking at
 * the end) sits behind DR3_CHAOS at the bottom of dr3_arena.c.
 *
 * The geometry of the engine, as found in race___4c434h.c / race___4b62ch.c:
 *   - Direction is in degrees; a car moves along (-sin D, -cos D) (x right, y down) - D = 0 is "up".
 *   - STEER_LEFT adds __a8 degrees a frame to Direction, STEER_RIGHT takes them away.
 *   - the surface mask: the low nibble of a pixel, 0..3 is solid (a wall), everything else can be driven on.
 */
#ifndef DR3_ARENA_H
#define DR3_ARENA_H

#define DR3_ARENA_TRACK         "TRA"   /* the track the mode runs on (TRA.BPA, scripts/arena/make_arena.py)      */
#define DR3_ARENA_MUSIC         "TR6"   /* the music of an existing track: TRA has none of its own                */
#define DR3_ARENA_CARS          12      /* the field: three cars to a corner - the arena is 960x600              */
#define DR3_ARENA_CARS_MAX      16      /* four to a corner is what the bays hold                                 */
#define DR3_ARENA_CORNERS       4

/* the spawn bays: the anchor of the first slot in a corner, the distance between two slots (make_arena.py) */
#define DR3_ARENA_BAY_ANCHOR    88
#define DR3_ARENA_BAY_STEP      56

/*
 * The prize money of the three best survivors.  The winner takes more than the pot of a deathmatch
 * ($5000, DR3_CHAOS_DM_POT) - an arena is a harder fight with a bigger field than a pot for the last car.
 */
#define DR3_ARENA_PRIZE_1       6000
#define DR3_ARENA_PRIZE_2       3500
#define DR3_ARENA_PRIZE_3       1500

/* the items the ground gives (race___526ach.c): kinds 1..8 are the engine's own, 10 is our custom Mine Crate */
#define DR3_ITEM_AMMO           1       /* +30 % of the gun's ammunition                                       */
#define DR3_ITEM_TURBO          2       /* +15 % of the turbo fuel                                             */
#define DR3_ITEM_MONEY          3       /* cash pickup (+$s_54[0].__50)                                        */
#define DR3_ITEM_MINE           3       /* (legacy alias for DR3_ITEM_MONEY)                                   */
#define DR3_ITEM_REPAIR_SMALL   5       /* +2..5 % HP                                                          */
#define DR3_ITEM_MONEY_BIG      7       /* 10x big cash pickup (+$10 * s_54[0].__50)                           */
#define DR3_ITEM_MINES_10       7       /* (legacy alias for DR3_ITEM_MONEY_BIG)                               */
#define DR3_ITEM_REPAIR_BIG     8       /* +20 % HP                                                            */
#define DR3_ITEM_MINE_CRATE     10      /* custom hazard-crate pickup: +3 real drop-mines (s_94 +0x28)         */

/* how many items lie around at once: all sixteen powerup places of the track (the original uses twelve, race___5209ch.c) */
#define DR3_ARENA_PICKUPS_MAX   16

/* the AI, tuning (frames of the race: 70 a second) */
#define DR3_ARENA_PLAN_EVERY    2       /* a car looks for a free heading every couple frames, in between it holds it */
#define DR3_ARENA_RETARGET      18      /* ... and re-evaluates prey / items ~4 times a second                     */
#define DR3_ARENA_LOOK          108     /* how far ahead a car looks for a wall (px)                               */
#define DR3_ARENA_PASSABLE      64      /* a heading with less room than this is a wall                            */
#define DR3_ARENA_GUN_RANGE     130     /* the machine gun's hit scan reaches 26 * 5 px (race___4ff50h.c)          */
#define DR3_ARENA_STUCK_FRAMES  36      /* ... checked every this many frames ...                                  */
#define DR3_ARENA_STUCK_PX      6       /* ... a car that has moved less than this is stuck                        */
#define DR3_ARENA_ITEM_GIVE_UP  260     /* frames after which a car gives up on an item it cannot get (and leaves it alone for DR3_ARENA_ITEM_BAN) */
#define DR3_ARENA_ITEM_BAN      700
#define DR3_ARENA_REVERSE       28      /* ... and it backs out for this many frames                               */
#define DR3_ARENA_SPECTATE_S    3       /* seconds the player must stay wrecked (0 HP) before Arena triggers Race Over */
#define DR3_ARENA_MINE_COOLDOWN 95      /* frames between two mines of a car (~1.35 s)                             */
#define DR3_ARENA_AMMO_MAX      102400  /* s_94 +0x30 is full at 0x19000                                           */
#define DR3_ARENA_HP_MAX        102400  /* s_94 +0x18 is full at 0x19000                                           */

/*
 * The closing ring (against hiding in a corner).  A circle round the middle of the arena starts bigger than the
 * arena itself (nobody is hurt), holds for DR3_ARENA_RING_GRACE_S seconds, then shrinks in a straight line to
 * DR3_ARENA_RING_END_R over DR3_ARENA_RING_SHRINK_S seconds.  A car outside it loses DR3_ARENA_RING_DRAIN_PCT
 * percent of a full car's HP a second.  It is painted into the track picture as a red line, a banner tells the player
 * when it starts to close and while his own car is outside, and the AI keeps away from its edge.
 */
#define DR3_ARENA_RING_START_R      620     /* px: more than the distance from the middle to a corner (566)        */
#define DR3_ARENA_RING_END_R        120     /* px: around the centre hub (radius 58) and the inner pickups         */
#define DR3_ARENA_RING_GRACE_S      12
#define DR3_ARENA_RING_SHRINK_S     80
#define DR3_ARENA_RING_DRAIN_PCT    3       /* percent of DR3_ARENA_HP_MAX per second outside                       */
#define DR3_ARENA_RING_THICK        4       /* px of the painted line                                               */
#define DR3_ARENA_RING_SAFE         70      /* the AI stays this far inside the edge while there is room            */

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------- portable (host tested) */

/*
 * How many cars the arena adds to the `num_cars` the event brings (the "extra cars" of the chaos field): the
 * switch says how many extras the player allows (0 = none: the arena is then just the event's cars), the arena
 * never goes past DR3_ARENA_CARS.
 */
int dr3_arena_extras(int num_cars, int switch_extras);

/* where the n-th place of the grid order stands: corner n % 4, slot n / 4, facing the middle of w x h */
void dr3_arena_spawn(int place, int w, int h, int *x, int *y, int *img);

/* the Direction that faces (dx, dy), 0..360 - the inverse of the engine's (-sin D, -cos D) */
float dr3_arena_bearing(float dx, float dy);

/* an angle brought into -180..180 */
float dr3_arena_wrap180(float a);

/* the way to turn to get from `dir` to `want`: +1 = STEER_LEFT, -1 = STEER_RIGHT, 0 = straight on (within dead) */
int dr3_arena_steer(float dir, float want, float dead);

/*
 * The room a car has along a heading: how many pixels it can drive from (x, y) along `dir` before the
 * surface is solid, in steps of 6 px, at most `max` (the mask is the engine's: a callback so the test can
 * hand in a picture of its own).  The same callback type serves the planner below.
 */
typedef int (*dr3_arena_solid_fn)(void *ctx, int x, int y);
int dr3_arena_room(dr3_arena_solid_fn solid, void *ctx, float x, float y, float dir, int max);

/*
 * The planner of the hunt.  `want` is the heading the goal is in; it returns the heading to steer to: the
 * wanted one when there is room for it (DR3_ARENA_PASSABLE px or more), otherwise the best of the headings
 * around it (+-25, 50, 75 ... up to 180 degrees) - the room it has, less a price for every degree it turns
 * away from the goal.  *blocked_ahead is set when the way straight on is shorter than `brake_at` px.
 */
float dr3_arena_plan(dr3_arena_solid_fn solid, void *ctx, float x, float y, float dir, float want,
                     int *blocked_ahead, int brake_at);

/* whether a target at (tx, ty) is in front of the gun of a car at (x, y) facing dir: range and lateral miss in px */
int dr3_arena_in_sights(float x, float y, float dir, float tx, float ty, int range, int lateral);

/* whether the straight line from (x, y) to (tx, ty) has no solid wall in between */
int dr3_arena_los(dr3_arena_solid_fn solid, void *ctx, float x, float y, float tx, float ty);

/* whether something at (tx, ty) is behind a car (within `range` px, +-`cone` degrees round the rear) */
int dr3_arena_behind(float x, float y, float dir, float tx, float ty, int range, float cone);

/* the item the dice give: roll 0..99 -> an item kind (the mix of DR3_ARENA_* above) */
int dr3_arena_item_for_roll(int roll);

/* the radius of the ring `ticks` (70 a second) into the race */
int dr3_arena_ring_radius(int ticks);

/* 0 = the ring waits, 1 = it is closing, 2 = it has closed to its end radius */
int dr3_arena_ring_phase(int ticks);

/* whether (x, y) is outside the ring of radius r round (cx, cy) */
int dr3_arena_ring_outside(float x, float y, int cx, int cy, int r);

/* the HP a car outside the ring loses in one frame (70 a second) */
int dr3_arena_ring_drain(int hp_max);

/* whether the player has stayed trashed (HP <= 0) for DR3_ARENA_SPECTATE_S seconds (0 if alive / revived) */
int dr3_arena_player_trashed_out(int player_hp, int ticks, int out_since_tick);

/* the radius inside which the AI feels safe from the ring edge (leaves room round the centre hub at the end) */
int dr3_arena_ring_safe_r(int r);

/*
 * The banner for the player: 1 ("THE RING / CLOSES" for the first seconds of the shrink), 2 ("GET INSIDE /
 * THE RING", blinking, while his car is outside - drawn in red), 3 ("<RIVAL> / IS MAD AT YOU!" at the start
 * of a regular FFA arena), or 0 when there is none to show.
 */
int dr3_arena_banner(int ticks, int own_outside, const char **line1, const char **line2);

/* Picks one AI car (0..n-1, != player) as the hostile Arena grudge rival from `roll`, or -1 if none. */
int dr3_arena_pick_grudge(int n, int player, int roll);

/* what a place pays (1st, 2nd, 3rd - nothing below) */
int dr3_arena_prize(int place);

/*
 * The standings of an arena: who outlived whom.  hp[i] is the HP a car has at the end (> 0 = it survived),
 * died_at[i] when it was wrecked (any increasing number: the frame, the tick - the later, the longer it
 * lasted; ignored for a survivor).  place[i] comes out as 1..n: survivors first, the one with the most HP
 * ahead (ties: the lower index), then the wrecks, the last one to die first.
 */
void dr3_arena_rank(int n, const int *hp, const int *died_at, int *place);

/* --------------------------------------------------------------------------- the engine side (chaos build) */
#if defined(DR3_CHAOS)

/* a race of the mode starts (race___42824h.c, after the track is loaded): cars to their corners, guns and mines */
void dr3_arena_setup(void);
/* one car's turn instead of race___4b62ch (race_main.c) */
void dr3_arena_ai(int car);
/* once per frame (dr3_dm_tick): who died when */
void dr3_arena_note_deaths(void);
/* the end of the race: the engine's Position of every car becomes the arena's place (race_main.c) */
void dr3_arena_final_places(void);
/* the ammunition/repair/... a new pickup of the arena holds (race___5209ch.c) */
int  dr3_arena_roll_item(int roll);
/* once per frame of the arena: the ring drains, is painted into the track picture, is logged */
void dr3_arena_ring_tick(void);
/* the banner of this frame (dr3_blit.c): 1 and its two lines, or 0 */
int  dr3_arena_banner_now(const char **line1, const char **line2);

#endif

#ifdef __cplusplus
}
#endif

#endif /* DR3_ARENA_H */
