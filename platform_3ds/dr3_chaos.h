/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_chaos.h - the chaos test build (-DDR3_CHAOS): the whole standings on one grid (twenty cars).
 *
 * The original engine has room for four cars per race and that limit is baked into the memory layout:
 * the cars of the running race (___1e6ed0h), s_94 (___1de580h), s_54 (___1de7d0h) and the car index
 * tables in bss.c are all sized for four.  This build sizes those arrays with DR3_MAX_CARS instead;
 * without -DDR3_CHAOS that is 4, so every array gets exactly the size it had before and the release
 * build is unchanged.
 *
 * Only the *race* grows.  The front end, the results and the in-race car list draw into fixed rows and
 * buffers, so they keep DR3_HUD_CARS entries.  The extra grid entries come from racers that are not in
 * the race yet (___33010h.c) and share the four start slots the track file provides (race___49bd4h.c);
 * platform_3ds/dr3_grid.c walks the road from those slots and moves them onto it (race___42824h.c).
 *
 * Only the files that take part in it include this header - and only they are recompiled when
 * -DDR3_CHAOS is flipped.
 */
#ifndef DR3_CHAOS_H
#define DR3_CHAOS_H

#define DR3_HUD_CARS    4                   /* rows the in-race car list has room for */
/*
 * How many cars the test build *may* add on top of the race's own (race_main.c, ___33010h.c).  That is
 * the maximum; how many are actually raced is the switch dr3_chaos_extra_cars() - a variable, so the
 * extra cars can be switched on and off while the game runs (implementation: platform_3ds/dr3_log.c):
 *
 *     dr3_chaos_extra_cars_set(n)     n = 0..DR3_CHAOS_EXTRA, the cars the next race will have
 *     dr3_chaos_extra_cars_toggle()   flips between 0 and DR3_CHAOS_EXTRA, returns the new value
 *     dr3_chaos_extra_cars_reload()   reads sdmc:/chaos_extras.txt (one number), done per race setup
 *
 * In game it is L + R + X (platform_3ds/dr3_input.c): in the front end those three are unbound, in a
 * race they are brake, gas and mine, and - unlike SELECT, which opens the software keyboard - the combo
 * cannot be mistyped.  The switch takes effect with the next race - the grid is placed when a race
 * starts, so the cars of the race that is running cannot change - and every race logs the value it
 * started with.
 *
 * The chaos build starts with DR3_CHAOS_EXTRA_DEFAULT extra cars - as many as the roster can fill, so
 * every driver of the standings is in the race (the race's own four plus the rest of the twenty).  How
 * many it really is comes from sdmc:/chaos_extras.txt at every race setup, so the number can be changed
 * at the SD card (or with L + R + X) without a rebuild: 0 gives the plain four car race back, which is
 * how a fault can be told apart from the bigger field.
 *
 * The release build has no extra cars and keeps the values it always had: the switch itself
 * (platform_3ds/dr3_log.c) is compiled in every build, and different numbers there would show up as
 * changed bytes in the release .3dsx without anything reading them.
 */
#if defined(DR3_CHAOS)
#define DR3_CHAOS_EXTRA 20
#define DR3_CHAOS_EXTRA_DEFAULT 18
#else
#define DR3_CHAOS_EXTRA 8
#define DR3_CHAOS_EXTRA_DEFAULT 1
#endif
/*
 * How many cars the per car arrays of the test build have room for (bss.c, race_memory.c).  The roster
 * is `racer_t racers[20]` (drally_structs_fixed.h) and a race is at most the four it brings plus the
 * rest of the roster, so twenty is exactly "every driver of the standings in one race".  The arrays
 * keep that size whichever way the switch is set, so the memory layout does not move with it.
 */
#define DR3_CHAOS_RACERS 20

