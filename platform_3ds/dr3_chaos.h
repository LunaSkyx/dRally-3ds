/*
 * Part of the dRally 3DS port - https://github.com/urxp/dRally
 * SPDX-License-Identifier: MIT (see LICENSE and THIRD_PARTY.md)
 */
/*
 * dr3_chaos.h - the chaos test build (-DDR3_CHAOS): eight extra cars on the grid.
 *
 * The original engine has room for four cars per race and that limit is baked into the memory layout:
 * the cars of the running race (___1e6ed0h), s_94 (___1de580h), s_54 (___1de7d0h) and the car index
 * tables in bss.c are all sized for four.  This build sizes those arrays with DR3_MAX_CARS instead;
 * without -DDR3_CHAOS that is 4, so every array gets exactly the size it had before and the release
 * build is unchanged.
 *
 * Only the *race* grows.  The front end, the results and the in-race car list draw into fixed rows and
 * buffers, so they keep DR3_HUD_CARS entries.  The extra grid entries come from racers that are not in
 * the race yet (___33010h.c) and share the four start slots the track file provides (race___49bd4h.c).
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
 * In game it is SELECT + RIGHT (platform_3ds/dr3_input.c).  The switch takes effect with the next race
 * - the grid is placed when a race starts, so the cars of the race that is running cannot change - and
 * every race logs the value it started with.
 *
 * The build starts with the extras off (DR3_CHAOS_EXTRA_DEFAULT): with them off the grey lines over the
 * player car are gone, and switching them back on is what tells whether they really are the cause.
 */
#define DR3_CHAOS_EXTRA 8
#define DR3_CHAOS_EXTRA_DEFAULT 0
/*
 * How many cars the per car arrays of the test build have room for (bss.c, race_memory.c).  This stays
 * at the twelve car size whichever way the switch is set, so the memory layout does not move with it.
 */
#define DR3_CHAOS_RACERS 12

#if defined(DR3_CHAOS)
#define DR3_MAX_CARS    DR3_CHAOS_RACERS

/* the extra car switch - implemented in platform_3ds/dr3_log.c, see the comment above */
int  dr3_chaos_extra_cars(void);
void dr3_chaos_extra_cars_set(int cars);
int  dr3_chaos_extra_cars_toggle(void);
void dr3_chaos_extra_cars_reload(void);
/*
 * The palette has four car colour ramps: 10 entries each, starting at index 0x0f (see
 * ___4a064h.c).  A race with more cars must not give the ones behind the fourth their own ramp -
 * that would overwrite the track colours - they keep the first ramp, and their sprites are not
 * shifted either (race___42824h.c).
 */
#define DR3_PALETTE_CAR_SLOT(n)     ((n) < DR3_HUD_CARS)
/*
 * The in-race car list stops where the list ends (its rows start at 0x20*n and the fourth row already
 * ends at row 177 of the 200).  A race with fewer cars keeps drawing exactly as many rows as it has,
 * so a two car duel still shows two - this only cuts down the chaos grid.
 */
#define DR3_HUD_LOOP_LIMIT(cars) (((cars) < DR3_HUD_CARS) ? (cars) : DR3_HUD_CARS)
#else
#define DR3_MAX_CARS    DR3_HUD_CARS
/* the release build keeps the original expression, so the generated code does not change at all */
#define DR3_HUD_LOOP_LIMIT(cars) (cars)
#define DR3_PALETTE_CAR_SLOT(n)  1
#endif

#endif /* DR3_CHAOS_H */