#if defined(DR3_CHAOS)
/* 21 slots: supports normal 20-car chaos fields plus Boss Phase 1 (1 player vs 20 Vagabonds). */
#define DR3_MAX_CARS    21
/* 128 active mines/explosions: 20 cars + street/arena mine crates easily exceed the vanilla 32-slot pool. */
#define DR3_MAX_MINES   128

/*
 * The standings order of the race.  ___33010h.c fills both when the race is set up (that is the function
 * that knows which racer every car is), and the grid uses them so the field starts the way the standings
 * read: dr3_chaos_car_rank[n] is the rank of the racer in car n (1 = leader, 0 = no standings entry yet)
 * and dr3_chaos_car_pos[n] is the place in the grid that follows from it, 0 being the front of the grid.
 * The four start slots of the track file go to the four cars with the smallest place (race___49bd4h.c),
 * the road walk hands its spots out in the same order (race___42824h.c) and the position a car starts
 * with is that place as well (race___4a25ch.c).
 */
/*
 * The pixel dump of a race frame (race_main.c) and the palette dump (race___4a25ch.c) write thousands of lines
 * per frame - far too much for a log a player reads on the SD card.  They have their own switch now, a file
 * that is created by hand (chaos_dump.txt / sdmc:/chaos_dump.txt), instead of following the log switch.  The
 * answer is read once per run (the game presents frames sixty times a second and it cannot change underway).
 */
int dr3_chaos_dump_on(void);

/* what is *set* (RANDOM means the wheel rolls) and what the wheel rolled for the next race (dr3_log.c) */
int dr3_chaos_mode_setting(void);
void dr3_chaos_mode_commit(int mode);
void dr3_chaos_mode_clear_roll(void);

/* the first race of a savegame is an ordinary one: the wheel is armed once a race has been set up */
int dr3_chaos_raced(void);
void dr3_chaos_raced_set(void);
const char * dr3_chaos_raced_path(void);

/* the rival of a duel: any driver within +-DR3_CHAOS_DUEL_RANK_WINDOW ranks of the player, drawn at random */
int dr3_chaos_duel_pick(int player_seat);

/* whether a rival named outside is still one of those drivers (the race setup asks before it keeps him) */
int dr3_chaos_duel_rival_ok(int player_seat, int rival);

extern int dr3_chaos_car_rank[DR3_MAX_CARS];
extern int dr3_chaos_car_pos[DR3_MAX_CARS];

/*
 * The racer entry (an index into s_6c) of the car in car n - the third part of the same mapping.  The grid
 * setup in ___33010h.c is the place that knows which racer every car is (the four start slots of the event
 * and the extras it adds), and the chaos points at the end of a race need it to credit every place to the
 * driver who earned it (drally.h: dr3_chaos_points).
 */
extern int dr3_chaos_car_seat[DR3_MAX_CARS];

/*
 * The extra car ladder.  One extra car is already enough to draw the grey lines over the player's car and
 * to break the collisions, so the extra cars are taken apart piece by piece: the ladder says how much of
 * an extra car is drawn, and L + R + X steps through it while the game runs (the log says where it is).
 *
 *    0  invisible    the car is in the race - it drives, it counts for position, it can collide - but
 *                    nothing of it is drawn: no sprite, no lights, no weapon or tyre effects
 *    1  sprite       + the car's sprite (and the wreck sprite when it is destroyed)
 *    2  lights       + its headlight beam
 *    3  everything   + its weapon and tyre effects - the state the faults are known in
 *
 * If the lines survive step 0 they are not drawn by the extra car at all, and the next thing to take apart
 * is the other half: its physics, its collision handling and its place in the per car loops.
 */
#define DR3_EXTRA_STAGE_INVISIBLE 0
#define DR3_EXTRA_STAGE_SPRITE    1
#define DR3_EXTRA_STAGE_LIGHTS    2
#define DR3_EXTRA_STAGE_ALL       3

/*
 * The drawing passes of a race frame, in the order they are called.  L + R + X walks through them and
 * switches ONE of them off at a time (the frames before/after it are drawn as always), so the pass that
 * draws the grey lines over the car is found by looking at the screen while pressing: the pass standing in
 * the log when the lines disappear is the one that draws them.
 *
 * Index 0 means "all passes drawn" - that is where it starts and where it ends after the last one.
 */
#define DR3_PASS_NONE           0
/* first the car pass (4) taken apart - the lines come from one of the first three */
#define DR3_PASS_PLAYER_SPRITE  1
#define DR3_PASS_PLAYER_BEAM    2
#define DR3_PASS_OTHER_BEAMS    3
#define DR3_PASS_CARS           4
#define DR3_PASS_OBJECTS        5
#define DR3_PASS_TRACK_SHADOW   6
#define DR3_PASS_TYRE_DUST      7
#define DR3_PASS_SPECTATORS     8
#define DR3_PASS_EXPLOSIONS     9
#define DR3_PASS_GUN_FIRE      10
#define DR3_PASS_ROCKET        11
#define DR3_PASS_SHOTS         12
#define DR3_PASS_CASH_TEXT     13
#define DR3_PASS_BANNER        14
#define DR3_PASS_HUD           15
#define DR3_PASS_COUNT         15

int dr3_chaos_skip_pass(int which);        /* 1 while that pass is the one switched off */
int dr3_chaos_skip_cycle(void);            /* L + R + X: the next pass, returns its index */

int dr3_chaos_extra_stage(void);
int dr3_chaos_extra_stage_cycle(void);
int dr3_chaos_extra_draw(void);                 /* may the extra cars draw their sprite?   */
int dr3_chaos_extra_lights(void);               /*   and their headlight beam?              */
int dr3_chaos_extra_effects(void);              /*   and their weapon / tyre effects?       */
int dr3_chaos_extra_pass_limit(int cars);       /* how many cars an effect pass may handle  */

/* the extra car switch - implemented in platform_3ds/dr3_log.c, see the comment above */
int  dr3_chaos_extra_cars(void);
int  dr3_chaos_extra_cars_override(void);
void dr3_chaos_extra_cars_set(int cars);
int  dr3_chaos_extra_cars_toggle(void);
void dr3_chaos_extra_cars_reload(void);

/*
 * The race mode switch (drally.h: DR3_CHAOS_MODE_...).  Same shape as the extra car switch above - one
 * number, a file (chaos_mode.txt / sdmc:/chaos_mode.txt) and a key combo - and read at every race setup.
 * The rival of a duel is a seat of s_6c: the setup in ___33010h.c draws one of the drivers within
 * +-DR3_CHAOS_DUEL_RANK_WINDOW ranks of the player (dr3_chaos_duel_pick), leaves it here and the payout of
 * the race reads it back.  A rival the sign up screen named is kept while he still is one of them
 * (dr3_chaos_duel_rival_ok).
 */
int  dr3_chaos_mode(void);
void dr3_chaos_mode_set(int mode);
int  dr3_chaos_mode_toggle(void);
void dr3_chaos_mode_reload(void);
const char * dr3_chaos_mode_name(int mode);

/*
 * The arena (platform_3ds/dr3_arena.h) and "the fights to the last car standing" - Destroy 'em all and the arena
 * share the rules that have no finish line, no lap duty and a pickup table of twelve places.
 */
#define dr3_chaos_arena()   (dr3_chaos_mode() == DR3_CHAOS_MODE_ARENA)
#define dr3_chaos_battle()  ((dr3_chaos_mode() == DR3_CHAOS_MODE_DM) || (dr3_chaos_mode() == DR3_CHAOS_MODE_ARENA))

int  dr3_chaos_duel_rival(void);
void dr3_chaos_duel_rival_set(int seat);

/*
 * Random Adversary guest invader in Chaos mode (Standard, DM, Arena; never Duel):
 *   - sdmc:/chaos_boss.txt (or chaos_boss.txt on PC host): 0 = off, 1..100 = percent chance per race,
 *     missing or negative = DR3_CHAOS_BOSS_CHANCE_DEFAULT (25%).
 *   - dr3_chaos_boss_car(): car index (0..DR3_MAX_CARS-1) of the Adversary in the current race, or -1.
 */
int  dr3_chaos_boss_chance(void);
int  dr3_chaos_boss_override(void);
void dr3_chaos_boss_chance_set(int pct);
void dr3_chaos_boss_reload(void);
int  dr3_chaos_boss_car(void);
void dr3_chaos_boss_car_set(int car);

/*
 * Whether the race is over.  race_main.c sets `___243d14h` to 1 when its race over sequence starts (to -1
 * while a race is being aborted) and the next race sets it back to 0 (race___49a34h.c) - so anything but 0
 * means "the RACE OVER screen and its fade to black are coming".  The port draws its own overlays over the
 * race screen (the health lines over the cars, the sudden death banner) and they have to be gone by then
 * (dr3_blit.c).  Implemented in platform_3ds/dr3_log.c.
 */
int dr3_chaos_race_over(void);

/*
 * The random selector (`DR3_CHAOS_MODE_RANDOM`, drally.h): it draws a race the player may sign up for and
 * one of the three rule sets.  Implemented in ___3266ch.c, where the sign up screen has both the selection
 * (D(___185a50h)) and the checks (the race is full, the player's car is not good enough).  Returns the race
 * (0..2), writes the mode into *mode (STANDARD, DUEL or DM) - the harness' "spin <n>" runs it over and over
 * to show the distribution (platform_3ds/dr3_host.c).
 */
int  dr3_chaos_spin_pick(int *mode);
void dr3_chaos_spin_report(int n);

/*
 * What every car of the race scored (___33010h.c, filled where the points are handed out) and the HP the
 * cars of the running race were seen with (race_main.c, the health line over them).  Both hold DR3_MAX_CARS
 * entries, both are reset when a race is set up (dr3_chaos_race_reset) - the results screens print the
 * "+N" from the first one instead of working the value out again, because a duel pays Elo, which the place
 * curve does not know.
 */
extern int dr3_chaos_car_gain[DR3_MAX_CARS];
extern int dr3_chaos_hp_max[DR3_MAX_CARS];
void dr3_chaos_race_reset(void);
/*
 * The colours of the cars.  The palette has room for four colour ramps of ten entries (0x0f..0x36, see
 * ___4a064h.c) and that is what a four car race uses - one ramp per car.  Twenty cars cannot have ten
 * shades each: that would need 200 of the 256 palette entries, and the track plus its textures already
 * use about 180..215 of them (measured with logs/palette_probe.c on the real TR*.BPA).
 *
 * So the cars keep the palette's own ramp - all ten shades, exactly as before - and the 3DS port
 * re-colours their colour pixels with the colour of each car's driver while it puts the frame on the
 * screen (platform_3ds/dr3_blit.c, dr3_carcolour_*).
 *
 * What that needs is *not* a position worked out from the engine's BACKBUFFER to the presented screen
 * (getting that right took two tries and was wrong both times) but an exact one: the engine marks the
 * pixels a car sprite wrote right in the blit that writes them (helper01, race___4f300h.c).
 *
 *      dr3_chaos_car_mask     one byte per BACKBUFFER pixel (0x200 x 200): 0 = no car,
 *                             otherwise the car index + 1.  It has the shape of the buffer the car
 *                             pass draws into, so the mark is just the write pointer's offset.
 *      dr3_chaos_car_screen   the same mask in the shape of the screen the port presents (320 x 200).
 *                             race_main.c copies it next to the image, with the very same row and
 *                             offset it copies the image with - that is what makes the two fit by
 *                             construction, whatever the offsets are.
 *      dr3_chaos_car_col      the r/g/b of every car of the race (s_54 +0x34/+0x38/+0x3c, the fields
 *                             ___33010h.c fills from carcol.pal and the bottom screen draws from).
 *
 * The port then walks the presented mask, and for every car pixel whose value is in the palette's
 * ramp it writes the matching shade of that car's colour.  Nothing else on the screen uses the ramp
 * entries (the track and its textures avoid 0x0f..0x18), so only the cars change.
 */
#if defined(__3DS__) && defined(DR3_USE_GFX) && !defined(DR3_HOST)
#define DR3_CAR_MASK_W      400      /* native 400x240 widescreen race screen          */
#define DR3_CAR_MASK_H      240
#else
#define DR3_CAR_MASK_W      320      /* the screen the port presents                   */
#define DR3_CAR_MASK_H      200
#endif
#define DR3_CAR_MASK_PITCH  0x200    /* BACKBUFFER's row length, for the engine's mask */

extern unsigned char dr3_chaos_car_mask[DR3_CAR_MASK_PITCH * DR3_CAR_MASK_H];
extern unsigned char dr3_chaos_car_screen[DR3_CAR_MASK_W * DR3_CAR_MASK_H];
extern unsigned char dr3_chaos_car_col[DR3_MAX_CARS][3];
/* the car the sprite blit is drawing - the engine sets it around its helper01() calls, -1 = not a car */
extern int           dr3_chaos_car_now;
/*
 * The in-race car list stops where the list ends (its rows start at 0x20*n and the fourth row already
 * ends at row 177 of the 200).  A race with fewer cars keeps drawing exactly as many rows as it has,
 * so a two car duel still shows two - this only cuts down the chaos grid.
 */
#define DR3_HUD_LOOP_LIMIT(cars) (((cars) < DR3_HUD_CARS) ? (cars) : DR3_HUD_CARS)

/*
 * The health line over the cars (platform_3ds/dr3_blit.c, dr3_carcolour_tint_cars).  A small line above
 * every car of a race that has taken damage - full length and green while the car is nearly whole,
 * shrinking and turning over yellow to red as it loses HP, scaled to the HP the car had when the race
 * started (s_94[n].__18 over dr3_chaos_hp_max[n], which the harness' own "hp <car> <value>" lowers before
 * the flag).
 *
 * The line only appears where it says something: a car at (or above) DR3_CHAOS_HP_LINE_BELOW percent of the
 * HP it started with is whole - a line over it would be the full length bar of a car nobody has touched -
 * and a wreck (0 HP) is a wreck.  Both get no line, and neither does the *player's own car*: the bar is there
 * to read the *other* cars (which of them is nearly done), and his own damage is what the game's own HUD
 * shows him (the DAMAGE panel, s_54[n].__10 from race___400ech.c).  The decision is
 * dr3_carcolour_hp_line_shown(), and the threshold sits next to it in dr3_blit.h (that function is portable,
 * the host tests build it without DR3_CHAOS).
 *
 * The driver of a chaos race needs it: the in-race car list has room for DR3_HUD_CARS rows, so of twenty
 * cars only the first four show a damage bar of their own.  The line is drawn in the port's own image (the
 * same tint pass that colours the cars, so the 3DS and the PC get the very same line) and needs no entry
 * of the game's palette - it is a plain RGB colour in the target format.
 */
#define DR3_CHAOS_HP_LINE_MAX       12          /* px at full health                     */
#define DR3_CHAOS_HP_LINE_H         2           /* px tall                               */
#define DR3_CHAOS_HP_LINE_GAP       3           /* px between the car and its line       */
#else
#define DR3_MAX_CARS    DR3_HUD_CARS
#define DR3_MAX_MINES   32
/* the release build keeps the original expression, so the generated code does not change at all */
#define DR3_HUD_LOOP_LIMIT(cars) (cars)
#define DR3_PALETTE_CAR_SLOT(n)  1
#endif

#endif /* DR3_CHAOS_H */
